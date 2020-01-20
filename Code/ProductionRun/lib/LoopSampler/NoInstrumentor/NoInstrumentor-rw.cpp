#include "LoopSampler/NoInstrumentor/NoInstrumentor.h"

#include <fstream>
#include <memory>

#include <llvm/Analysis/AliasSetTracker.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/ADT/Statistic.h>

#include "LoopSampler/NoInstrumentor/BBProfiling.h"

#include "Common/ArrayLinkedIndentifier.h"
#include "Common/Constant.h"
#include "Common/Helper.h"

using namespace llvm;
using std::vector;
using std::map;
using std::set;
using std::unique_ptr;

#define DEBUG_TYPE "NoInstrumentor"

STATISTIC(no_opt_cost, "No Opt Cost");
STATISTIC(opt_cost, "Opt Cost");

static RegisterPass<NoInstrumentor> X("opt-loop-instrument",
                                           "instrument a loop accessing an array element in each iteration",
                                           false, false);

static cl::opt<unsigned> uSrcLine("noLine",
                                  cl::desc("Source Code Line Number for the Loop"), cl::Optional,
                                  cl::value_desc("uSrcCodeLine"));

static cl::opt<std::string> strFileName("strFile",
                                        cl::desc("File Name for the Loop"), cl::Optional,
                                        cl::value_desc("strFileName"));

static cl::opt<std::string> strFuncName("strFunc",
                                        cl::desc("Function Name"), cl::Optional,
                                        cl::value_desc("strFuncName"));

char NoInstrumentor::ID = 0;

template<typename T>
static T* getClonedValue(T* Value, ValueToValueMapTy &VMap) {
    auto itMap = VMap.find(Value);
    if (itMap != VMap.end()) {
        T* ClonedValue = cast<T>(itMap->second);
        if (ClonedValue) {
            return ClonedValue;
        } else {
            Value->dump();
            assert(false && "Hoist cloned mapping is NULL");
        }
    } else {
        Value->dump();
        assert(false && "No hoist cloned mapping found");
    }
}

NoInstrumentor::NoInstrumentor() : ModulePass(ID) {

    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeDominatorTreeWrapperPassPass(Registry);
    initializeLoopInfoWrapperPassPass(Registry);
    initializeAAResultsWrapperPassPass(Registry);
}

void NoInstrumentor::getAnalysisUsage(AnalysisUsage &AU) const {

    AU.setPreservesAll();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<AAResultsWrapperPass>();
}

bool NoInstrumentor::runOnModule(Module &M) {

    SetupInit(M);

    Function *pFunction = searchFunctionByName(M, strFileName, strFuncName, uSrcLine);
    if (!pFunction) {
        errs() << "Cannot find the function\n";
        return false;
    }

    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*pFunction).getDomTree();
    LoopInfo &LoopInfo = getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();
    AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>(*pFunction).getAAResults();

    unique_ptr<AliasSetTracker> CurAST = make_unique<AliasSetTracker>(AA);

    Loop *pLoop = searchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);
    if (!pLoop) {
        errs() << "Cannot find the loop\n";
        return false;
    }

    set<BasicBlock *> setBlocksInLoop;
    for (BasicBlock *BB : pLoop->blocks()) {
        setBlocksInLoop.insert(BB);
    }

    vector<Function *> vecWorkList;
    set<Function *> setDirectFuncs;
    std::map<Function *, std::set<Instruction *>> directFuncCallSiteMapping;

    FindDirectCallees(setBlocksInLoop, vecWorkList, setDirectFuncs, directFuncCallSiteMapping);

    // Callee functions
    set<Function *> setCallees;
    ValueToValueMapTy VCalleeMap;
    std::map<Function *, std::set<Instruction *>> funcCallSiteMapping;
    bool ok = CloneRemapCallees(setBlocksInLoop, setCallees, VCalleeMap, funcCallSiteMapping);
    assert(ok);

    InstrumentCallees(setCallees, VCalleeMap, DT, nullptr);

//    set<Function *> setClonedCallees;
//    for (Function *pFunc : setCallees) {
//        auto it = VCalleeMap.find(pFunc);
//        if (it != VCalleeMap.end()) {
//            Function *pClonedFunc = cast<Function>(it->second);
//            if (pClonedFunc) {
//                errs() << "Cloned Func" << pClonedFunc->getName() << "\n";
//                InlineGlobalCostForCallee(pClonedFunc);
//            }
//        }
//    }

//    MonitoredRWInsts inplaceMI;
//    getMonitoredRWInsts(setBlocksInLoop, inplaceMI);
//    InstrumentMonitoredInsts(inplaceMI);
//sfd
    // Search monitored
    MonitoredRWInsts MI;
    MonitoredRWInsts hoistMI;
//    MonitoredRWInsts hoistSinkMI;

    {
        errs() << "In Loop:\n";
        getMonitoredRWInsts(setBlocksInLoop, MI);
//        errs() << "+" << MI.size() << "\n";
//
//        vector<set<Instruction *>> vecAI;
//        getAliasInstInfo(std::move(CurAST), MI, vecAI);
//        removeByDomInfo(DT, vecAI, MI);
////        std::set<LoadInst *> toRemove;
//        std::set<StoreInst *> toRemoveStore;
//        for (auto &kv : MI.mapLoadID) {
//            for (auto &kv2 : MI.mapStoreID) {
//                if (DT.dominates(kv.first, kv2.first)) {
//                    toRemoveStore.insert(kv2.first);
//                }
//            }
////            for (auto &kv2 : MI.mapLoadID) {
////                if (kv.first != kv2.first && DT.dominates(kv.first, kv2.first)) {
////                    toRemove.insert(kv2.first);
////                }
////            }
//        }
////        for (auto &LI : toRemove) {
////            if (MI.mapLoadID.find(LI) != MI.mapLoadID.end()) {
////                MI.mapLoadID.erase(LI);
////            }
////        }
//
//        for (auto &SI : toRemoveStore) {
//            if (MI.mapStoreID.find(SI) != MI.mapStoreID.end()) {
//                MI.mapStoreID.erase(SI);
//            }
//        }

        errs() << "-" << MI.size() << "\n";

//        InstrumentMonitoredInsts(MI);
        getInstsToBeHoisted(pLoop, AA, inplaceMI, hoistMI);

//        DEBUG(dbgs() << "Inplace:\n");
//        DEBUG(inplaceMI.dump());
//        DEBUG(dbgs() << "Hoist:\n");
//        DEBUG(hoistMI.dump());
//        DEBUG(dbgs() << "HoistSink:\n");
//        DEBUG(hoistSinkMI.dump());
    }

//    std::vector<BasicBlock *> vecAdded;
//    CreateIfElseBlock(pLoop, vecAdded);

//    ValueToValueMapTy originClonedMapping;
//
//    for (Function *pFunc : setDirectFuncs) {
//        Function *clonedFunc = dyn_cast<Function>(VCalleeMap[pFunc]);
//        if (clonedFunc) {
//            errs() << clonedFunc->getName() << "\n";
//            originClonedMapping[pFunc] = clonedFunc;
//        }
//    }

//    InlineGlobalCostForLoop(setBlocksInLoop);
//
////    Instruction *delimitLoc = &*pClonedBody->getFirstInsertionPt();
//    Instruction *delimitLoc = &*pLoop->getLoopPreheader()->getFirstInsertionPt();
//    InlineHookDelimit(delimitLoc);

    InstrumentMain("main");
//    errs() << "no_opt_cost:" << no_opt_cost << "\n";
//    errs() << "opt_cost:" << opt_cost << "\n";
    return true;
}

//void NoInstrumentor::InlineNumGlobalCost(Loop *pLoop, ValueToValueMapTy &VMap) {
//
//    BasicBlock *pHeader = pLoop->getHeader();
//    Instruction *pHeaderTerm = pHeader->getTerminator();
//
//    pHeaderTerm = getClonedValue(pHeaderTerm, VMap);
//
//    auto pLoad = new LoadInst(this->numGlobalCost, "numGlobalCost", pHeaderTerm);
//    BinaryOperator *pBinary = BinaryOperator::Create(Instruction::Add, pLoad, this->ConstantLong1, "numGlobalCost++",
//                                                     pHeaderTerm);
//    auto pStoreAdd = new StoreInst(pBinary, this->numGlobalCost, false, pHeaderTerm);
//    pStoreAdd->setAlignment(8);
//}


void NoInstrumentor::getMonitoredRWInsts(const std::set<BasicBlock *> &setBB, MonitoredRWInsts &MI) {

    for (BasicBlock *BB : setBB) {
        for (Instruction &II : *BB) {
            Instruction *pInst = &II;
            MI.add(pInst);
        }
    }
}

void NoInstrumentor::getAliasInstInfo(std::unique_ptr<AliasSetTracker> CurAST, const MonitoredRWInsts &MI,
                                           vector<set<Instruction *>> &vecAI) {

    set<Instruction *> setAllLoadStore;
    CurAST->clear();
    // setLoadStore = MI.setLoad U MI.setStore
    for (auto &kv : MI.mapLoadID) {
        LoadInst *pLoad = kv.first;
        setAllLoadStore.insert(pLoad);
        pLoad->dump();
        CurAST->add(pLoad);
    }
    for (auto &kv : MI.mapStoreID) {
        StoreInst *pStore = kv.first;
        setAllLoadStore.insert(pStore);
        pStore->dump();
        CurAST->add(pStore);
    }

    for (AliasSet &AS : *CurAST) {
        if (AS.isMustAlias()) {
            set<Instruction *> setLoadStore;
            for (auto It = AS.begin(); It != AS.end(); ++It) {
                Value *addr = It->getValue();
                uint64_t size = It->getSize();
                for (Instruction *pInst : setAllLoadStore) {
                    MemoryLocation ML = MemoryLocation::get(pInst);
                    if (ML.Ptr == addr && ML.Size == size) {
                        setLoadStore.insert(pInst);
                        setAllLoadStore.erase(pInst);
                    }
                }
            }
            vecAI.push_back(setLoadStore);
        }
    }
}

void NoInstrumentor::removeByDomInfo(DominatorTree &DT, vector<set<Instruction *>> &vecAI, MonitoredRWInsts &MI) {

    for (set<Instruction *> &setInst: vecAI) {
        for (Instruction *pInst1 : setInst) {
            for (Instruction *pInst2 : setInst) {
                if (pInst1 != pInst2 && DT.dominates(pInst1, pInst2)) {
                    if (LoadInst *pLoad = dyn_cast<LoadInst>(pInst2)) {
                        MI.mapLoadID.erase(pLoad);
                    } else if (StoreInst *pStore = dyn_cast<StoreInst>(pInst2)) {
                        MI.mapStoreID.erase(pStore);
                    }
                }
            }
        }
    }
}

void NoInstrumentor::InlineGlobalCostForLoop(std::set<BasicBlock *> &setBBInLoop) {
    Function *pFunction = NULL;

    set<BasicBlock *>::iterator itSetBegin = setBBInLoop.begin();
    set<BasicBlock *>::iterator itSetEnd = setBBInLoop.end();

    assert(itSetBegin != itSetEnd);

    for (BasicBlock *BB : setBBInLoop) {
        no_opt_cost++;
//        Instruction *pTerm = BB->getTerminator();
//        auto pLoad = new LoadInst(this->numGlobalCost, "numGlobalCost", pTerm);
//        BinaryOperator *pBinary = BinaryOperator::Create(Instruction::Add, pLoad, this->ConstantLong1, "numGlobalCost++",
//                                                         pTerm);
//        auto pStoreAdd = new StoreInst(pBinary, this->numGlobalCost, false, pTerm);
//        pStoreAdd->setAlignment(8);
    }

    pFunction = (*itSetBegin)->getParent();

    BBProfilingGraph bbGraph = BBProfilingGraph(*pFunction);
    bbGraph.init();

    bbGraph.splitGivenBlock(setBBInLoop);
    bbGraph.calculateSpanningTree();

    BBProfilingEdge *pQueryEdge = bbGraph.addQueryChord();
    bbGraph.calculateChordIncrements();

    Instruction *entryInst = pFunction->getEntryBlock().getFirstNonPHI();
    AllocaInst *numLocalCounter = new AllocaInst(this->LongType, 0, "LOCAL_COST_BB", entryInst);
    numLocalCounter->setAlignment(8);
    StoreInst *pStore = new StoreInst(ConstantInt::get(this->LongType, 0), numLocalCounter, false, entryInst);
    pStore->setAlignment(8);

    bbGraph.instrumentLocalCounterUpdate(numLocalCounter, this->numGlobalCost);
}

void NoInstrumentor::InlineGlobalCostForCallee(Function *pFunction) {
    if (pFunction->getName() == "JS_Assert") {
        return;
    }

    if (pFunction->begin() == pFunction->end()) {
        return;
    }

    for (BasicBlock& B : *pFunction) {
//        no_opt_cost++;
        BasicBlock *BB = &B;
//        ++count;
        Instruction *pTerm = BB->getTerminator();
        auto pLoad = new LoadInst(this->numGlobalCost, "numGlobalCost", pTerm);
        BinaryOperator *pBinary = BinaryOperator::Create(Instruction::Add, pLoad, this->ConstantLong1, "numGlobalCost++",
                                                         pTerm);
        auto pStoreAdd = new StoreInst(pBinary, this->numGlobalCost, false, pTerm);
        pStoreAdd->setAlignment(8);
    }

    BBProfilingGraph bbGraph = BBProfilingGraph(*pFunction);
    bbGraph.init();
    bbGraph.splitNotExitBlock();

    bbGraph.calculateSpanningTree();
    BBProfilingEdge *pQueryEdge = bbGraph.addQueryChord();
    bbGraph.calculateChordIncrements();

    Instruction *entryInst = pFunction->getEntryBlock().getFirstNonPHI();
    AllocaInst *numLocalCounter = new AllocaInst(this->LongType, 0, "LOCAL_COST_BB", entryInst);
    numLocalCounter->setAlignment(8);
    StoreInst *pStore = new StoreInst(ConstantInt::get(this->LongType, 0), numLocalCounter, false, entryInst);
    pStore->setAlignment(8);
//    opt_cost++;

    bbGraph.instrumentLocalCounterUpdate(numLocalCounter, this->numGlobalCost);
}

void NoInstrumentor::InlineSetRecord(Value *address, Value *length, Value *id, Instruction *InsertBefore) {

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

    // ps->id = id;
    vector<Value *> ptr_id_indices;
    ptr_id_indices.push_back(this->ConstantInt0);
    ptr_id_indices.push_back(this->ConstantInt2);
    GetElementPtrInst *pFlag = GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst, ptr_id_indices,
                                                         "", InsertBefore);
    auto pStoreFlag = new StoreInst(id, pFlag, false, InsertBefore);
    pStoreFlag->setAlignment(4);

    // iBufferIndex_CPI += 16
    pBinary = BinaryOperator::Create(Instruction::Add, pLoadIndex, this->ConstantLong16, "iBufferIndex+=16:",
                                     InsertBefore);
    auto pStoreIndex = new StoreInst(pBinary, this->iBufferIndex_CPI, false, InsertBefore);
    pStoreIndex->setAlignment(8);
}

void NoInstrumentor::InlineHookDelimit(Instruction *InsertBefore) {

    InlineSetRecord(this->ConstantLong0, this->ConstantInt0, this->ConstantDelimit, InsertBefore);
}

void NoInstrumentor::InlineHookLoad(LoadInst *pLoad, unsigned uID, Instruction *InsertBefore) {

    assert(pLoad && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    int ID = uID;

    DataLayout *dl = new DataLayout(this->pModule);

    Value *addr = pLoad->getOperand(0);
    assert(addr);
    Type *type = addr->getType();
    assert(type);
    Type *type1 = type->getContainedType(0);
    assert(type1);

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}

void NoInstrumentor::InlineHookStore(StoreInst *pStore, unsigned uID, Instruction *InsertBefore) {

    assert(pStore && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    // Write: Negative ID
    int ID = -uID;

    DataLayout *dl = new DataLayout(this->pModule);

    Value *addr = pStore->getOperand(1);
    assert(addr);
    Type *type = addr->getType();
    assert(type);
    Type *type1 = type->getContainedType(0);
    assert(type1);

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}


void NoInstrumentor::InlineHookMemSet(MemSetInst *pMemSet, unsigned uID, Instruction *InsertBefore) {

    assert(pMemSet && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    // Write: Negative ID
    int ID = -uID;

    DataLayout *dl = new DataLayout(this->pModule);

    Value *dest = pMemSet->getDest();
    assert(dest);
    Value *len = pMemSet->getLength();
    assert(len);
    CastInst *int64_address = new PtrToIntInst(dest, this->LongType, "", InsertBefore);
    CastInst *int32_len = new TruncInst(len, this->IntType, "memset_len_conv", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(int64_address, int32_len, const_id, InsertBefore);
}

void
NoInstrumentor::InlineHookMemTransfer(MemTransferInst *pMemTransfer, unsigned uID, Instruction *InsertBefore) {

    assert(pMemTransfer && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    // Read Src: positive ID
    int srcID = uID;
    // Write Dest: Negative ID
    int dstID = -uID;

    DataLayout *dl = new DataLayout(this->pModule);

    Value *src = pMemTransfer->getSource();
    assert(src);
    Value *dest = pMemTransfer->getDest();
    assert(dest);
    Value *len = pMemTransfer->getLength();
    assert(len);
    CastInst *src_int64_address = new PtrToIntInst(src, this->LongType, "", InsertBefore);
    CastInst *dest_int64_address = new PtrToIntInst(dest, this->LongType, "", InsertBefore);
    CastInst *int32_len = new TruncInst(len, this->IntType, "memtransfer_len_conv", InsertBefore);
    ConstantInt *const_src_id = ConstantInt::get(this->pModule->getContext(),
                                                 APInt(32, StringRef(std::to_string(srcID)), 10));
    ConstantInt *const_dst_id = ConstantInt::get(this->pModule->getContext(),
                                                 APInt(32, StringRef(std::to_string(dstID)), 10));
    InlineSetRecord(src_int64_address, int32_len, const_src_id, InsertBefore);
    InlineSetRecord(dest_int64_address, int32_len, const_dst_id, InsertBefore);
}


void NoInstrumentor::InlineHookFgetc(Instruction *pCall, unsigned uID, Instruction *InsertBefore) {

    assert(pCall && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    int ID = uID;

    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(ConstantLong0, ConstantInt1, const_id, InsertBefore);
}

void NoInstrumentor::InlineHookFread(Instruction *pCall, unsigned uID, Instruction *InsertBefore) {

    assert(pCall && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    assert(InsertBefore == pCall->getNextNode());
    int ID = uID;

    CastInst *length = new TruncInst(pCall, this->IntType, "fread_len_conv", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(ConstantLong0, length, const_id, InsertBefore);
}

void NoInstrumentor::InlineHookOstream(Instruction *pCall, unsigned uID, Instruction *InsertBefore) {

    assert(pCall && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    int ID = uID;

    DataLayout *dl = new DataLayout(this->pModule);

    CallSite cs(pCall);
    Value *pSecondArg = cs.getArgument(1);
    assert(pSecondArg);

    Type *type = pSecondArg->getType();
    assert(type);
    Type *type1 = type->getContainedType(0);
    assert(type1);

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(
            std::to_string(dl->getTypeAllocSizeInBits(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(pSecondArg, this->LongType, "", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}

void NoInstrumentor::SetupInit(Module &M) {
    // all set up operation
    this->pModule = &M;
    SetupTypes();
    SetupStructs();
    SetupConstants();
    SetupGlobals();
    SetupFunctions();
}

void NoInstrumentor::SetupTypes() {

    this->VoidType = Type::getVoidTy(pModule->getContext());
    this->LongType = IntegerType::get(pModule->getContext(), 64);
    this->IntType = IntegerType::get(pModule->getContext(), 32);
    this->CharType = IntegerType::get(pModule->getContext(), 8);
    this->CharStarType = PointerType::get(this->CharType, 0);
}

void NoInstrumentor::SetupStructs() {
    vector<Type *> struct_fields;

    assert(pModule->getTypeByName("struct.stMemRecord") == nullptr);
    this->struct_stMemRecord = StructType::create(pModule->getContext(), "struct.stMemRecord");
    struct_fields.clear();
    struct_fields.push_back(this->LongType);  // address
    struct_fields.push_back(this->IntType);   // length
    struct_fields.push_back(this->IntType);   // id (+: Read, -: Write, Special:DELIMIT, LOOP_BEGIN, LOOP_END)
    if (this->struct_stMemRecord->isOpaque()) {
        this->struct_stMemRecord->setBody(struct_fields, false);
    }
}

void NoInstrumentor::SetupConstants() {

    // long: 0, 1, 16
    this->ConstantLong0 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("0"), 10));
    this->ConstantLong1 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("1"), 10));
    this->ConstantLong16 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("16"), 10));

    // int: -1, 0, 1, 2, 3, 4, 5, 6
    this->ConstantIntN1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("-1"), 10));
    this->ConstantInt0 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("0"), 10));
    this->ConstantInt1 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("1"), 10));
    this->ConstantInt2 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("2"), 10));
    this->ConstantInt3 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("3"), 10));
    this->ConstantInt4 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("4"), 10));
    this->ConstantInt5 = ConstantInt::get(pModule->getContext(), APInt(32, StringRef("5"), 10));
    // int: delimit, loop_begin, loop_end
    this->ConstantDelimit = ConstantInt::get(pModule->getContext(), APInt(32, StringRef(std::to_string(DELIMIT)), 10));
    this->ConstantLoopBegin = ConstantInt::get(pModule->getContext(),
                                               APInt(32, StringRef(std::to_string(LOOP_BEGIN)), 10));
    this->ConstantLoopEnd = ConstantInt::get(pModule->getContext(), APInt(32, StringRef(std::to_string(LOOP_END)), 10));

    // char*: NULL
    this->ConstantNULL = ConstantPointerNull::get(this->CharStarType);
}

void NoInstrumentor::SetupGlobals() {

    // int numGlobalCounter = 1;
    // TODO: CommonLinkage or ExternalLinkage
    assert(pModule->getGlobalVariable("numGlobalCounter") == nullptr);
    this->numGlobalCounter = new GlobalVariable(*pModule, this->IntType, false, GlobalValue::ExternalLinkage, nullptr,
                                                "numGlobalCounter");
    this->numGlobalCounter->setAlignment(4);
    this->numGlobalCounter->setInitializer(this->ConstantInt1);

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
    this->numGlobalCost = new GlobalVariable(*pModule, this->LongType, false, GlobalValue::ExternalLinkage, nullptr,
                                             "numGlobalCost");
    this->numGlobalCost->setAlignment(8);
    this->numGlobalCost->setInitializer(this->ConstantLong0);
}

void NoInstrumentor::SetupFunctions() {

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

bool NoInstrumentor::CloneRemapCallees(const std::set<BasicBlock *> &setBB, std::set<Function *> &setCallee,
                                            ValueToValueMapTy &originClonedMapping, std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping) {

    FindCalleesInDepth(setBB, setCallee, funcCallSiteMapping);

    DEBUG(dbgs() << "# of Functions to be cloned: " << setCallee.size() << "\n");

    CloneFunctions(setCallee, originClonedMapping);

    DEBUG(dbgs() << "# of Functions cloned: " << funcCallSiteMapping.size() << "\n");

    return RemapFunctionCalls(setCallee, funcCallSiteMapping, originClonedMapping);
}

void NoInstrumentor::FindDirectCallees(const std::set<BasicBlock *> &setBB, std::vector<Function *> &vecWorkList,
                                            std::set<Function *> &setToDo,
                                            std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping) {

    for (BasicBlock *BB : setBB) {
        if (isa<UnreachableInst>(BB->getTerminator())) {
            continue;
        }

        for (Instruction &II : *BB) {
            Instruction *pInst = &II;

            if (Function *pCalled = getCalleeFunc(pInst)) {
                if (pCalled->getName() == "JS_Assert") {
                    continue;
                }
                if (pCalled->begin() == pCalled->end()) {
                    continue;
                }
                if (pCalled->getSection().str() == ".text.startup") {
                    continue;
                }
                if (!pCalled->isDeclaration()) {
                    funcCallSiteMapping[pCalled].insert(pInst);

                    if (setToDo.find(pCalled) == setToDo.end()) {
                        setToDo.insert(pCalled);
                        vecWorkList.push_back(pCalled);
                    }
                }
            }
        }
    }
}

void NoInstrumentor::FindCalleesInDepth(const std::set<BasicBlock *> &setBB, std::set<Function *> &setToDo,
                                             std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping) {

    // Function stack to recursively handle callees
    std::vector<Function *> vecWorkList;

    // Find in the outmost BBs
    FindDirectCallees(setBB, vecWorkList, setToDo, funcCallSiteMapping);

    while (!vecWorkList.empty()) {

        Function *pCurrent = vecWorkList.back();
        vecWorkList.pop_back();

        // Store BBs in current func into set
        std::set<BasicBlock *> setCurrentBB;
        for (BasicBlock &BB : *pCurrent) {
            setCurrentBB.insert(&BB);
        }
        FindDirectCallees(setCurrentBB, vecWorkList, setToDo, funcCallSiteMapping);
    }
}

void NoInstrumentor::CloneFunctions(std::set<Function *> &setFunc, ValueToValueMapTy &originClonedMapping) {

    for (Function *pOriginFunc : setFunc) {
        DEBUG(dbgs() << pOriginFunc->getName() << "\n");
        errs() << pOriginFunc->getName() << "\n";
        Function *pClonedFunc = CloneFunction(pOriginFunc, originClonedMapping, nullptr);
        pClonedFunc->setName(pOriginFunc->getName() + ".CPI");
        pClonedFunc->setLinkage(GlobalValue::InternalLinkage);

        originClonedMapping[pOriginFunc] = pClonedFunc;
    }

    for (Function *pOriginFunc : setFunc) {
        assert(originClonedMapping.find(pOriginFunc) != originClonedMapping.end());
    }
}

bool NoInstrumentor::RemapFunctionCalls(const std::set<Function *> &setFunc,
                                             std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping,
                                             ValueToValueMapTy &originClonedMapping) {

    for (Function *pFunc : setFunc) {
        auto itFunc = originClonedMapping.find(pFunc);
        if (itFunc == originClonedMapping.end()) {
            errs() << "Cannot find the original function in origin->cloned mapping " << pFunc->getName().str()
                   << "\n";
            return false;
        }

        Function *clonedFunc = cast<Function>(itFunc->second);
        assert(clonedFunc);

        auto itSetInst = funcCallSiteMapping.find(pFunc);
        if (itSetInst == funcCallSiteMapping.end()) {
            errs() << "Cannot find the Instruction set of function " << pFunc->getName().str() << "\n";
            return false;
        }

        std::set<Instruction *> &setFuncInst = itSetInst->second;
        for (Instruction *pInst : setFuncInst) {
            auto itInst = originClonedMapping.find(pInst);
            if (itInst != originClonedMapping.end()) {
                if (CallInst *pCall = dyn_cast<CallInst>(itInst->second)) {
                    pCall->setCalledFunction(clonedFunc);
                } else if (InvokeInst *pInvoke = dyn_cast<InvokeInst>(itInst->second)) {
                    pInvoke->setCalledFunction(clonedFunc);
                } else {
                    errs() << "Instruction is not CallInst or InvokeInst\n";
                    pInst->dump();
                }
            }
        }
    }

    return true;
}

void NoInstrumentor::InstrumentMain(StringRef funcName) {

    AttributeList emptyList;

    Function *pFunctionMain = this->pModule->getFunction(funcName);

    if (!pFunctionMain) {
        errs() << "Cannot find the main function\n";
        return;
    }

    // Instrument to entry block
    {
        CallInst *pCall;
        StoreInst *pStore;

        BasicBlock &entryBlock = pFunctionMain->getEntryBlock();

        // Location: before FirstNonPHI of main function.
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
    }

    CallInst *pCall;

    for (auto &BB : *pFunctionMain) {
        for (auto &II : BB) {
            Instruction *pInst = &II;
            // Instrument FinalizeMemHooks before return/unreachable/exit...
            if (isReturnLikeInst(pInst) || isExitInst(pInst)) {
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

void NoInstrumentor::InlineOutputCost(Instruction *InsertBefore) {

    auto pLoad = new LoadInst(this->numGlobalCost, "", false, InsertBefore);
    InlineSetRecord(pLoad, this->ConstantInt0, this->ConstantInt0, InsertBefore);
}

void NoInstrumentor::InstrumentMonitoredInsts(MonitoredRWInsts &MI) {

    for (auto &kv : MI.mapLoadID) {
        LoadInst *pLoad = kv.first;
        unsigned uID = kv.second;
        InlineHookLoad(pLoad, uID, pLoad);
    }

    for (auto &kv : MI.mapStoreID) {
        StoreInst *pStore = kv.first;
        unsigned uID = kv.second;
        InlineHookStore(pStore, uID, pStore);
    }

    for (auto &kv : MI.mapMemSetID) {
        MemSetInst *pMemSet = kv.first;
        unsigned uID = kv.second;
        InlineHookMemSet(pMemSet, uID, pMemSet);
    }

    for (auto &kv : MI.mapMemTransferID) {
        MemTransferInst *pMemTransfer = kv.first;
        unsigned uID = kv.second;
        InlineHookMemTransfer(pMemTransfer, uID, pMemTransfer);
    }

    for (auto &kv : MI.mapFgetcID) {
        Instruction *pCall = kv.first;
        unsigned uID = kv.second;
        InlineHookFgetc(pCall, uID, pCall);
    }

    for (auto &kv : MI.mapFreadID) {
        Instruction *pCall = kv.first;
        unsigned uID = kv.second;
        InlineHookFread(pCall, uID, pCall->getNextNode());
    }

    for (auto &kv : MI.mapOstreamID) {
        Instruction *pCall = kv.first;
        unsigned uID = kv.second;
        InlineHookOstream(pCall, uID, pCall);
    }
}

void
NoInstrumentor::InstrumentCallees(std::set<Function *> setOriginFunc, ValueToValueMapTy &originClonedMapping, DominatorTree &DT, shared_ptr<AliasSetTracker> CurAST2) {

    MonitoredRWInsts MI;
    MonitoredRWInsts clonedMI;

    for (Function *pFunc : setOriginFunc) {

        MI.clear();
        clonedMI.clear();

        set<BasicBlock *> setBlocksInFunc;
        for (BasicBlock &BB : *pFunc) {
            for (Instruction &II : BB) {
                MI.add(&II);
            }
            setBlocksInFunc.insert(&BB);
        }

//        DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*pFunc).getDomTree();
//        LoopInfo &LoopInfo = getAnalysis<LoopInfoWrapperPass>(*pFunc).getLoopInfo();
//        AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>(*pFunc).getAAResults();
//
//        unique_ptr<AliasSetTracker> CurAST = make_unique<AliasSetTracker>(AA);

        getMonitoredRWInsts(setBlocksInFunc, MI);
        errs() << "+" << MI.size() << "\n";

//        vector<set<Instruction *>> vecAI;
//        getAliasInstInfo(std::move(CurAST), MI, vecAI);
//        removeByDomInfo(DT, vecAI, MI);

//        std::set<LoadInst *> toRemove;
//        std::set<StoreInst *> toRemoveStore;
//        for (auto &kv : MI.mapLoadID) {
//            for (auto &kv2 : MI.mapStoreID) {
//                if (DT.dominates(kv.first, kv2.first)) {
//                    toRemoveStore.insert(kv2.first);
//                }
//            }
//            for (auto &kv2 : MI.mapLoadID) {
//                if (kv.first != kv2.first && DT.dominates(kv.first, kv2.first)) {
//                    toRemove.insert(kv2.first);
//                }
//            }
//        }
//        for (auto &LI : toRemove) {
//            if (MI.mapLoadID.find(LI) != MI.mapLoadID.end()) {
//                MI.mapLoadID.erase(LI);
//            }
//        }
//
//        for (auto &SI : toRemoveStore) {
//            if (MI.mapStoreID.find(SI) != MI.mapStoreID.end()) {
//                MI.mapStoreID.erase(SI);
//            }
//        }

        errs() << "-" << MI.size() << "\n";
//
//        errs() << "-" << MI.size() << "\n";

//        InstrumentMonitoredInsts(MI);

        mapFromOriginToCloned(originClonedMapping, MI, clonedMI);

        InstrumentMonitoredInsts(clonedMI);
    }
}

void NoInstrumentor::CreateIfElseBlock(Loop *pInnerLoop, vector<BasicBlock *> &vecAdded) {
    /*
     * If (counter == 1) {              // condition1
     *      counter = gen_random();     // ifBody
     *      // while                    //      cloneBody (to be instrumented)
     * } else {
     *      counter--;                  // elseBody
     *      // while                    //      header (original code, no instrumented)
     * }
     */

    BasicBlock *pCondition1 = nullptr;

    BasicBlock *pIfBody = nullptr;
    BasicBlock *pElseBody = nullptr;
    BasicBlock *pClonedBody = nullptr;

    LoadInst *pLoad1 = nullptr;
    LoadInst *pLoad2 = nullptr;

    ICmpInst *pCmp = nullptr;

    BinaryOperator *pBinary = nullptr;
    TerminatorInst *pTerminator = nullptr;
    BranchInst *pBranch = nullptr;
    StoreInst *pStore = nullptr;
    CallInst *pCall = nullptr;
    AttributeList emptySet;

    Function *pInnerFunction = pInnerLoop->getHeader()->getParent();
    Module *pModule = pInnerFunction->getParent();

    pCondition1 = pInnerLoop->getLoopPreheader();
    BasicBlock *pHeader = pInnerLoop->getHeader();

    pIfBody = BasicBlock::Create(pModule->getContext(), ".if.body.CPI", pInnerFunction, nullptr);
    // Contains original code, thus no CPI
    pElseBody = BasicBlock::Create(pModule->getContext(), ".else.body", pInnerFunction, nullptr);
    // Cloned code, added to ifBody and elseIfBody
    pClonedBody = BasicBlock::Create(pModule->getContext(), ".cloned.body.CPI", pInnerFunction, nullptr);

    pTerminator = pCondition1->getTerminator();

    /*
    * Append to condition1:
    *  if (counter == 1) {
    *    goto ifBody;
    *  } else {
    *    goto elseBody;
    *  }
    */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pTerminator);
        pLoad1->setAlignment(4);
        pCmp = new ICmpInst(pTerminator, ICmpInst::ICMP_EQ, pLoad1, this->ConstantInt1, "cmp1");
        pBranch = BranchInst::Create(pIfBody, pElseBody, pCmp);
        ReplaceInstWithInst(pTerminator, pBranch);
    }

    /*
     * Append to ifBody:
     *  counter = gen_random();
     *  // instrumentDelimiter
     *  goto clonedBody;
     */
    {
        pLoad2 = new LoadInst(this->SAMPLE_RATE, "", false, 4, pIfBody);
        pLoad2->setAlignment(4);
        pCall = CallInst::Create(this->geo, pLoad2, "", pIfBody);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptySet);
        pStore = new StoreInst(pCall, this->numGlobalCounter, false, 4, pIfBody);
        pStore->setAlignment(4);


        BranchInst::Create(pClonedBody, pIfBody);


    }

    /*
     * Append to elseBody:
     *  counter--;
     *  goto header;
     */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pElseBody);
        pLoad1->setAlignment(4);
        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1", pElseBody);
        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pElseBody);
        pStore->setAlignment(4);
        BranchInst::Create(pHeader, pElseBody);

        for (BasicBlock::iterator II = pHeader->begin(); II != pHeader->end(); II++) {
            Instruction *I = &*II;
            if (PHINode *pPHI = dyn_cast<PHINode>(I)) {
                unsigned numIncomming = pPHI->getNumIncomingValues();
                for (unsigned j = 0; j < numIncomming; j++) {
                    BasicBlock *incommingBlock = pPHI->getIncomingBlock(j);

                    if (incommingBlock == pCondition1) {
                        pPHI->setIncomingBlock(j, pElseBody);
                    }
                }
            }
        }

    }

    // condition1: num 0
    vecAdded.push_back(pCondition1);
    vecAdded.push_back(pIfBody);
    // cloneBody: num 2
    vecAdded.push_back(pClonedBody);
    vecAdded.push_back(pElseBody);
}

//void NoInstrumentor::CreateIfElseIfBlock(Loop *pInnerLoop, vector<BasicBlock *> &vecAdded) {
//    /*
//     * if (counter == 0) {              // condition1
//     *      counter--;                  // ifBody
//     *       // while                   //      cloneBody (to be instrumented)
//     * } else if (counter == -1) {      // condition2
//     *      counter = gen_random();     // elseIfBody
//     *      // while                    //      cloneBody (to be instrumented)
//     * } else {
//     *      counter--;                  // elseBody
//     *      // while                    //      header (original code, no instrumented)
//     * }
//     */
//
//    BasicBlock *pCondition1 = nullptr;
//    BasicBlock *pCondition2 = nullptr;
//
//    BasicBlock *pIfBody = nullptr;
//    BasicBlock *pElseIfBody = nullptr;
//    BasicBlock *pElseBody = nullptr;
//    BasicBlock *pClonedBody = nullptr;
//
//    LoadInst *pLoad1 = nullptr;
//    LoadInst *pLoad2 = nullptr;
//    ICmpInst *pCmp = nullptr;
//
//    BinaryOperator *pBinary = nullptr;
//    TerminatorInst *pTerminator = nullptr;
//    BranchInst *pBranch = nullptr;
//    StoreInst *pStore = nullptr;
//    CallInst *pCall = nullptr;
//    AttributeList emptySet;
//
//    Function *pInnerFunction = pInnerLoop->getHeader()->getParent();
//    Module *pModule = pInnerFunction->getParent();
//
//    pCondition1 = pInnerLoop->getLoopPreheader();
//    BasicBlock *pHeader = pInnerLoop->getHeader();
//
//    pIfBody = BasicBlock::Create(pModule->getContext(), ".if.body.CPI", pInnerFunction, nullptr);
//
//    pCondition2 = BasicBlock::Create(pModule->getContext(), ".if2.CPI", pInnerFunction, nullptr);
//
//    pElseIfBody = BasicBlock::Create(pModule->getContext(), ".else.if.body.CPI", pInnerFunction, nullptr);
//    // Contains original code, thus no CPI
//    pElseBody = BasicBlock::Create(pModule->getContext(), ".else.body", pInnerFunction, nullptr);
//    // Cloned code, added to ifBody and elseIfBody
//    pClonedBody = BasicBlock::Create(pModule->getContext(), ".cloned.body.CPI", pInnerFunction, nullptr);
//
//    pTerminator = pCondition1->getTerminator();
//
//    /*
//     * Append to condition1:
//     *  if (counter == 0) {
//     *    goto ifBody;
//     *  } else {
//     *    goto condition2;
//     *  }
//     */
//    {
//        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pTerminator);
//        pLoad1->setAlignment(4);
//        pCmp = new ICmpInst(pTerminator, ICmpInst::ICMP_EQ, pLoad1, this->ConstantInt0, "cmp0");
//        pBranch = BranchInst::Create(pIfBody, pCondition2, pCmp);
//        ReplaceInstWithInst(pTerminator, pBranch);
//    }
//
//    /*
//     * Append to ifBody:
//     *  counter--;  // counter-- -> counter += -1
//     *  goto clonedBody;
//     */
//    {
//        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pIfBody);
//        pLoad1->setAlignment(4);
//        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1_0", pIfBody);
//        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pIfBody);
//        pStore->setAlignment(4);
//        BranchInst::Create(pClonedBody, pIfBody);
//    }
//
//    /*
//     * Append to condition2:
//     *  if (counter == -1) {
//     *    goto elseIfBody;
//     *  } else {
//     *    goto elseBody;
//     *  }
//     */
//    {
//        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pCondition2);
//        pLoad1->setAlignment(4);
//        pCmp = new ICmpInst(*pCondition2, ICmpInst::ICMP_EQ, pLoad1, this->ConstantIntN1, "cmpN1");
//        BranchInst::Create(pElseIfBody, pElseBody, pCmp, pCondition2);
//    }
//
//    /*
//     * Append to elseIfBody:
//     *  counter = gen_random();
//     *  goto clonedBody;
//     */
//    {
//        pLoad2 = new LoadInst(this->SAMPLE_RATE, "", false, 4, pElseIfBody);
//        pLoad2->setAlignment(4);
//        pCall = CallInst::Create(this->geo, pLoad2, "", pElseIfBody);
//        pCall->setCallingConv(CallingConv::C);
//        pCall->setTailCall(false);
//        pCall->setAttributes(emptySet);
//        pStore = new StoreInst(pCall, this->numGlobalCounter, false, 4, pElseIfBody);
//        pStore->setAlignment(4);
//        BranchInst::Create(pClonedBody, pElseIfBody);
//    }
//
//    /*
//     * Append to elseBody:
//     *  counter--;
//     *  goto header;
//     */
//    {
//        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pElseBody);
//        pLoad1->setAlignment(4);
//        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1_1", pElseBody);
//        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pElseBody);
//        pStore->setAlignment(4);
//        BranchInst::Create(pHeader, pElseBody);
//    }
//
//    // condition1: num 0
//    vecAdded.push_back(pCondition1);
//    vecAdded.push_back(pIfBody);
//    // cloneBody: num 2
//    vecAdded.push_back(pClonedBody);
//    vecAdded.push_back(pCondition2);
//    vecAdded.push_back(pElseIfBody);
//    vecAdded.push_back(pElseBody);
//}

void NoInstrumentor::CloneInnerLoop(Loop *pLoop, vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap,
                                         vector<BasicBlock *> &vecCloned) {

    Function *pFunction = pLoop->getHeader()->getParent();

    SmallVector<BasicBlock *, 4> ExitBlocks;
    pLoop->getExitBlocks(ExitBlocks);

    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
        VMap[ExitBlocks[i]] = ExitBlocks[i];
    }

    vector<BasicBlock *> ToClone;
    vector<BasicBlock *> BeenCloned;

    set<BasicBlock *> setCloned;

    //clone loop
    ToClone.push_back(pLoop->getHeader());

    while (!ToClone.empty()) {

        BasicBlock *pCurrent = ToClone.back();
        ToClone.pop_back();

        WeakTrackingVH &BBEntry = VMap[pCurrent];
        if (BBEntry) {
            continue;
        }

        BasicBlock *NewBB;
        BBEntry = NewBB = BasicBlock::Create(pCurrent->getContext(), "", pFunction);

        if (pCurrent->hasName()) {
            NewBB->setName(pCurrent->getName() + ".CPI");
            // add to vecCloned
            vecCloned.push_back(NewBB);
        }

        if (pCurrent->hasAddressTaken()) {
            errs() << "hasAddressTaken branch\n";
            exit(0);
        }

        for (auto &II : *pCurrent) {
            Instruction *Inst = &II;
            Instruction *NewInst = Inst->clone();
            if (Inst->hasName()) {
                NewInst->setName(Inst->getName() + ".CPI");
            }
            VMap[Inst] = NewInst;
            NewBB->getInstList().push_back(NewInst);
        }

        const TerminatorInst *TI = pCurrent->getTerminator();
        for (unsigned i = 0, e = TI->getNumSuccessors(); i != e; ++i) {
            ToClone.push_back(TI->getSuccessor(i));
        }

        setCloned.insert(pCurrent);
        BeenCloned.push_back(NewBB);
    }

    //remap value used inside loop
    for (auto &BB : BeenCloned) {
        for (auto &II : *BB) {
            this->RemapInstruction(&II, VMap);
        }
    }

    //add to ifBody and elseIfBody
    BasicBlock *pCondition1 = vecAdd[0];
    BasicBlock *pClonedBody = vecAdd[2];

    auto pClonedHeader = cast<BasicBlock>(VMap[pLoop->getHeader()]);

    BranchInst::Create(pClonedHeader, pClonedBody);

    for (BasicBlock::iterator II = pClonedHeader->begin(); II != pClonedHeader->end(); II++) {
        if (auto pPHI = dyn_cast<PHINode>(II)) {
            // vector<int> vecToRemoved;
            for (unsigned i = 0, e = pPHI->getNumIncomingValues(); i != e; ++i) {
                if (pPHI->getIncomingBlock(i) == pCondition1) {
                    pPHI->setIncomingBlock(i, pClonedBody);
                }
            }
        }
    }

    set<BasicBlock *> setProcessedBlock;

    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {

        if (setProcessedBlock.find(ExitBlocks[i]) != setProcessedBlock.end()) {
            continue;

        } else {

            setProcessedBlock.insert(ExitBlocks[i]);
        }

        for (BasicBlock::iterator II = ExitBlocks[i]->begin(); II != ExitBlocks[i]->end(); II++) {

            if (auto pPHI = dyn_cast<PHINode>(II)) {

                unsigned numIncomming = pPHI->getNumIncomingValues();

                for (unsigned j = 0; j < numIncomming; j++) {

                    BasicBlock *incommingBlock = pPHI->getIncomingBlock(j);

                    if (VMap.find(incommingBlock) != VMap.end()) {

                        Value *incommingValue = pPHI->getIncomingValue(j);

                        if (VMap.find(incommingValue) != VMap.end()) {
                            incommingValue = VMap[incommingValue];
                        }

                        pPHI->addIncoming(incommingValue, cast<BasicBlock>(VMap[incommingBlock]));

                    }
                }
            }
        }
    }
}

//void NoInstrumentor::CloneInnerLoop(Loop *pLoop, vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap,
//                                         vector<BasicBlock *> &vecCloned) {
//
//    Function *pFunction = pLoop->getHeader()->getParent();
//
//    SmallVector<BasicBlock *, 4> ExitBlocks;
//    pLoop->getExitBlocks(ExitBlocks);
//
//    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
//        VMap[ExitBlocks[i]] = ExitBlocks[i];
//    }
//
//    vector<BasicBlock *> ToClone;
//    vector<BasicBlock *> BeenCloned;
//
//    set<BasicBlock *> setCloned;
//
//    //clone loop
//    ToClone.push_back(pLoop->getHeader());
//
//    while (!ToClone.empty()) {
//
//        BasicBlock *pCurrent = ToClone.back();
//        ToClone.pop_back();
//
//        WeakTrackingVH &BBEntry = VMap[pCurrent];
//        if (BBEntry) {
//            continue;
//        }
//
//        BasicBlock *NewBB;
//        BBEntry = NewBB = BasicBlock::Create(pCurrent->getContext(), "", pFunction);
//
//        if (pCurrent->hasName()) {
//            NewBB->setName(pCurrent->getName() + ".CPI");
//            // add to vecCloned
//            vecCloned.push_back(NewBB);
//        }
//
//        if (pCurrent->hasAddressTaken()) {
//            errs() << "hasAddressTaken branch\n";
//            exit(0);
//        }
//
//        for (auto &II : *pCurrent) {
//            Instruction *Inst = &II;
//            Instruction *NewInst = Inst->clone();
//            if (Inst->hasName()) {
//                NewInst->setName(Inst->getName() + ".CPI");
//            }
//            VMap[Inst] = NewInst;
//            NewBB->getInstList().push_back(NewInst);
//        }
//
//        const TerminatorInst *TI = pCurrent->getTerminator();
//        for (unsigned i = 0, e = TI->getNumSuccessors(); i != e; ++i) {
//            ToClone.push_back(TI->getSuccessor(i));
//        }
//
//        setCloned.insert(pCurrent);
//        BeenCloned.push_back(NewBB);
//    }
//
//    //remap value used inside loop
//    for (auto &BB : BeenCloned) {
//        for (auto &II : *BB) {
//            this->RemapInstruction(&II, VMap);
//        }
//    }
//
//    //add to ifBody and elseIfBody
//    BasicBlock *pCondition1 = vecAdd[0];
//    BasicBlock *pClonedBody = vecAdd[2];
//
//    auto pClonedHeader = cast<BasicBlock>(VMap[pLoop->getHeader()]);
//
//    BranchInst::Create(pClonedHeader, pClonedBody);
//
//    for (BasicBlock::iterator II = pClonedHeader->begin(); II != pClonedHeader->end(); II++) {
//        if (auto pPHI = dyn_cast<PHINode>(II)) {
//            // vector<int> vecToRemoved;
//            for (unsigned i = 0, e = pPHI->getNumIncomingValues(); i != e; ++i) {
//                if (pPHI->getIncomingBlock(i) == pCondition1 || pPHI->getIncomingBlock(i)->getName() == ".else.body") {
//                    pPHI->setIncomingBlock(i, pClonedBody);
//                }
//            }
//        }
//    }
//
//    set<BasicBlock *> setProcessedBlock;
//
//    for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
//        if (setProcessedBlock.find(ExitBlocks[i]) != setProcessedBlock.end()) {
//            continue;
//        } else {
//            setProcessedBlock.insert(ExitBlocks[i]);
//        }
//
//        for (BasicBlock::iterator II = ExitBlocks[i]->begin(); II != ExitBlocks[i]->end(); II++) {
//            if (auto pPHI = dyn_cast<PHINode>(II)) {
//                unsigned numIncomming = pPHI->getNumIncomingValues();
//                for (unsigned j = 0; j < numIncomming; j++) {
//                    BasicBlock *incommingBlock = pPHI->getIncomingBlock(j);
//                    if (VMap.find(incommingBlock) != VMap.end() && VMap[incommingBlock] != incommingBlock) {
//                        Value *incommingValue = pPHI->getIncomingValue(j);
//                        if (VMap.find(incommingValue) != VMap.end()) {
//                            incommingValue = VMap[incommingValue];
//                        }
//                        pPHI->addIncoming(incommingValue, cast<BasicBlock>(VMap[incommingBlock]));
//                    }
//                }
//            }
//        }
//    }
//
//    map<Instruction *, set<Instruction *> > mapAddPHI;
//
//    for (Loop::block_iterator BB = pLoop->block_begin(); BB != pLoop->block_end(); BB++) {
//        BasicBlock *B = *BB;
//        for (BasicBlock::iterator II = B->begin(); II != B->end(); II++) {
//            Instruction *I = &*II;
//            for (Instruction::user_iterator UU = I->user_begin(); UU != I->user_end(); UU++) {
//                if (Instruction *IU = dyn_cast<Instruction>(*UU)) {
//                    if (!pLoop->contains(IU->getParent())) {
//                        if (mapAddPHI.find(I) == mapAddPHI.end()) {
//                            set<Instruction *> setTmp;
//                            mapAddPHI[I] = setTmp;
//                        }
//
//                        mapAddPHI[I].insert(IU);
//                    }
//                }
//            }
//        }
//    }
//
//    map<Instruction *, set<Instruction *> >::iterator itMapBegin = mapAddPHI.begin();
//    map<Instruction *, set<Instruction *> >::iterator itMapEnd = mapAddPHI.end();
//
//    for (; itMapBegin != itMapEnd; itMapBegin++) {
//        Instruction *I = itMapBegin->first;
//
//        if (VMap.find(I) == VMap.end()) {
//            continue;
//        }
//
//        BasicBlock *pInsert = NULL;
//        for (unsigned long i = 0; i < ExitBlocks.size(); i++) {
//            set<Instruction *>::iterator itSetBegin = itMapBegin->second.begin();
//            set<Instruction *>::iterator itSetEnd = itMapBegin->second.end();
//
//            bool bFlag = true;
//            for (; itSetBegin != itSetEnd; itSetBegin++) {
//                if (ExitBlocks[i] != (*itSetBegin)->getParent()) {
//                    bFlag = false;
//                    break;;
//                }
//            }
//
//            if (bFlag) {
//                pInsert = ExitBlocks[i];
//                break;
//            }
//
//        }
//
//        if (pInsert == NULL) {
//            assert(0);
//        }
//
//        PHINode *pPHI = PHINode::Create(I->getType(), 2, ".Created.PHI", pInsert->getFirstNonPHI());
//
//        BasicBlock *pIncomingBlock1 = NULL;
//
//        for (pred_iterator PI = pred_begin(pInsert), PE = pred_end(pInsert); PI != PE; ++PI) {
//            BasicBlock *BB = *PI;
//            if (pLoop->contains(BB)) {
//                pIncomingBlock1 = BB;
//            }
//        }
//
//
//        pPHI->addIncoming(I, pIncomingBlock1);
//
//        BasicBlock *clonedBB = dyn_cast<BasicBlock>(VMap[pIncomingBlock1]);
//        pPHI->addIncoming(VMap[I], clonedBB);
//
//        set<Instruction *>::iterator itSetBegin = itMapBegin->second.begin();
//        set<Instruction *>::iterator itSetEnd = itMapBegin->second.end();
//
//        for (; itSetBegin != itSetEnd; itSetBegin++) {
//            Instruction *IU = *itSetBegin;
//
//            unsigned indexOperand = 0;
//
//            for (; indexOperand < IU->getNumOperands(); indexOperand++) {
//                if (IU->getOperand(indexOperand) == I) {
//                    IU->setOperand(indexOperand, pPHI);
//                }
//            }
//
//        }
//    }
//
//}

void NoInstrumentor::RemapInstruction(Instruction *I, ValueToValueMapTy &VMap) {
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

void NoInstrumentor::getInstsToBeHoisted(Loop *pLoop, AliasAnalysis &AA, MonitoredRWInsts &inplaceMI,
                                              MonitoredRWInsts &hoistMI) {

    assert(pLoop);

    for (auto &kv : inplaceMI.mapLoadID) {
        LoadInst *pLoad = kv.first;
        unsigned uID = kv.second;
        if (pLoop->isLoopInvariant(pLoad->getOperand(0))) {
            hoistMI.mapLoadID[pLoad] = uID;
        }
    }

    for (auto &kv : inplaceMI.mapStoreID) {
        StoreInst *pStore = kv.first;
        unsigned uID = kv.second;
        if (pLoop->isLoopInvariant(pStore->getOperand(1))) {
            hoistMI.mapStoreID[pStore] = uID;
        }
    }

    // After Alias+Dominance checking and removing,
    // if operands of a LoadInst and a StoreInst are still Alias/equal,
    // then their sequence is unknown, thus should not be hoisted.
    for (auto &kvLoad : hoistMI.mapLoadID) {
        LoadInst *pLoad = kvLoad.first;
        for (auto &kvStore : hoistMI.mapStoreID) {
            StoreInst *pStore = kvStore.first;
            if (AA.isMustAlias(pLoad, pStore)) {
                hoistMI.mapLoadID.erase(pLoad);
                hoistMI.mapStoreID.erase(pStore);
            }
        }
    }

    inplaceMI.diff(hoistMI);

    // TODO: mem and IO wait to be verified
}

bool NoInstrumentor::cloneDependantInsts(Loop *pLoop, Instruction *pInst, Instruction *InsertBefore,
                                              ValueToValueMapTy &VMap) {

//    errs() << pLoop->contains(pInst) << ":\n";
//    pInst->dump();
//
//    vector<Instruction *> workList;
//
//    unsigned opNum = pInst->getNumOperands();
//    for (unsigned i = 0; i < opNum; ++i) {
//        Value *value = pInst->getOperand(i);
//        value->dump();
//        if (Instruction *pNextInst = dyn_cast<Instruction>(value)) {
//            errs() << "is Inst\n";
//            errs() << pLoop->contains(pNextInst) << "\n";
//            workList.push_back(pNextInst);
//        }
//        errs() << "is not Inst\n";
//        errs() << "\n";
//    }
//
//    vector<Instruction *> workList2;
//    for (Instruction *pNextInst : workList) {
//        unsigned opNum = pNextInst->getNumOperands();
//        for (unsigned i = 0; i < opNum; ++i) {
//            Value *value = pNextInst->getOperand(i);
//            value->dump();
//            if (Instruction *pNextInst2 = dyn_cast<Instruction>(value)) {
//                errs() << "is Inst\n";
//                errs() << pLoop->contains(pNextInst2) << "\n";
//                workList2.push_back(pNextInst2);
//            }
//            errs() << "is not Inst\n";
//            errs() << "\n";
//        }
//    }
//
//    vector<Instruction *> workList3;
//    for (Instruction *pNextInst : workList2) {
//        unsigned opNum = pNextInst->getNumOperands();
//        for (unsigned i = 0; i < opNum; ++i) {
//            Value *value = pNextInst->getOperand(i);
//            value->dump();
//            if (Instruction *pNextInst2 = dyn_cast<Instruction>(value)) {
//                errs() << "is Inst\n";
//                errs() << pLoop->contains(pNextInst2) << "\n";
//                workList3.push_back(pNextInst2);
//            }
//            errs() << "is not Inst\n";
//            errs() << "\n";
//        }
//    }


    vector<Instruction *> toClone;
    vector<Instruction *> workList;

    workList.push_back(pInst);
    toClone.push_back(pInst);
    while (!workList.empty()) {
        Instruction *pCurrInst = workList.back();
        workList.pop_back();
        unsigned opNum = pCurrInst->getNumOperands();
        for (unsigned i = 0; i < opNum; ++i) {
            if (Value *value = pCurrInst->getOperand(i)) {
                if (Instruction *pNextInst = dyn_cast<Instruction>(value)) {
                    if (pLoop->contains(pNextInst)) {
                        toClone.push_back(pNextInst);
                        workList.push_back(pNextInst);
                    }
                }
            }
        }
    }

    vector<Instruction *> clonedToClone;
    for (Instruction *pOrigin : toClone) {
        auto it = VMap.find(pOrigin);
        if (it != VMap.end()) {
            if (Instruction *pCloned = cast<Instruction>(it->second)) {
                clonedToClone.push_back(pCloned);
            }
        }
    }

    vector<Instruction *> cloned;
    // Reverse: seq: first pInst, insert: last pInst
    for (auto rit = clonedToClone.rbegin(); rit != clonedToClone.rend(); ++rit) {
        Instruction *pInstToClone = *rit;
        Instruction *pClonedInst = pInstToClone->clone();
        if (pClonedInst->getName() != "") {
            pClonedInst->setName(pClonedInst->getName() + ".Hoist");
        }
        VMap[pInstToClone] = pClonedInst;
        cloned.push_back(pClonedInst);
        pClonedInst->insertBefore(InsertBefore);
    }

    for (Instruction *pClonedInst : cloned) {
        if (pClonedInst) {
            RemapInstruction(pClonedInst, VMap);
        }
    }

    unsigned uID = GetInstructionID(pInst);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    // the cloned loop
    auto it = VMap.find(pInst);
    if (it == VMap.end()) {
        return false;
    }
    // the cloned Insts in hoist/sink loc
    auto it2 = VMap.find(it->second);
    if (it2 == VMap.end()) {
        return false;
    }

    if (LoadInst *pClonedLoad = dyn_cast<LoadInst>(it2->second)) {
        InlineHookLoad(pClonedLoad, uID, pClonedLoad);
        return true;
    } else {
        return false;
    }
}

bool NoInstrumentor::sinkInstsToLoopExit(Loop *pLoop, Instruction *pInst, Instruction *InsertBefore,
                                              ValueToValueMapTy &VMap) {

//    SmallVector<BasicBlock *, 4> vecExitBlocks;
//    pLoop->getExitBlocks(vecExitBlocks);
//    SmallSet<BasicBlock *, 4> setClonedExitBlocks;
//    for (BasicBlock *BB : vecExitBlocks) {
//        auto itBB = VMap.find(BB);
//        if (itBB != VMap.end()) {
//            BasicBlock *clonedBB = dyn_cast<BasicBlock>(itBB->second);
//            if (clonedBB) {
//                setClonedExitBlocks.insert(clonedBB);
//            }
//        }
//    }
//    SmallVector<Loop::Edge, 4> vecExitEdge;
//    pLoop->getExitEdges(vecExitEdge);
//
//    SmallSet<BasicBlock *, 4> setPreExitBlock;
//    for (const auto &edge : vecExitEdge) {
//        BasicBlock *PreExit = const_cast<BasicBlock *>(edge.first);
//        setPreExitBlock.insert(PreExit);
//    }
//
    unsigned uID = GetInstructionID(pInst);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);

    auto it = VMap.find(pInst);
    if (it == VMap.end()) {
        return false;
    }
    LoadInst *pClonedLoad = dyn_cast<LoadInst>(it->second);
    if (!pClonedLoad) {
        return false;
    }

    InlineHookLoad(pClonedLoad, uID, InsertBefore);
//
//    for (BasicBlock *BB : setPreExitBlock) {
//        errs() << BB->getName() << "\n";
//        Instruction *InsertBefore = BB->getTerminator();
//        InsertBefore->dump();
//        InlineHookLoad(pClonedLoad, uID, InsertBefore);
//    }



    return true;
}

void NoInstrumentor::insertBeforeExitLoop(Loop *pLoop, DominatorTree &DT, ValueToValueMapTy &VMap,
                                               map<BasicBlock *, BasicBlock *> &mapExit2Inter) {

    Function *pFunction = pLoop->getHeader()->getParent();

    SmallVector<BasicBlock *, 4> vecExitBlock;
    pLoop->getExitBlocks(vecExitBlock);

    for (const auto &BB : vecExitBlock) {
        errs() << "ExitBlock:" << BB->getName() << '\n';
    }

    SmallDenseSet<BasicBlock *> setNonDomExitBlock;
    for (const auto &dominatee : vecExitBlock) {
        bool isDominated = false;
        for (const auto &dominator : vecExitBlock) {
            if (dominatee != dominator && DT.dominates(dominator, dominatee)) {
                isDominated = true;
                break;
            }
        }
        if (!isDominated) {
            setNonDomExitBlock.insert(dominatee);
        }
    }

    SmallVector<Loop::Edge, 4> vecExitEdge;
    pLoop->getExitEdges(vecExitEdge);

    SmallDenseSet<Loop::Edge> setNonDomExitEdge;
    for (auto &exitEdge : vecExitEdge) {
        const BasicBlock *outsideBlock = exitEdge.second;
        if (outsideBlock &&
            std::find(setNonDomExitBlock.begin(), setNonDomExitBlock.end(), outsideBlock) != setNonDomExitBlock.end()) {
            setNonDomExitEdge.insert(exitEdge);
            errs() << "PreExitBlock:" << exitEdge.first->getName() << '\n';
        }
    }

    for (auto &exitEdge : setNonDomExitEdge) {
        const BasicBlock *insideBlock = exitEdge.first;
        const BasicBlock *outsideBlock = exitEdge.second;

        // convert
        BasicBlock *clonedInsideBlock = nullptr;
        BasicBlock *clonedOutsideBlock = nullptr;
        ValueToValueMapTy::iterator It = VMap.find(insideBlock);
        if (It != VMap.end()) {
            clonedInsideBlock = cast<BasicBlock>(It->second);
        }
        It = VMap.find(outsideBlock);
        if (It != VMap.end()) {
            clonedOutsideBlock = cast<BasicBlock>(It->second);
        }

        BasicBlock *interBlock = BasicBlock::Create(clonedInsideBlock->getContext(), ".inter.block.CPI", pFunction);

        TerminatorInst *TI = clonedInsideBlock->getTerminator();

        for (unsigned i = 0, e = TI->getNumSuccessors(); i != e; ++i) {
            if (TI->getSuccessor(i) == clonedOutsideBlock) {
                TI->setSuccessor(i, interBlock);
                BranchInst::Create(clonedOutsideBlock, interBlock);
                interBlock->replaceSuccessorsPhiUsesWith(interBlock);

                mapExit2Inter[clonedInsideBlock] = interBlock;
            }
        }
    }
}

