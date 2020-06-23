#include "RecursiveSampler/RecursiveInstrumentor/RecursiveInstrumentor.h"
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <Common/Constant.h>
#include "Common/Helper.h"

using namespace llvm;
using namespace std;

static RegisterPass<RecursiveInstrumentor> X(
        "recursive-instrument",
        "only instrument hook in interested recursive function call",
        false, false);

static cl::opt<std::string> strFuncName(
        "strFunc",
        cl::desc("The name of function to instrumention."), cl::Optional,
        cl::value_desc("strFuncName"));

static cl::opt<bool> bElseIf("bElseIf", cl::desc("use if-elseif-else instead of if-else"), cl::Optional,
                             cl::value_desc("bElseIf"), cl::init(false));

char RecursiveInstrumentor::ID = 0;

void RecursiveInstrumentor::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<ScalarEvolutionWrapperPass>();
}

RecursiveInstrumentor::RecursiveInstrumentor() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeScalarEvolutionWrapperPassPass(Registry);
}

void RecursiveInstrumentor::SetupTypes() {

    this->VoidType = Type::getVoidTy(pModule->getContext());
    this->LongType = IntegerType::get(pModule->getContext(), 64);
    this->IntType = IntegerType::get(pModule->getContext(), 32);
    this->CharType = IntegerType::get(pModule->getContext(), 8);
    this->CharStarType = PointerType::get(this->CharType, 0);
    this->VoidPointerType = PointerType::get(IntegerType::get(pModule->getContext(), 8), 0);

}

void RecursiveInstrumentor::SetupStructs() {
    vector<Type *> struct_fields;

    assert(pModule->getTypeByName("struct.stMemRecord") == nullptr);
    this->struct_stMemRecord = StructType::create(pModule->getContext(), "struct.stMemRecord");
    struct_fields.clear();
    struct_fields.push_back(this->LongType);  // address
    struct_fields.push_back(this->IntType);   // length
    // 0: end; 1: delimiter; 2: load; 3: store; 4: loop begin; 5: loop end
    struct_fields.push_back(this->IntType);   // flag
    if (this->struct_stMemRecord->isOpaque()) {
        this->struct_stMemRecord->setBody(struct_fields, false);
    }
}

void RecursiveInstrumentor::SetupConstants() {

    // long: 0, 16
    this->ConstantLong0 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("0"), 10));
    this->ConstantLong16 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("16"), 10));

    // int: -1, 0, 1, 2, 3, 4, 5, 6
    this->ConstantIntN1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("-1"), 10));
    this->ConstantInt0 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("0"), 10));
    this->ConstantInt1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("1"), 10));
    this->ConstantInt2 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("2"), 10));
    this->ConstantInt3 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("3"), 10));
    this->ConstantInt4 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("4"), 10));
    this->ConstantInt5 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("5"), 10));

    // char*: NULL
    this->ConstantNULL = ConstantPointerNull::get(this->CharStarType);

}

void RecursiveInstrumentor::SetupGlobals() {

    // int numGlobalCounter = 0;
    // TODO: CommonLinkage or ExternalLinkage
    assert(pModule->getGlobalVariable("numGlobalCounter") == nullptr);
    this->numGlobalCounter = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::ExternalLinkage, nullptr,
                                                "numGlobalCounter");
    this->numGlobalCounter->setAlignment(4);
    this->numGlobalCounter->setInitializer(this->ConstantInt0);

    // int SAMPLE_RATE = 0;
    assert(pModule->getGlobalVariable("SAMPLE_RATE") == nullptr);
    this->SAMPLE_RATE = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::CommonLinkage, nullptr,
                                           "SAMPLE_RATE");
    this->SAMPLE_RATE->setAlignment(4);
    this->SAMPLE_RATE->setInitializer(this->ConstantInt0);

    // char *pcBuffer_CPI = nullptr;
    assert(pModule->getGlobalVariable("pcBuffer_CPI") == nullptr);
    this->pcBuffer_CPI = new GlobalVariable(*pModule, this->CharStarType, false, GlobalValue::ExternalLinkage, nullptr,
                                            "pcBuffer_CPI");
    this->pcBuffer_CPI->setAlignment(8);
    this->pcBuffer_CPI->setInitializer(this->ConstantNULL);

    // long iBufferIndex_CPI = 0;
    assert(pModule->getGlobalVariable("iBufferIndex_CPI") == nullptr);
    this->iBufferIndex_CPI = new GlobalVariable(*pModule, this->LongType, false, GlobalValue::ExternalLinkage, nullptr,
                                                "iBufferIndex_CPI");
    this->iBufferIndex_CPI->setAlignment(8);
    this->iBufferIndex_CPI->setInitializer(this->ConstantLong0);

    // struct_stLogRecord Record_CPI
    assert(pModule->getGlobalVariable("Record_CPI") == nullptr);
    this->Record_CPI = new GlobalVariable(*pModule, this->struct_stMemRecord, false, GlobalValue::ExternalLinkage,
                                          nullptr,
                                          "Record_CPI");
    this->Record_CPI->setAlignment(16);
    ConstantAggregateZero *const_struct = ConstantAggregateZero::get(this->struct_stMemRecord);
    this->Record_CPI->setInitializer(const_struct);

    // const char *SAMPLE_RATE_ptr = "SAMPLE_RATE";
    ArrayType *ArrayTy12 = ArrayType::get(this->CharType, 12);
    GlobalVariable *pArrayStr = new GlobalVariable(*pModule, ArrayTy12, true, GlobalValue::PrivateLinkage, nullptr, "");
    pArrayStr->setAlignment(1);
    Constant *ConstArray = ConstantDataArray::getString(pModule->getContext(), "SAMPLE_RATE", true);
    vector<Constant *> vecIndex;
    vecIndex.push_back(this->ConstantInt0);
    vecIndex.push_back(this->ConstantInt0);
    this->SAMPLE_RATE_ptr = ConstantExpr::getGetElementPtr(ArrayTy12, pArrayStr, vecIndex);
    pArrayStr->setInitializer(ConstArray);

    // long numGlobalCost = 0;
    assert(pModule->getGlobalVariable("numGlobalCost") == nullptr);
    this->numGlobalCost = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::ExternalLinkage, nullptr,
                                             "numGlobalCost");
    this->numGlobalCost->setAlignment(4);
    this->numGlobalCost->setInitializer(this->ConstantInt0);
}

void RecursiveInstrumentor::SetupFunctions() {

    vector<Type *> ArgTypes;

    // getenv
    this->getenv = pModule->getFunction("getenv");
    if (!this->getenv) {
        ArgTypes.push_back(this->CharStarType);
        FunctionType *getenv_FuncTy = FunctionType::get(this->CharStarType, ArgTypes, false);
        this->getenv = Function::Create(getenv_FuncTy, GlobalValue::ExternalLinkage, "getenv", pModule);
        this->getenv->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // atoi
    this->function_atoi = pModule->getFunction("atoi");
    if (!this->function_atoi) {
        ArgTypes.clear();
        ArgTypes.push_back(this->CharStarType);
        FunctionType *atoi_FuncTy = FunctionType::get(this->IntType, ArgTypes, false);
        this->function_atoi = Function::Create(atoi_FuncTy, GlobalValue::ExternalLinkage, "atoi", pModule);
        this->function_atoi->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // geo
    this->geo = this->pModule->getFunction("geo");
    if (!this->geo) {
        ArgTypes.push_back(this->IntType);
        FunctionType *Geo_FuncTy = FunctionType::get(this->IntType, ArgTypes, false);
        this->geo = Function::Create(Geo_FuncTy, GlobalValue::ExternalLinkage, "geo", this->pModule);
        this->geo->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // InitMemHooks
    this->InitMemHooks = this->pModule->getFunction("InitMemHooks");
    if (!this->InitMemHooks) {
        FunctionType *InitHooks_FuncTy = FunctionType::get(this->CharStarType, ArgTypes, false);
        this->InitMemHooks = Function::Create(InitHooks_FuncTy, GlobalValue::ExternalLinkage, "InitMemHooks",
                                              this->pModule);
        this->InitMemHooks->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // FinalizeMemHooks
    this->FinalizeMemHooks = this->pModule->getFunction("FinalizeMemHooks");
    if (!this->FinalizeMemHooks) {
        ArgTypes.push_back(this->LongType);
        FunctionType *FinalizeMemHooks_FuncTy = FunctionType::get(this->VoidType, ArgTypes, false);
        this->FinalizeMemHooks = Function::Create(FinalizeMemHooks_FuncTy, GlobalValue::ExternalLinkage,
                                                  "FinalizeMemHooks", this->pModule);
        this->FinalizeMemHooks->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
}

void RecursiveInstrumentor::SetupInit(Module &M) {
    // all set up operation
    this->pModule = &M;
    SetupTypes();
    SetupStructs();
    SetupConstants();
    SetupGlobals();
    SetupFunctions();
}

void RecursiveInstrumentor::InstrumentMain() {

    AttributeList emptyList;

    Function *pFunctionMain = this->pModule->getFunction("main");

    if (!pFunctionMain) {
        errs() << "Cannot find the main function\n";
        return;
    }

    // Instrument to entry block
    {
        CallInst *pCall;
        StoreInst *pStore;

        BasicBlock &entryBlock = pFunctionMain->getEntryBlock();

        // Instrument before FirstNonPHI of main function.
        Instruction *firstInst = pFunctionMain->getEntryBlock().getFirstNonPHI();

        // Instrument InitMemHooks
        pCall = CallInst::Create(this->InitMemHooks, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptyList);

        // Store to pcBuffer_CPI
        pStore = new StoreInst(pCall, this->pcBuffer_CPI, false, firstInst);
        pStore->setAlignment(8);

        // Instrument getenv
        vector<Value *> vecParam;
        vecParam.push_back(SAMPLE_RATE_ptr);
        pCall = CallInst::Create(this->getenv, vecParam, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        AttributeList func_get_env_PAL;
        {
            SmallVector<AttributeList, 4> Attrs;
            AttributeList PAS;
            {
                AttrBuilder B;
                B.addAttribute(Attribute::NoUnwind);
                PAS = AttributeList::get(pModule->getContext(), ~0U, B);
            }
            Attrs.push_back(PAS);
            func_get_env_PAL = AttributeList::get(pModule->getContext(), Attrs);
        }
        pCall->setAttributes(func_get_env_PAL);

        // Instrument atoi
        pCall = CallInst::Create(this->function_atoi, pCall, "", firstInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        AttributeList func_atoi_PAL;
        {
            SmallVector<AttributeList, 4> Attrs;
            AttributeList PAS;
            {
                AttrBuilder B;
                B.addAttribute(Attribute::NoUnwind);
                B.addAttribute(Attribute::ReadOnly);
                PAS = AttributeList::get(pModule->getContext(), ~0U, B);
            }

            Attrs.push_back(PAS);
            func_atoi_PAL = AttributeList::get(pModule->getContext(), Attrs);
        }
        pCall->setAttributes(func_atoi_PAL);

        // SAMPLE_RATE = atoi(getenv("SAMPLE_RATE"));
        pStore = new StoreInst(pCall, SAMPLE_RATE, false, firstInst);
        pStore->setAlignment(4);

        // add one delimit to the begin
        InlineHookDelimit(firstInst);
    }

    CallInst *pCall;

    for (auto &BB : *pFunctionMain) {
        for (auto &II : BB) {
            Instruction *pInst = &II;
            // Instrument FinalizeMemHooks before return.
            if (auto pRet = dyn_cast<ReturnInst>(pInst)) {

                // Output numGlobalCost before return.
                InlineOutputCost(pRet);

                auto pLoad = new LoadInst(this->iBufferIndex_CPI, "", false, pRet);
                pLoad->setAlignment(8);
                pCall = CallInst::Create(this->FinalizeMemHooks, pLoad, "", pRet);
                pCall->setCallingConv(CallingConv::C);
                pCall->setTailCall(false);
                pCall->setAttributes(emptyList);

            } else if (isa<CallInst>(pInst) || isa<InvokeInst>(pInst)) {
                CallSite cs(pInst);
                Function *pCalled = cs.getCalledFunction();
                if (!pCalled) {
                    continue;
                }

                // Instrument FinalizeMemHooks before calling exit or functions similar to exit.
                // TODO: any other functions similar to exit?
                if (pCalled->getName() == "exit" || pCalled->getName() == "_ZL9mysql_endi") {

                    // Output numGlobalCost before exit.
                    InlineOutputCost(pInst);

                    auto pLoad = new LoadInst(this->iBufferIndex_CPI, "", false, pInst);
                    pLoad->setAlignment(8);
                    pCall = CallInst::Create(this->FinalizeMemHooks, pLoad, "", pInst);
                    pCall->setCallingConv(CallingConv::C);
                    pCall->setTailCall(false);
                    pCall->setAttributes(emptyList);
                }
            }
        }
    }
}

void RecursiveInstrumentor::InlineSetRecord(Value *address, Value *length, Value *flag, Instruction *InsertBefore) {

    LoadInst *pLoadPointer;
    LoadInst *pLoadIndex;
    BinaryOperator *pBinary;
    CastInst *pCastInst;
    PointerType *pPointerType;

    // char *pc = (char *)pcBuffer_CPI[iBufferIndex_CPI];
    pLoadPointer = new LoadInst(this->pcBuffer_CPI, "", false, InsertBefore);
    pLoadPointer->setAlignment(8);
    pLoadIndex = new LoadInst(this->iBufferIndex_CPI, "", false, InsertBefore);
    pLoadIndex->setAlignment(8);
    GetElementPtrInst *getElementPtr = GetElementPtrInst::Create(this->CharType, pLoadPointer, pLoadIndex, "",
                                                                 InsertBefore);
    // struct_stMemRecord *ps = (struct_stMemRecord *)pc;
    pPointerType = PointerType::get(this->struct_stMemRecord, 0);
    pCastInst = new BitCastInst(getElementPtr, pPointerType, "", InsertBefore);

    // ps->address = address;
    vector<Value *> ptr_address_indices;
    ptr_address_indices.push_back(this->ConstantInt0);
    ptr_address_indices.push_back(this->ConstantInt0);
    GetElementPtrInst *pAddress = GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst,
                                                            ptr_address_indices, "", InsertBefore);
    auto pStoreAddress = new StoreInst(address, pAddress, false, InsertBefore);
    pStoreAddress->setAlignment(8);

    // ps->length = length;
    vector<Value *> ptr_length_indices;
    ptr_length_indices.push_back(this->ConstantInt0);
    ptr_length_indices.push_back(this->ConstantInt1);
    GetElementPtrInst *pLength = GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst,
                                                           ptr_length_indices, "", InsertBefore);
    auto pStoreLength = new StoreInst(length, pLength, false, InsertBefore);
    pStoreLength->setAlignment(8);

    // ps->flag = flag;
    vector<Value *> ptr_flag_indices;
    ptr_flag_indices.push_back(this->ConstantInt0);
    ptr_flag_indices.push_back(this->ConstantInt2);
    GetElementPtrInst *pFlag = GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst, ptr_flag_indices,
                                                         "", InsertBefore);
    auto pStoreFlag = new StoreInst(flag, pFlag, false, InsertBefore);
    pStoreFlag->setAlignment(4);

    // iBufferIndex_CPI += 16
    pBinary = BinaryOperator::Create(Instruction::Add, pLoadIndex, this->ConstantLong16, "iBufferIndex += 16",
                                     InsertBefore);
    auto pStoreIndex = new StoreInst(pBinary, this->iBufferIndex_CPI, false, InsertBefore);
    pStoreIndex->setAlignment(8);
}

void RecursiveInstrumentor::InlineHookDelimit(Instruction *InsertBefore) {

    InlineSetRecord(this->ConstantLong0, this->ConstantInt0, this->ConstantInt1, InsertBefore);
}

void RecursiveInstrumentor::InlineHookLoad(Value *addr, Type *type1, Instruction *InsertBefore) {

    auto dl = new DataLayout(this->pModule);
    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);

    InlineSetRecord(int64_address, const_length, this->ConstantInt2, InsertBefore);
}

void RecursiveInstrumentor::InlineHookLoad(Value *addr, ConstantInt *const_length, Instruction *InsertBefore) {

    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);

    InlineSetRecord(int64_address, const_length, this->ConstantInt2, InsertBefore);
}

void RecursiveInstrumentor::InlineHookStore(Value *addr, Type *type1, Instruction *InsertBefore) {

    auto dl = new DataLayout(this->pModule);

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);

    InlineSetRecord(int64_address, const_length, this->ConstantInt3, InsertBefore);
}

// Cost Record Format: (0, numGlobalCost, 0)
void RecursiveInstrumentor::InlineOutputCost(Instruction *InsertBefore) {

    auto pLoad = new LoadInst(this->numGlobalCost, "", false, InsertBefore);
    InlineSetRecord(this->ConstantLong0, pLoad, this->ConstantInt0, InsertBefore);
}

bool RecursiveInstrumentor::runOnModule(Module &M) {

    SetupInit(M);

    Function *pFunction = M.getFunction(strFuncName);
    if (!pFunction) {
        errs() << "Cannot find the input function\n";
        return false;
    }

    set<BasicBlock *> setBlocksInRecurFunc;
    for (auto BB = pFunction->begin(); BB != pFunction->end(); BB++) {
        setBlocksInRecurFunc.insert(&*BB);
    }

    ValueToValueMapTy VMap;

    map<Function *, set<Instruction *> > FuncCallSiteMapping;
    CloneFunctionCalled(setBlocksInRecurFunc, VMap, FuncCallSiteMapping);

    InstrumentMain();

    vector<BasicBlock *> vecAdd;
    CreateIfElseBlock(pFunction, vecAdd, VMap);

    // InstrumentRecursiveFunction(pFunction);

    CloneRecursiveFunction();

    return false;
}

void RecursiveInstrumentor::CloneFunctionCalled(set<BasicBlock *> &setBlocksInLoop, ValueToValueMapTy &VCalleeMap,
                                           map<Function *, set<Instruction *> > &FuncCallSiteMapping) {
    vector<Function *> vecWorkList;
    set<Function *> toDo;

    set<Instruction *> setMonitoredInstInCallee;

    auto itBlockSetBegin = setBlocksInLoop.begin();
    auto itBlockSetEnd = setBlocksInLoop.end();

    for (; itBlockSetBegin != itBlockSetEnd; itBlockSetBegin++) {
        BasicBlock *BB = *itBlockSetBegin;

        if (isa<UnreachableInst>(BB->getTerminator())) {
            continue;
        }

        for (BasicBlock::iterator II = (BB)->begin(); II != (BB)->end(); II++) {
            if (isa<DbgInfoIntrinsic>(II)) {
                continue;
            } else if (isa<InvokeInst>(II) || isa<CallInst>(II)) {
                CallSite cs(&*II);
                Function *pCalled = cs.getCalledFunction();

                if (pCalled == nullptr) {
                    continue;
                }

                if (pCalled->isDeclaration()) {
                    continue;
                }

                FuncCallSiteMapping[pCalled].insert(&*II);

                if (toDo.find(pCalled) == toDo.end()) {
                    toDo.insert(pCalled);
                    vecWorkList.push_back(pCalled);
                }
            }
        }
    }

    while (!vecWorkList.empty()) {
        Function *pCurrent = vecWorkList.back();
        vecWorkList.pop_back();

        for (auto &BB : *pCurrent) {
            auto pBB = &BB;
            if (isa<UnreachableInst>(pBB->getTerminator())) {
                continue;
            }

            for (BasicBlock::iterator II = pBB->begin(); II != pBB->end(); II++) {
                if (isa<DbgInfoIntrinsic>(II)) {
                    continue;
                } else if (isa<InvokeInst>(II) || isa<CallInst>(II)) {
                    CallSite cs(&*II);
                    Function *pCalled = cs.getCalledFunction();

                    if (pCalled != nullptr && !pCalled->isDeclaration()) {
                        FuncCallSiteMapping[pCalled].insert(&*II);

                        if (toDo.find(pCalled) == toDo.end()) {
                            toDo.insert(pCalled);
                            vecWorkList.push_back(pCalled);
                        }
                    }
                }

                MDNode *Node = II->getMetadata("ins_id");

                if (!Node) {
                    continue;
                }

//                assert(Node->getNumOperands() == 1);
//                if (auto *MDV = dyn_cast<ValueAsMetadata>(Node)) {
//                    Value *V = MDV->getValue();
//                    auto CI = dyn_cast<ConstantInt>(V);
//                    assert(CI);
//                    if (this->setInstID.find(CI->getZExtValue()) != this->setInstID.end()) {
//                        if (isa<LoadInst>(II) || isa<CallInst>(II) || isa<InvokeInst>(II) || isa<MemTransferInst>(II)) {
//                            setMonitoredInstInCallee.insert(&*II);
//                        } else {
//                            assert(0);
//                        }
//                    }
//                }

                if (isa<LoadInst>(II) || isa<StoreInst>(II) || isa<CallInst>(II) || isa<InvokeInst>(II) || isa<MemTransferInst>(II)) {
                    setMonitoredInstInCallee.insert(&*II);
                }
            }
        }
    }

    auto itSetFuncBegin = toDo.begin();
    auto itSetFuncEnd = toDo.end();

    for (; itSetFuncBegin != itSetFuncEnd; itSetFuncBegin++) {
        Function *rawFunction = *itSetFuncBegin;
        Function *duplicateFunction = CloneFunction(rawFunction, VCalleeMap, nullptr);
        duplicateFunction->setName(rawFunction->getName() + ".CPI");
        duplicateFunction->setLinkage(GlobalValue::InternalLinkage);
//        rawFunction->getParent()->getFunctionList().push_back(duplicateFunction);

        VCalleeMap[rawFunction] = duplicateFunction;
    }

    itSetFuncBegin = toDo.begin();

    for (; itSetFuncBegin != itSetFuncEnd; itSetFuncBegin++) {
        auto itSetInstBegin = FuncCallSiteMapping[*itSetFuncBegin].begin();
        auto itSetInstEnd = FuncCallSiteMapping[*itSetFuncBegin].end();

        ValueToValueMapTy::iterator FuncIt = VCalleeMap.find(*itSetFuncBegin);
        assert(FuncIt != VCalleeMap.end());

        auto clonedFunction = cast<Function>(FuncIt->second);

        for (; itSetInstBegin != itSetInstEnd; itSetInstBegin++) {
            ValueToValueMapTy::iterator It = VCalleeMap.find(*itSetInstBegin);

            if (It != VCalleeMap.end()) {
                if (auto pCall = dyn_cast<CallInst>(It->second)) {
                    pCall->setCalledFunction(clonedFunction);
                } else if (auto pInvoke = dyn_cast<InvokeInst>(It->second)) {
                    pInvoke->setCalledFunction(clonedFunction);
                }
            }
        }
    }

    auto itMonInstBegin = setMonitoredInstInCallee.begin();
    auto itMonInstEnd = setMonitoredInstInCallee.end();

    errs() << "Size: " << setMonitoredInstInCallee.size() << '\n';

    for (; itMonInstBegin != itMonInstEnd; itMonInstBegin++) {
        ValueToValueMapTy::iterator It = VCalleeMap.find(*itMonInstBegin);
        assert(It != VCalleeMap.end());

        auto pInst = cast<Instruction>(It->second);

//        errs() << "In cloned function: " << *pInst << '\n';

        if (isa<LoadInst>(pInst)) {
            if (dyn_cast<LoadInst>(pInst)) {
                unsigned oprandidx = 0;  // first operand
                NCAddrType addrType;
                if (getNCAddrType(pInst, oprandidx, addrType)) {
                    InlineHookLoad(addrType.pAddr, addrType.pType, pInst);
                }
            }

        } else if (isa<StoreInst>(pInst)) {
            if (dyn_cast<StoreInst>(pInst)) {
                unsigned oprandidx = 1;  // second operand
                NCAddrType addrType;
                if (getNCAddrType(pInst, oprandidx, addrType)) {
                    InlineHookStore(addrType.pAddr, addrType.pType, pInst);
                }
            }

        } else if (MemTransferInst * pMem = dyn_cast<MemTransferInst>(pInst)) {
//            errs() << "MemIntrinsic met\n";
//            pInst->dump();
            InlineHookMem(pMem, pInst);
        } else if (isa<CallInst>(pInst)) {
//            if (CallInst *pCallInst = dyn_cast<CallInst>(pInst)) {
////                errs() << "IOFunction called\n";
//                Function *pFunc = pCallInst->getCalledFunction();
////                errs() << pCallInst->getType();
//                InlineHookIOFunc(pFunc, pInst);
//            }
        } else if (isa<InvokeInst>(pInst)) {
//            if (InvokeInst *pInvokeInst = dyn_cast<InvokeInst>(pInst)) {
////                errs() << "IOFunction invoked\n";
//                Function *pFunc = pInvokeInst->getCalledFunction();
////                errs() << pInvokeInst->getType();
//                InlineHookIOFunc(pFunc, pInst);
//            }
        }
    }
}

void RecursiveInstrumentor::InlineHookMem(MemTransferInst * pMem, Instruction *II) {

    if (MemSetInst *pMemSet = dyn_cast<MemSetInst>(pMem)) {
        Value *pAddr = pMemSet->getDest();
        CastInst *int64_address = new PtrToIntInst(pAddr, this->LongType, "", II);
        Value *pLen = pMemSet->getLength();
        CastInst* int32_len = new TruncInst(pLen, this->IntType, "memset_len_conv", II);
        InlineSetRecord(int64_address, int32_len, this->ConstantInt3, II);

    } else if (MemCpyInst *pMemCpy = dyn_cast<MemCpyInst>(pMem)) {
        Value *pReadAddr = pMemCpy->getSource();
        CastInst *int64_address = new PtrToIntInst(pReadAddr, this->LongType, "", II);
        Value *pLen = pMemCpy->getLength();
        CastInst *int32_len = new TruncInst(pLen, this->IntType, "memcpy_len_conv", II);
        InlineSetRecord(int64_address, int32_len, this->ConstantInt2, II);

        Value *pWriteAddr = pMemCpy->getDest();
        CastInst *int64_address_write = new PtrToIntInst(pWriteAddr, this->LongType, "", II);
        InlineSetRecord(int64_address_write, int32_len, this->ConstantInt3, II);

    } else if (MemMoveInst *pMemMove = dyn_cast<MemMoveInst>(pMem)) {

        Value *pReadAddr = pMemMove->getSource();
        CastInst *int64_address = new PtrToIntInst(pReadAddr, this->LongType, "", II);
        Value *pLen = pMemMove->getLength();
        CastInst* int32_len = new TruncInst(pLen, this->IntType, "memmove_len_conv", II);
        InlineSetRecord(int64_address, int32_len, this->ConstantInt2, II);

        Value *pWriteAddr = pMemMove->getDest();
        CastInst *int64_address_write = new PtrToIntInst(pWriteAddr, this->LongType, "", II);
        InlineSetRecord(int64_address_write, int32_len, this->ConstantInt3, II);
    }
}

void RecursiveInstrumentor::InlineHookIOFunc(Function *F, Instruction *II) {
    std::string funcName = F->getName();
    std::vector<Value *> vecParams;

    if (funcName == "fgetc") {
        // Set addr == 0
        // length of return value is 1 (a char)
        // flag is READ
        InlineSetRecord(ConstantLong0, ConstantInt1, ConstantInt2, II);

    } else if (funcName == "fread") {
        // Set addr == 0
        // length of return value
        // flag is READ
        // Note: 1. to get return value we must insert code AFTER the CallInst
        // 2. Return type is 64 bit, must be cast to 32 bit first
        Instruction *InsertBefore = II->getNextNode();
        CastInst* length = new TruncInst(II, this->IntType, "conv", InsertBefore);
        InlineSetRecord(ConstantLong0, length, ConstantInt2, InsertBefore);
    }

}

void RecursiveInstrumentor::CreateIfElseBlock(Function *pRecursiveFunc, std::vector<BasicBlock *> &vecAdded, ValueToValueMapTy &VMap) {

    ValueToValueMapTy newVMap;
    ValueToValueMapTy rawVMap;
    LoadInst *pLoad1 = NULL;
    ICmpInst *pCmp = NULL;

    // clone new function
    Function *newF = CloneFunction(pRecursiveFunc, newVMap);
    // clone raw function
    Function *rawF = CloneFunction(pRecursiveFunc, rawVMap);

    // the prefix is used to
    string name = newF->getName().str();
    name = CLONE_FUNCTION_PREFIX + name;
    newF->setName(name);

    myNewF = newF;

    errs() << newF->getName() << '\n';

    // delete all current basic blocks;
    pRecursiveFunc->dropAllReferences();

    // create all need block!
    BasicBlock *newEntry = BasicBlock::Create(this->pModule->getContext(),
                                              "newEntry", pRecursiveFunc, 0);

    BasicBlock *label_if_then = BasicBlock::Create(this->pModule->getContext(),
                                                   "if.then", pRecursiveFunc, 0);

    BasicBlock *label_if_else = BasicBlock::Create(this->pModule->getContext(),
                                                   "if.else", pRecursiveFunc, 0);

    // load counter
    pLoad1 = new LoadInst(this->numGlobalCounter, "", false, newEntry);
    pLoad1->setAlignment(4);

    // create if (counter == 0)
    pCmp = new ICmpInst(*newEntry, ICmpInst::ICMP_EQ, pLoad1, this->ConstantInt0, "");
    BranchInst::Create(label_if_then, label_if_else, pCmp, newEntry);

    // collect all args to pass to newF and rawF.
    vector<Value *> callArgs;
    for (Function::arg_iterator k = pRecursiveFunc->arg_begin(), ke = pRecursiveFunc->arg_end(); k != ke; ++k)
        callArgs.push_back(k);

    // call geo random a int num and store to switcher
    pLoad1 = new LoadInst(this->SAMPLE_RATE, "", false, label_if_then);
    pLoad1->setAlignment(4);

    std::vector<Value *> geo_params;
    geo_params.push_back(pLoad1);

    CallInst *callGeoInst = CallInst::Create(this->geo, geo_params, "", label_if_then);
    callGeoInst->setCallingConv(CallingConv::C);
    callGeoInst->setTailCall(true);
    new StoreInst(callGeoInst, this->numGlobalCounter, false, label_if_then);

    // Block if.then: insert call new function in if.then
    CallInst *oneCall = CallInst::Create(newF, callArgs,
                                         (pRecursiveFunc->getReturnType()->isVoidTy()) ? "" : "theCall",
                                         label_if_then);
    oneCall->setTailCall(false);

    // return
    if (pRecursiveFunc->getReturnType()->isVoidTy())
        ReturnInst::Create(this->pModule->getContext(), label_if_then);
    else
        ReturnInst::Create(this->pModule->getContext(), oneCall, label_if_then);

    // Block if.else: insert counter-- and call raw function in if.else
    LoadInst *loadInst_1 = new LoadInst(this->numGlobalCounter, "", false, label_if_else);
    loadInst_1->setAlignment(8);

    BinaryOperator *int32_dec = BinaryOperator::Create(
            Instruction::Add, loadInst_1,
            this->ConstantIntN1, "dec", label_if_else);

    StoreInst *void_35 = new StoreInst(
            int32_dec, this->numGlobalCounter,
            false, label_if_else);
    void_35->setAlignment(4);

    oneCall = CallInst::Create(
            rawF, callArgs,
            (pRecursiveFunc->getReturnType()->isVoidTy()) ? "" : "theCall",
            label_if_else);
    oneCall->setTailCall(true);

    if (pRecursiveFunc->getReturnType()->isVoidTy())
        ReturnInst::Create(this->pModule->getContext(), label_if_else);
    else
        ReturnInst::Create(this->pModule->getContext(), oneCall, label_if_else);

    //remap value used inside new functions
    for (auto &BB : *newF) {
        for (auto &II : BB) {
            this->RemapInstruction(&II, VMap);
        }
    }
}

void RecursiveInstrumentor::CreateIfElseIfBlock(Function *pRecursiveFunc, std::vector<BasicBlock *> &vecAdded) {

    ValueToValueMapTy newVMap;
    ValueToValueMapTy rawVMap;
    LoadInst *pLoad1 = NULL;
    ICmpInst *pCmp = NULL;

    // clone new function
    Function *newF = CloneFunction(pRecursiveFunc, newVMap);
    // clone raw function
    Function *rawF = CloneFunction(pRecursiveFunc, rawVMap);

    // the prefix is used to
    string name = newF->getName().str();
    name = CLONE_FUNCTION_PREFIX + name;
    newF->setName(name);

    myNewF = newF;

    errs() << newF->getName() << '\n';

    // delete all current basic blocks;
    pRecursiveFunc->dropAllReferences();

    // create all need block!
    BasicBlock *newEntry = BasicBlock::Create(this->pModule->getContext(),
                                              "newEntry", pRecursiveFunc, 0);

    BasicBlock *label_if_then = BasicBlock::Create(this->pModule->getContext(),
                                                   "if.then", pRecursiveFunc, 0);

    BasicBlock *label_if_else = BasicBlock::Create(this->pModule->getContext(),
                                                   "if.else", pRecursiveFunc, 0);

    // load counter
    pLoad1 = new LoadInst(this->numGlobalCounter, "", false, newEntry);
    pLoad1->setAlignment(4);

    // create if (counter == 0)
    pCmp = new ICmpInst(*newEntry, ICmpInst::ICMP_EQ, pLoad1, this->ConstantInt0, "");
    BranchInst::Create(label_if_then, label_if_else, pCmp, newEntry);

    // collect all args to pass to newF and rawF.
    vector<Value *> callArgs;
    for (Function::arg_iterator k = pRecursiveFunc->arg_begin(), ke = pRecursiveFunc->arg_end(); k != ke; ++k)
        callArgs.push_back(k);

    // call geo random a int num and store to switcher
    pLoad1 = new LoadInst(this->SAMPLE_RATE, "", false, label_if_then);
    pLoad1->setAlignment(4);

    std::vector<Value *> geo_params;
    geo_params.push_back(pLoad1);

    CallInst *callGeoInst = CallInst::Create(this->geo, geo_params, "", label_if_then);
    callGeoInst->setCallingConv(CallingConv::C);
    callGeoInst->setTailCall(true);
    new StoreInst(callGeoInst, this->numGlobalCounter, false, label_if_then);

    // Block if.then: insert call new function in if.then
    CallInst *oneCall = CallInst::Create(newF, callArgs,
                                         (pRecursiveFunc->getReturnType()->isVoidTy()) ? "" : "theCall",
                                         label_if_then);
    oneCall->setTailCall(false);

    // return
    if (pRecursiveFunc->getReturnType()->isVoidTy())
        ReturnInst::Create(this->pModule->getContext(), label_if_then);
    else
        ReturnInst::Create(this->pModule->getContext(), oneCall, label_if_then);

    // Block if.else: insert counter-- and call raw function in if.else
    LoadInst *loadInst_1 = new LoadInst(this->numGlobalCounter, "", false, label_if_else);
    loadInst_1->setAlignment(8);

    BinaryOperator *int32_dec = BinaryOperator::Create(
            Instruction::Add, loadInst_1,
            this->ConstantIntN1, "dec", label_if_else);

    StoreInst *void_35 = new StoreInst(
            int32_dec, this->numGlobalCounter,
            false, label_if_else);
    void_35->setAlignment(4);

    oneCall = CallInst::Create(
            rawF, callArgs,
            (pRecursiveFunc->getReturnType()->isVoidTy()) ? "" : "theCall",
            label_if_else);
    oneCall->setTailCall(true);

    if (pRecursiveFunc->getReturnType()->isVoidTy())
        ReturnInst::Create(this->pModule->getContext(), label_if_else);
    else
        ReturnInst::Create(this->pModule->getContext(), oneCall, label_if_else);
}

void RecursiveInstrumentor::InstrumentReturn(Function *Func) {

    for (Function::iterator BI = Func->begin(); BI != Func->end(); BI++) {

        BasicBlock *BB = &*BI;

        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {
            Instruction *Inst = &*II;

            if (GetInstructionID(Inst) == -1) {
                continue;
            }

            switch (Inst->getOpcode()) {
                case Instruction::Ret: {
                    InlineOutputCost(Inst);
                    break;
                }
            }
        }
    }
}

void RecursiveInstrumentor::InstrumentNewReturn(Function *Func) {

    for (Function::iterator BI = Func->begin(); BI != Func->end(); BI++) {

        BasicBlock *BB = &*BI;

        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {
            Instruction *Inst = &*II;

//            if (GetInstructionID(Inst) == -1) {
//                continue;
//            }

            switch (Inst->getOpcode()) {
                case Instruction::Ret: {
                    InlineHookDelimit(Inst);
                    break;
                }
            }
        }
    }
}


void RecursiveInstrumentor::InstrumentRmsUpdater(Function *F) {

    if (F->arg_size() > 0) {

        DataLayout *dl = new DataLayout(this->pModule);
        int add_rms = 0;
        for (Function::arg_iterator argIt = F->arg_begin(); argIt != F->arg_end(); argIt++) {
            Argument *argument = argIt;

            Type *argType = argument->getType();
            add_rms += dl->getTypeAllocSize(argType);
        }

        Instruction *II = &*(F->getEntryBlock().getFirstInsertionPt());
        Instruction *pNext = II->getNextNode();

        AllocaInst *tempAlloc = new AllocaInst(this->LongType, 0, "tempLocal", II);
        CastInst *ptr_50 = new BitCastInst(tempAlloc, this->VoidPointerType, "");
        ptr_50->insertAfter(II);

        std::vector<Value *> vecParams;
        ConstantInt *const_rms = ConstantInt::get(
                this->pModule->getContext(),
                APInt(32, StringRef(std::to_string(add_rms)), 10));
        vecParams.push_back(ptr_50);
        vecParams.push_back(const_rms);

        // InlineHookLoad(ptr_50, const_rms, pNext);
        CastInst *int64_address = new PtrToIntInst(ptr_50, this->LongType, "", pNext);
        InlineSetRecord(int64_address, const_rms, this->ConstantInt2, pNext);
    }
}

void RecursiveInstrumentor::CloneRecursiveFunction() {

    if (strFuncName.empty()) {
        errs() << "Please set recursive function name." << "\n";
        exit(0);
    }

    Function *Func = this->pModule->getFunction(strFuncName);

    if (!Func) {
        errs() << "could not find target recursive function." << "\n";
        exit(0);
    }
    // std::string new_name = getClonedFunctionName(this->pModule, strFuncName);
    //Function *NewFunc = this->pModule->getFunction(new_name);

    Function *NewFunc = myNewF;

    if (!NewFunc) {
        errs() << "could not find cloned recursive function." << "\n";
        exit(0);
    }



    InlineNumGlobalCost(Func);
    InstrumentRmsUpdater(NewFunc);
    InlineNumGlobalCost(NewFunc);
    InstrumentNewReturn(NewFunc);
}

void RecursiveInstrumentor::InlineNumGlobalCost(Function *pFunction) {

    Instruction *pFirst = &*(pFunction->getEntryBlock().getFirstInsertionPt());

    auto pLoad = new LoadInst(this->numGlobalCost, "numGlobalCost", pFirst);
    BinaryOperator *pBinary = BinaryOperator::Create(Instruction::Add, pLoad, this->ConstantInt1, "numGlobalCost++",
                                                     pFirst);
    auto pStoreAdd = new StoreInst(pBinary, this->numGlobalCost, false, pFirst);
    pStoreAdd->setAlignment(4);
}

enum NCInstType : unsigned {
    NCOthers,
    NCLoad,
    NCStore,
    NCMemIntrinsic
};

static unsigned parseInst(Instruction *pInst, NCAddrType &addrType) {
    if (isa<LoadInst>(pInst)) {
        if (getNCAddrType(pInst, 0, addrType)) {
            return NCInstType::NCLoad;
        }
    } else if (isa<StoreInst>(pInst)) {
        if (getNCAddrType(pInst, 1, addrType)) {
            return NCInstType::NCStore;
        }
    } else if (isa<MemIntrinsic>(pInst)) {
        return NCInstType::NCMemIntrinsic;
    }
    return NCInstType::NCOthers;
}

bool
RecursiveInstrumentor::SearchToBeInstrumented(Function *pRecursiveFunc, std::vector<Instruction *> &vecToInstrument) {

    for (auto &BB : *pRecursiveFunc) {
        for (auto &II : BB) {
            Instruction *pInst = &II;
            NCAddrType addrType;
            switch (parseInst(pInst, addrType)) {
                case NCInstType::NCLoad: {

                    vecToInstrument.push_back(pInst);
                    break;
                }
                case NCInstType::NCStore: {

                    vecToInstrument.push_back(pInst);
                    break;
                }
                case NCInstType::NCMemIntrinsic: {
                    errs() << "MemIntrinsic met\n";
                    pInst->dump();
                    break;
                }
                default: {
                    break;
                }
            }

        }
    }

    return true;
}

//void RecursiveInstrumentor::InstrumentRecordMemHooks(std::set<Instruction *> vecToInstrumentCloned) {
//
//    for (auto &inst: vecToInstrumentCloned) {
//        Instruction *pInst = inst;
//
//        switch (pInst->getOpcode()) {
//            case Instruction::Load: {
//                NCAddrType addrType;
//                if (getNCAddrType(pInst, 0, addrType)) {
//                    InlineHookLoad(addrType.pAddr, addrType.pType, pInst);
//                } else {
//                    errs() << "Load fails to get AddrType: " << pInst << '\n';
//                }
//                break;
//            }
//            case Instruction::Store: {
//                NCAddrType addrType;
//                if (getNCAddrType(pInst, 1, addrType)) {
//                    InlineHookStore(addrType.pAddr, addrType.pType, pInst);
//                } else {
//                    errs() << "Store fails to get AddrType: " << pInst << '\n';
//                }
//                break;
//            }
//            default: {
//                errs() << "Instrument not Load or Store: " << pInst << '\n';
//                break;
//            }
//        }
//    }
//}

//void RecursiveInstrumentor::InstrumentRecursiveFunction(Function *pRecursiveFunc) {
//
//    std::vector<Instruction *> vecInstToBeInstrumented;
//    SearchToBeInstrumented(pRecursiveFunc, vecInstToBeInstrumented);
//
//    set<BasicBlock *> setBlocksInRecursiveFunc;
//    for (auto &BB : *pRecursiveFunc) {
//        BasicBlock *pBB = &BB;
//        setBlocksInRecursiveFunc.insert(pBB);
//    }
//
//    BasicBlock *pOldFirstBB = &pRecursiveFunc->front();
//
//    ValueToValueMapTy VCalleeMap;
//    map<Function *, set<Instruction *> > FuncCallSiteMapping;
//    // add hooks to function called inside the loop
//    CloneFunctionCalled(setBlocksInRecursiveFunc, VCalleeMap, FuncCallSiteMapping);
//
//    std::vector<BasicBlock *> vecAdded;
//    if (!bElseIf) {
//        CreateIfElseBlock(pRecursiveFunc, vecAdded);
//    } else {
//        CreateIfElseIfBlock(pRecursiveFunc, vecAdded);
//    }
//
//    InlineNumGlobalCost(pOldFirstBB);
//
//    // clone recursive function
//    ValueToValueMapTy VMap;
//    vector<BasicBlock *> vecCloned;
//    CloneRecursiveFunction(pRecursiveFunc, pOldFirstBB, vecAdded, VMap, vecCloned);
//
//    set<Instruction *> setClonedInstToBeInstrumented;
//
//    for (auto &pInst: vecInstToBeInstrumented) {
//        auto pClonedInst = cast<Instruction>(VMap[pInst]);
//        if (pClonedInst) {
//            setClonedInstToBeInstrumented.insert(pClonedInst);
//        } else {
//            errs() << "Not cloned: " << *pInst << '\n';
//        }
//    }
//
//    unsigned idx = 0;
//    for (const auto& pInst: setClonedInstToBeInstrumented) {
//        errs() << idx << ": "<< *pInst << '\n';
//        ++idx;
//    }
//
//    InstrumentRecordMemHooks(setClonedInstToBeInstrumented);
//}

void RecursiveInstrumentor::RemapInstruction(Instruction *I, ValueToValueMapTy &VMap) {
    for (unsigned op = 0, E = I->getNumOperands(); op != E; ++op) {
        Value *Op = I->getOperand(op);
        ValueToValueMapTy::iterator It = VMap.find(Op);
        if (It != VMap.end()) {
            I->setOperand(op, It->second);
        }
    }

    if (auto PN = dyn_cast<PHINode>(I)) {
        for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
            ValueToValueMapTy::iterator It = VMap.find(PN->getIncomingBlock(i));
            if (It != VMap.end())
                PN->setIncomingBlock(i, cast<BasicBlock>(It->second));
        }
    }
}

