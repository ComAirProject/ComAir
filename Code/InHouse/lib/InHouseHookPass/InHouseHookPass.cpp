#define DEBUG_TYPE "InHouseHookPass"

#include "InHouseHookPass/InHouseHookPass.h"

#include "llvm/IR/Dominators.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "Common/InputFileParser.h"
#include "Common/Helper.h"
#include "Common/BBProfiling.h"
#include "Common/MonitorRWInsts.h"

using namespace llvm;

namespace inhouse_hook_pass
{

STATISTIC(NumRWInsts, "The # of RW Insts");
STATISTIC(NumCost, "The # of Costs");

static cl::opt<std::string> strEntryFunc("entry-func",
                                         cl::desc("Entry Function Name"), cl::Optional,
                                         cl::value_desc("strEntryFunc"));

static cl::opt<std::string> strFuncListFile("func-list-file", cl::desc("FuncList File"), cl::Optional,
                                            cl::value_desc("strFuncListFile"));

static cl::opt<bool> bNoOptInst("bNoOptInst", cl::desc("No Opt for Num of Instrument Sites"), cl::Optional,
                                cl::value_desc("bNoOptInst"));

static cl::opt<bool> bNoOptCost("bNoOptCost", cl::desc("No Opt for Merging Cost"), cl::Optional,
                                cl::value_desc("bNoOptCost"));

char InHouseHookPass::ID = 0;

InHouseHookPass::InHouseHookPass() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeDominatorTreeWrapperPassPass(Registry);
}

void InHouseHookPass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<DominatorTreeWrapperPass>();
}

bool InHouseHookPass::runOnModule(llvm::Module &M)
{

    StringRef EntryFuncName;
    if (strEntryFunc.empty())
    {
        EntryFuncName = "main";
    }
    else
    {
        EntryFuncName = strEntryFunc;
    }

    Function *EntryFunc = M.getFunction(EntryFuncName);
    if (!EntryFunc)
    {
        errs() << "Entry Func: " << EntryFuncName << " not found\n";
        return false;
    }

    this->pModule = &M;

    SetupInit();

    std::set<Function *> setFuncList;
    if (!strFuncListFile.empty())
    {
        std::set<unsigned> setFuncIDList;
        bool ok = common::parseFuncListFile(strFuncListFile.c_str(), setFuncIDList);
        if (!ok)
        {
            errs() << strFuncListFile << " parsing failed\n";
            return false;
        }
        GetFuncList(setFuncIDList, setFuncList);
    }
    else
    {
        GetFuncList(setFuncList);
    }

    InstrumentEntryFunc(EntryFunc);

    // remove global ctors and their callees
    std::set<Function *> setTextStartupFuncList;
    for (Function &F : M)
    {
        if (F.getSection().str() == ".text.startup")
        {
            setTextStartupFuncList.insert(&F);
        }
    }

    std::list<Function *> WorkList;
    std::set<Function *> Visited; // Global ctors and their callees
    for (Function *F : setTextStartupFuncList)
    {
        WorkList.push_back(F);
    }
    while (!WorkList.empty())
    {
        Function *Curr = WorkList.front();
        WorkList.pop_front();

        for (BasicBlock &B : *Curr)
        {
            for (Instruction &I : B)
            {
                if (isa<CallInst>(&I) || isa<InvokeInst>(&I))
                {
                    CallSite CS(&I);
                    Function *Callee = CS.getCalledFunction();
                    if (Callee && Visited.find(Callee) == Visited.end())
                    {
                        WorkList.push_back(Callee);
                        Visited.insert(Callee);
                    }
                }
            }
        }
    }

    for (Function *F : setFuncList)
    {
        if (!IsIgnoreFunc(F) && F != EntryFunc && Visited.find(F) == Visited.end())
        {
            // errs().write_escaped(F->getName()) << "\n";
            InstrumentFunc(F);
        }
    }

    errs() << "NumRWInsts:" << NumRWInsts << ", NumCost:" << NumCost << "\n";

    return true;
}

static void removeByDomInfo(common::MonitoredRWInsts &MI, DominatorTree &DT)
{
    std::map<Value *, std::set<Instruction *>> AliasedInsts;
    for (auto &kv : MI.mapLoadID)
    {
        Value *V = kv.first->getPointerOperand();
        if (AliasedInsts.find(V) == AliasedInsts.end())
        {
            AliasedInsts[V] = std::set<Instruction *>();
        }
        AliasedInsts[V].insert(kv.first);
    }
    for (auto &kv : MI.mapStoreID)
    {
        Value *V = kv.first->getPointerOperand();
        if (AliasedInsts.find(V) == AliasedInsts.end())
        {
            AliasedInsts[V] = std::set<Instruction *>();
        }
        AliasedInsts[V].insert(kv.first);
    }

    for (auto &kv : AliasedInsts)
    {
        if (!common::hasNonLoadStoreUse(kv.first) && kv.second.size() > 1)
        {
            for (Instruction *rhs : kv.second)
            {
                for (Instruction *lhs : kv.second)
                {
                    if (lhs != rhs)
                    {
                        if (DT.dominates(lhs, rhs))
                        {
                            if (LoadInst *LI = dyn_cast<LoadInst>(rhs))
                            {
                                // LI->dump();
                                MI.mapLoadID.erase(LI);
                            }
                            else if (StoreInst *SI = dyn_cast<StoreInst>(rhs))
                            {
                                // SI->dump();
                                MI.mapStoreID.erase(SI);
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
}

void InHouseHookPass::SetupInit()
{

    SetupTypes();
    SetupConstants();
    SetupFunctions();
}

void InHouseHookPass::SetupTypes()
{

    this->VoidType = Type::getVoidTy(this->pModule->getContext());
    this->IntType = IntegerType::get(this->pModule->getContext(), 32);
    this->LongType = IntegerType::get(this->pModule->getContext(), 64);
}

void InHouseHookPass::SetupConstants()
{
    this->ConstantLong0 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("0"), 10));
    this->ConstantLong1 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("1"), 10));
}

void InHouseHookPass::SetupFunctions()
{

    SmallVector<Type *, 4> ArgTypes;

    // inhouse aprof hooks
    // void aprof_init();
    this->aprof_init = this->pModule->getFunction("aprof_init");
    if (!this->aprof_init)
    {
        FunctionType *AprofInitType = FunctionType::get(this->VoidType, ArgTypes, false);
        this->aprof_init = Function::Create(AprofInitType, GlobalValue::ExternalLinkage, "aprof_init",
                                            this->pModule);
        this->aprof_init->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
    // void aprof_write(unsigned long start_addr, unsigned long length);
    this->aprof_write = this->pModule->getFunction("aprof_write");
    if (!this->aprof_write)
    {
        ArgTypes.push_back(this->LongType);
        ArgTypes.push_back(this->LongType);
        FunctionType *AprofWriteType = FunctionType::get(this->VoidType, ArgTypes, false);
        this->aprof_write = Function::Create(AprofWriteType, GlobalValue::ExternalLinkage, "aprof_write",
                                             this->pModule);
        this->aprof_write->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
    //void aprof_read(unsigned long start_addr, unsigned long length);
    this->aprof_read = this->pModule->getFunction("aprof_read");
    if (!this->aprof_read)
    {
        ArgTypes.push_back(this->LongType);
        ArgTypes.push_back(this->LongType);
        FunctionType *AprofReadType = FunctionType::get(this->VoidType, ArgTypes, false);
        this->aprof_read = Function::Create(AprofReadType, GlobalValue::ExternalLinkage, "aprof_read",
                                            this->pModule);
        this->aprof_read->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
    // void aprof_increment_rms(unsigned long length);
    this->aprof_increment_rms = this->pModule->getFunction("aprof_increment_rms");
    if (!this->aprof_increment_rms)
    {
        ArgTypes.push_back(this->LongType);
        FunctionType *AprofIncrementRmsType = FunctionType::get(this->VoidType, ArgTypes, false);
        this->aprof_increment_rms = Function::Create(AprofIncrementRmsType, GlobalValue::ExternalLinkage,
                                                     "aprof_increment_rms", this->pModule);
        this->aprof_read->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
    // void aprof_call_before(unsigned funcId);
    this->aprof_call_before = this->pModule->getFunction("aprof_call_before");
    if (!this->aprof_call_before)
    {
        ArgTypes.push_back(this->IntType);
        FunctionType *AprofCallBeforeType = FunctionType::get(this->VoidType, ArgTypes, false);
        this->aprof_call_before = Function::Create(AprofCallBeforeType, GlobalValue::ExternalLinkage,
                                                   "aprof_call_before", this->pModule);
        this->aprof_call_before->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
    // void aprof_return(unsigned long numCost);
    this->aprof_return = this->pModule->getFunction("aprof_return");
    if (!this->aprof_return)
    {
        ArgTypes.push_back(this->LongType);
        FunctionType *AprofReturnType = FunctionType::get(this->VoidType, ArgTypes, false);
        this->aprof_return = Function::Create(AprofReturnType, GlobalValue::ExternalLinkage, "aprof_return",
                                              this->pModule);
        this->aprof_return->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
    // void aprof_final();
    this->aprof_final = this->pModule->getFunction("aprof_final");
    if (!this->aprof_final)
    {
        FunctionType *AprofFinalType = FunctionType::get(this->VoidType, ArgTypes, false);
        this->aprof_final = Function::Create(AprofFinalType, GlobalValue::ExternalLinkage, "aprof_final",
                                             this->pModule);
        this->aprof_final->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
}

void InHouseHookPass::GetFuncList(const std::set<unsigned> &setFuncIDList, std::set<Function *> &setFuncList)
{

    for (Function &F : *pModule)
    {
        if (IsIgnoreFunc(&F))
        {
            continue;
        }
        unsigned funcID = GetFunctionID(&F);
        if (setFuncIDList.find(funcID) != setFuncIDList.end())
        {
            setFuncList.insert(&F);
        }
    }
}

void InHouseHookPass::GetFuncList(std::set<Function *> &setFuncList)
{

    for (Function &F : *pModule)
    {
        if (IsIgnoreFunc(&F))
        {
            continue;
        }
        setFuncList.insert(&F);
    }
}

void InHouseHookPass::CollectInstructions(Function *F, common::MonitoredRWInsts &MI, Instruction *&FuncEntryInst,
                                          std::map<BasicBlock *, Instruction *> &mapBBTermInst,
                                          std::set<Instruction *> &setFuncExitInst)
{

    assert(F);

    for (BasicBlock &B : *F)
    {
        TerminatorInst *TI = B.getTerminator();
        if (TI) {
            mapBBTermInst[&B] = TI;
        }
        Instruction *prev = nullptr;
        for (Instruction &I : B)
        {
            if (IsIgnoreInst(&I))
            {
                continue;
            }
            if (isa<PHINode>(&I) || isa<LandingPadInst>(&I))
            {
                continue;
            }
            if (!FuncEntryInst)
            {
                FuncEntryInst = &I;
            }
            if (isa<ReturnInst>(&I))
            {
                setFuncExitInst.insert(&I);
            }
            else if (common::isUnreachableInst(&I) && prev)
            {
                setFuncExitInst.insert(prev);
            }

            MI.add(&I);

            prev = &I;
        }
    }
}

void InHouseHookPass::InstrumentEntryFunc(Function *EntryFunc)
{

    common::MonitoredRWInsts MI;
    Instruction *FuncEntryInst = nullptr;                 // call_before loc
    std::map<BasicBlock *, Instruction *> mapBBTermInst; // cost loc
    std::set<Instruction *> setFuncExitInst;              // return loc

    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*EntryFunc).getDomTree();
    CollectInstructions(EntryFunc, MI, FuncEntryInst, mapBBTermInst, setFuncExitInst);
    DEBUG(dbgs() << EntryFunc->getName() << ", " << mapBBTermInst.size() << ", " << setFuncExitInst.size() << "\n");
    if (!bNoOptInst) {
        removeByDomInfo(MI, DT);
    }
    NumRWInsts += MI.size();

    InstrumentInit(FuncEntryInst);

    unsigned FuncID = GetFunctionID(EntryFunc);
    InstrumentCallBefore(FuncID, FuncEntryInst);

    AllocaInst *BBAllocInst = nullptr;
    InstrumentCostAlloc(BBAllocInst, FuncEntryInst);

    // InstrumentParams(EntryFunc, BBAllocInst, FuncEntryInst);
    // InstrumentCostAdd(BBAllocInst, mapBBTermInst);
    InstrumentRW(MI);
    InstrumentReturn(BBAllocInst, setFuncExitInst);
    InstrumentFinal(setFuncExitInst);
}

void InHouseHookPass::InstrumentFunc(Function *F)
{

    common::MonitoredRWInsts MI;
    Instruction *FuncEntryInst = nullptr;                 // call_before loc
    std::map<BasicBlock *, Instruction *> mapBBTermInst; // cost loc
    std::set<Instruction *> setFuncExitInst;              // return loc

    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*F).getDomTree();
    CollectInstructions(F, MI, FuncEntryInst, mapBBTermInst, setFuncExitInst);
    DEBUG(dbgs() << F->getName() << ", " << mapBBTermInst.size() << ", " << setFuncExitInst.size() << "\n");
    if (!bNoOptInst) {
        removeByDomInfo(MI, DT);
    }
    NumRWInsts += MI.size();

    unsigned FuncID = GetFunctionID(F);
    InstrumentCallBefore(FuncID, FuncEntryInst);

    AllocaInst *BBAllocInst = nullptr;
    InstrumentCostAlloc(BBAllocInst, FuncEntryInst);

    // InstrumentParams(F, BBAllocInst, FuncEntryInst);
    // InstrumentCostAdd(BBAllocInst, mapBBTermInst);
    InstrumentRW(MI);
    InstrumentReturn(BBAllocInst, setFuncExitInst);
}

void InHouseHookPass::InstrumentInit(llvm::Instruction *InsertBefore)
{

    CallInst *pCall = CallInst::Create(this->aprof_init, "", InsertBefore);
    pCall->setCallingConv(CallingConv::C);
    pCall->setTailCall(false);
    AttributeList empty;
    pCall->setAttributes(empty);
    //        appendToGlobalCtors(*this->pModule, this->aprof_init, 0, nullptr);
}

void InHouseHookPass::InstrumentCallBefore(unsigned FuncID, llvm::Instruction *InsertBefore)
{

    std::vector<Value *> vecParams;
    ConstantInt *constFuncID = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(FuncID)), 10));
    vecParams.push_back(constFuncID);
    CallInst *pCall = CallInst::Create(this->aprof_call_before, vecParams, "", InsertBefore);
    pCall->setCallingConv(CallingConv::C);
    pCall->setTailCall(false);
    AttributeList empty;
    pCall->setAttributes(empty);
}

void InHouseHookPass::InstrumentCostAlloc(AllocaInst *&BBAllocInst, Instruction *InsertBefore)
{

    // Alloc cost and assign 0
    BBAllocInst = new AllocaInst(this->LongType, 0, "numCost", InsertBefore);
    BBAllocInst->setAlignment(8);
    StoreInst *pStore = new StoreInst(this->ConstantLong0, BBAllocInst, false, InsertBefore);
    pStore->setAlignment(8);
}

void InHouseHookPass::InstrumentParams(Function *F, AllocaInst *BBAllocInst, Instruction *InsertBefore)
{

    if (F->arg_size() > 0)
    {
        DataLayout *dl = new DataLayout(this->pModule);
        int accuRms = 0;
        for (Function::arg_iterator argIt = F->arg_begin(); argIt != F->arg_end(); argIt++)
        {
            Argument *argument = argIt;
            Type *argType = argument->getType();
            accuRms += dl->getTypeAllocSize(argType);
        }

        CastInst *pAllocAddr = new PtrToIntInst(BBAllocInst, this->LongType, "", InsertBefore);
        std::vector<Value *> params;
        ConstantInt *constRms = ConstantInt::get(this->pModule->getContext(), APInt(64, StringRef(std::to_string(accuRms)), 10));
        params.push_back(pAllocAddr);
        params.push_back(constRms);
        CallInst *pCall = CallInst::Create(this->aprof_read, params, "", InsertBefore);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        AttributeList empty;
        pCall->setAttributes(empty);
    }
}

void InHouseHookPass::InstrumentCostAdd(AllocaInst *BBAllocInst, std::map<BasicBlock *, Instruction *> &mapBBTermInst)
{

    Function *F = BBAllocInst->getFunction();
    if (!bNoOptCost && getExitBlockSize(F) == 1) {
        BBProfilingGraph bbGraph = BBProfilingGraph(*F);

        bbGraph.init();
        bbGraph.splitNotExitBlock();
        bbGraph.calculateSpanningTree();

        BBProfilingEdge *pQueryEdge = bbGraph.addQueryChord();
        bbGraph.calculateChordIncrements();
        NumCost += bbGraph.instrumentLocalCounterUpdate(BBAllocInst);
    } else {
        for (auto &BBTermInst : mapBBTermInst)
        {
            Instruction *TI = BBTermInst.second;
            LoadInst *pLoadNumCost = new LoadInst(BBAllocInst, "", false, TI);
            pLoadNumCost->setAlignment(8);
            BinaryOperator *pAdd = BinaryOperator::Create(Instruction::Add, pLoadNumCost, this->ConstantLong1, "add", TI);
            StoreInst *pStoreAdd = new StoreInst(pAdd, BBAllocInst, false, TI);
            pStoreAdd->setAlignment(8);
            ++NumCost;
        }
    }
}

void InHouseHookPass::InstrumentRW(common::MonitoredRWInsts &MI)
{

    for (auto &kv : MI.mapLoadID)
    {
        LoadInst *pLoad = kv.first;
        InstrumentLoad(pLoad, pLoad);
    }

    for (auto &kv : MI.mapStoreID)
    {
        StoreInst *pStore = kv.first;
        InstrumentStore(pStore, pStore);
    }

    for (auto &kv : MI.mapMemSetID)
    {
        MemSetInst *pMemSet = kv.first;
        InstrumentMemSet(pMemSet, pMemSet);
    }

    for (auto &kv : MI.mapMemTransferID)
    {
        MemTransferInst *pMemTransfer = kv.first;
        InstrumentMemTransfer(pMemTransfer, pMemTransfer);
    }

    for (auto &kv : MI.mapFgetcID)
    {
        Instruction *pCall = kv.first;
        InstrumentFgetc(pCall, pCall);
    }

    for (auto &kv : MI.mapFreadID)
    {
        Instruction *pCall = kv.first;
        InstrumentFread(pCall, pCall);
    }

    for (auto &kv : MI.mapOstreamID)
    {
        Instruction *pCall = kv.first;
        InstrumentOstream(pCall, pCall);
    }
}

void InHouseHookPass::InstrumentReturn(AllocaInst *BBAllocInst, std::set<Instruction *> &setFuncExitInst)
{

    for (Instruction *II : setFuncExitInst)
    {
        std::vector<Value *> vecParams;
        LoadInst *pLoad = new LoadInst(BBAllocInst, "", false, II);
        pLoad->setAlignment(8);
        vecParams.push_back(pLoad);

        CallInst *pCall = CallInst::Create(this->aprof_return, vecParams, "", II);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        AttributeList empty;
        pCall->setAttributes(empty);
    }
}

void InHouseHookPass::InstrumentFinal(std::set<Instruction *> &setFuncExitInst)
{

    for (Instruction *II : setFuncExitInst)
    {
        CallInst *pCall = CallInst::Create(this->aprof_final, "", II);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        AttributeList empty;
        pCall->setAttributes(empty);
    }
    //        appendToGlobalDtors(*pModule, this->aprof_final, 65535, nullptr);
}

void InHouseHookPass::InstrumentLoad(LoadInst *pLoad, Instruction *InsertBefore)
{

    assert(pLoad && InsertBefore);

    DataLayout *dl = new DataLayout(this->pModule);

    Value *addr = pLoad->getOperand(0);
    assert(addr);
    Type *type = addr->getType();
    assert(type);
    Type *type1 = type->getContainedType(0);
    assert(type1);
    assert(type1->isSized());

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(64, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);
    std::vector<Value *> params;
    params.push_back(int64_address);
    params.push_back(const_length);
    CallInst *pCall = CallInst::Create(this->aprof_read, params, "", InsertBefore);
    pCall->setCallingConv(CallingConv::C);
    pCall->setTailCall(false);
    AttributeList empty;
    pCall->setAttributes(empty);
}

void InHouseHookPass::InstrumentStore(StoreInst *pStore, Instruction *InsertBefore)
{

    assert(pStore && InsertBefore);

    DataLayout *dl = new DataLayout(this->pModule);

    Value *addr = pStore->getOperand(1);
    assert(addr);
    Type *type = addr->getType();
    assert(type);
    Type *type1 = type->getContainedType(0);
    assert(type1);
    assert(type1->isSized());

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(64, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);
    std::vector<Value *> params;
    params.push_back(int64_address);
    params.push_back(const_length);
    CallInst *pCall = CallInst::Create(this->aprof_write, params, "", InsertBefore);
    pCall->setCallingConv(CallingConv::C);
    pCall->setTailCall(false);
    AttributeList empty;
    pCall->setAttributes(empty);
}

void InHouseHookPass::InstrumentMemTransfer(MemTransferInst *pMemTransfer, Instruction *InsertBefore)
{

    assert(pMemTransfer && InsertBefore);

    Value *src = pMemTransfer->getSource();
    assert(src);
    Value *dest = pMemTransfer->getDest();
    assert(dest);
    Value *len = pMemTransfer->getLength();
    assert(len);

    {
        CastInst *src_int64_address = new PtrToIntInst(src, this->LongType, "", InsertBefore);
        std::vector<Value *> src_params;
        src_params.push_back(src_int64_address);
        src_params.push_back(len);
        CallInst *pReadCall = CallInst::Create(this->aprof_read, src_params, "", InsertBefore);
        pReadCall->setCallingConv(CallingConv::C);
        pReadCall->setTailCall(false);
        AttributeList src_attr;
        pReadCall->setAttributes(src_attr);
    }
    {
        CastInst *dst_int64_address = new PtrToIntInst(dest, this->LongType, "", InsertBefore);
        std::vector<Value *> dst_params;
        dst_params.push_back(dst_int64_address);
        dst_params.push_back(len);
        CallInst *pWriteCall = CallInst::Create(this->aprof_write, dst_params, "", InsertBefore);
        pWriteCall->setCallingConv(CallingConv::C);
        pWriteCall->setTailCall(false);
        AttributeList dst_attr;
        pWriteCall->setAttributes(dst_attr);
    }
}

void InHouseHookPass::InstrumentMemSet(MemSetInst *pMemSet, Instruction *InsertBefore)
{

    assert(pMemSet && InsertBefore);

    DataLayout *dl = new DataLayout(this->pModule);

    Value *dest = pMemSet->getDest();
    assert(dest);
    Value *len = pMemSet->getLength();
    assert(len);

    CastInst *int64_address = new PtrToIntInst(dest, this->LongType, "", InsertBefore);
    std::vector<Value *> params;
    params.push_back(int64_address);
    params.push_back(len);
    CallInst *pCall = CallInst::Create(this->aprof_write, params, "", InsertBefore);
    pCall->setCallingConv(CallingConv::C);
    pCall->setTailCall(false);
    AttributeList empty;
    pCall->setAttributes(empty);
}

void InHouseHookPass::InstrumentFgetc(Instruction *pCall, Instruction *InsertBefore)
{

    assert(pCall && InsertBefore);

    std::vector<Value *> params;
    params.push_back(this->ConstantLong1);
    CallInst *pIncrRMS = CallInst::Create(this->aprof_increment_rms, params, "", InsertBefore);
    pIncrRMS->setCallingConv(CallingConv::C);
    pIncrRMS->setTailCall(false);
    AttributeList empty;
    pIncrRMS->setAttributes(empty);
}

void InHouseHookPass::InstrumentFread(Instruction *pCall, Instruction *InsertAfter)
{

    assert(pCall && InsertAfter);

    std::vector<Value *> params;
    params.push_back(pCall);
    CallInst *pIncrRMS = CallInst::Create(this->aprof_increment_rms, params, "");
    pIncrRMS->insertAfter(InsertAfter);
    pIncrRMS->setCallingConv(CallingConv::C);
    pIncrRMS->setTailCall(false);
    AttributeList empty;
    pIncrRMS->setAttributes(empty);
}

void InHouseHookPass::InstrumentOstream(Instruction *pCall, Instruction *InsertBefore)
{

    assert(pCall && InsertBefore);

    DataLayout *dl = new DataLayout(this->pModule);

    CallSite cs(pCall);
    Value *pSecondArg = cs.getArgument(1);
    assert(pSecondArg);

    Type *type = pSecondArg->getType();
    assert(type);
    Type *type1 = type->getContainedType(0);
    assert(type1);

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(64, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(pSecondArg, this->LongType, "", InsertBefore);

    std::vector<Value *> params;
    params.push_back(int64_address);
    params.push_back(const_length);
    CallInst *pReadCall = CallInst::Create(this->aprof_read, params, "", InsertBefore);
    pReadCall->setCallingConv(CallingConv::C);
    pReadCall->setTailCall(false);
    AttributeList empty;
    pReadCall->setAttributes(empty);
}

void InHouseHookPass::FindDirectCallees(const std::set<BasicBlock *> &setBB, std::vector<Function *> &vecWorkList,
                                        std::set<Function *> &setToDo,
                                        std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping)
{

    for (BasicBlock *BB : setBB)
    {
        if (isa<UnreachableInst>(BB->getTerminator()))
        {
            continue;
        }

        for (Instruction &II : *BB)
        {
            Instruction *pInst = &II;

            if (Function *pCalled = common::getCalleeFunc(pInst))
            {
                if (pCalled->getName() == "JS_Assert")
                {
                    continue;
                }
                if (pCalled->begin() == pCalled->end())
                {
                    continue;
                }
                if (pCalled->getSection().str() == ".text.startup")
                {
                    continue;
                }
                if (!pCalled->isDeclaration())
                {
                    funcCallSiteMapping[pCalled].insert(pInst);

                    if (setToDo.find(pCalled) == setToDo.end())
                    {
                        setToDo.insert(pCalled);
                        vecWorkList.push_back(pCalled);
                    }
                }
            }
        }
    }
}

void InHouseHookPass::FindCalleesInDepth(const std::set<BasicBlock *> &setBB, std::set<Function *> &setToDo,
                                         std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping)
{

    // Function stack to recursively handle callees
    std::vector<Function *> vecWorkList;

    // Find in the outmost BBs
    FindDirectCallees(setBB, vecWorkList, setToDo, funcCallSiteMapping);

    while (!vecWorkList.empty())
    {

        Function *pCurrent = vecWorkList.back();
        vecWorkList.pop_back();

        // Store BBs in current func into set
        std::set<BasicBlock *> setCurrentBB;
        for (BasicBlock &BB : *pCurrent)
        {
            setCurrentBB.insert(&BB);
        }
        FindDirectCallees(setCurrentBB, vecWorkList, setToDo, funcCallSiteMapping);
    }
}

void InHouseHookPass::CloneFunctions(std::set<Function *> &setFunc, ValueToValueMapTy &originClonedMapping)
{

    for (Function *pOriginFunc : setFunc)
    {
        DEBUG(dbgs() << pOriginFunc->getName() << "\n");
        Function *pClonedFunc = CloneFunction(pOriginFunc, originClonedMapping, nullptr);
        pClonedFunc->setName(pOriginFunc->getName() + ".CPI");
        pClonedFunc->setLinkage(GlobalValue::InternalLinkage);

        originClonedMapping[pOriginFunc] = pClonedFunc;
    }

    for (Function *pOriginFunc : setFunc)
    {
        assert(originClonedMapping.find(pOriginFunc) != originClonedMapping.end());
    }
}

bool InHouseHookPass::RemapFunctionCalls(const std::set<Function *> &setFunc,
                                         std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping,
                                         ValueToValueMapTy &originClonedMapping)
{

    for (Function *pFunc : setFunc)
    {
        auto itFunc = originClonedMapping.find(pFunc);
        if (itFunc == originClonedMapping.end())
        {
            errs() << "Cannot find the original function in origin->cloned mapping " << pFunc->getName().str()
                   << "\n";
            return false;
        }

        Function *clonedFunc = cast<Function>(itFunc->second);
        assert(clonedFunc);

        auto itSetInst = funcCallSiteMapping.find(pFunc);
        if (itSetInst == funcCallSiteMapping.end())
        {
            errs() << "Cannot find the Instruction set of function " << pFunc->getName().str() << "\n";
            return false;
        }

        std::set<Instruction *> &setFuncInst = itSetInst->second;
        for (Instruction *pInst : setFuncInst)
        {
            auto itInst = originClonedMapping.find(pInst);
            if (itInst != originClonedMapping.end())
            {
                if (CallInst *pCall = dyn_cast<CallInst>(itInst->second))
                {
                    pCall->setCalledFunction(clonedFunc);
                }
                else if (InvokeInst *pInvoke = dyn_cast<InvokeInst>(itInst->second))
                {
                    pInvoke->setCalledFunction(clonedFunc);
                }
                else
                {
                    errs() << "Instruction is not CallInst or InvokeInst\n";
                    pInst->dump();
                }
            }
        }
    }

    return true;
}

} // namespace inhouse_hook_pass

static RegisterPass<inhouse_hook_pass::InHouseHookPass> X(
    "instrument-hooks",
    "Instrument InHouse Hooks",
    false,
    true);
