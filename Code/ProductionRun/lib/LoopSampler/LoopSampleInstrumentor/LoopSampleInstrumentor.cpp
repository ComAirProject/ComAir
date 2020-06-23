#include "LoopSampler/LoopSampleInstrumentor/LoopSampleInstrumentor.h"

#include <fstream>
#include <vector>
#include <map>
#include <set>

#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ADT/Statistic.h"

#include "Common/ArrayLinkedIdentifier.h"
#include "Common/Helper.h"
#include "Common/MonitorRWInsts.h"
#include "Common/LoadStoreMem.h"
#include "Common/BBProfiling.h"
#include "Common/Constant.h"

using namespace llvm;
using std::map;
using std::set;
using std::vector;

#define DEBUG_TYPE "LoopSampleInstrumentor"

// #ifndef GLOBAL_SAMPLED_COUNTER
// #define GLOBAL_SAMPLED_COUNTER
// #endif

STATISTIC(NumNoOptRW, "Num of Non-optimized Read/Write Instrument Sites");
STATISTIC(NumOptRW, "Num of Optimized Read/Write Instrument Sites");
STATISTIC(NumHoistRW, "Num of Hoisted Read/Write Instrument Sites");
STATISTIC(NumNoOptCost, "Num of Non-optimized Cost Instrument Sites");
STATISTIC(NumOptCost, "Num of Optimized Cost Instrument Sites");

static RegisterPass<LoopSampleInstrumentor> X("opt-loop-instrument",
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

static cl::opt<std::string> strEntryFuncName("strEntryFunc",
                                             cl::desc("Entry Function Name"), cl::Optional,
                                             cl::value_desc("strEntryFuncName"));

static cl::opt<bool> bNoOptInst("bNoOptInst", cl::desc("No Opt for Num of Instrument Sites"), cl::Optional,
                                cl::value_desc("bNoOptInst"));

static cl::opt<bool> bNoOptCost("bNoOptCost", cl::desc("No Opt for Merging Cost"), cl::Optional,
                                cl::value_desc("bNoOptCost"));

static cl::opt<bool> bNoOptLoop("bNoOptLoop", cl::desc("No Opt for Loop"), cl::Optional, cl::value_desc("bNoOptLoop"));

char LoopSampleInstrumentor::ID = 0;

LoopSampleInstrumentor::LoopSampleInstrumentor() : ModulePass(ID)
{
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeDominatorTreeWrapperPassPass(Registry);
    initializeLoopInfoWrapperPassPass(Registry);
}

void LoopSampleInstrumentor::getAnalysisUsage(AnalysisUsage &AU) const
{
    AU.setPreservesAll();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
}

static void removeByDomInfo(MonitoredRWInsts &MI, DominatorTree &DT)
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
                                MI.mapLoadID.erase(LI);
                            }
                            else if (StoreInst *SI = dyn_cast<StoreInst>(rhs))
                            {
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

void LoopSampleInstrumentor::CreateIfElseBlock(Loop *pInnerLoop, BasicBlock *pClonedLoopHeader, BasicBlock *&pLoopPreheader)
{
    /*
     * if (counter != 0 || sampled > 100) { // condition1: original preheader
     *     counter--;                   // unsampled, if.body: new preheader
     *     original loop                // goto original loop header
     * } else {                         // condition1: orignal preheader
     *     sampled++;
     *     counter = gen_random();      // sampled, else.body: new preheader
     *     instrument delimiter         //
     *     instrumented cloned loop     // goto instrumented cloned loop header
     * }
     */

    BasicBlock *pCondition1 = nullptr;

    BasicBlock *pIfBody = nullptr;
#ifdef GLOBAL_SAMPLED_COUNTER
    BasicBlock *pLOR = nullptr;
#endif
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

    pIfBody = BasicBlock::Create(pModule->getContext(), ".if.body", pInnerFunction, nullptr);
#ifdef GLOBAL_SAMPLED_COUNTER
    pLOR = BasicBlock::Create(pModule->getContext(), ".lor.lhs.false", pInnerFunction, nullptr);
#endif
    pElseBody = BasicBlock::Create(pModule->getContext(), ".else.body", pInnerFunction, nullptr);

    pTerminator = pCondition1->getTerminator();

    /*
    * Append to condition1:
    *  if (counter != 0) {
    *    goto ifBody;  // unsampled
    *  } else {
    *    goto LOR;  // sampled
    *  }
    */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pTerminator);
        pLoad1->setAlignment(4);
        pCmp = new ICmpInst(pTerminator, ICmpInst::ICMP_NE, pLoad1, this->ConstantInt0, "cmp0");
#ifdef GLOBAL_SAMPLED_COUNTER
        pBranch = BranchInst::Create(pIfBody, pLOR, pCmp);
#else
        pBranch = BranchInst::Create(pIfBody, pElseBody, pCmp);
#endif
        ReplaceInstWithInst(pTerminator, pBranch);
    }

#ifdef GLOBAL_SAMPLED_COUNTER
    /*
    * Append to LOR
    * if (sampled > 100) {
    *    goto ifBody;  // unsampled
    * } else {
    *    goto elseBody;  // sampled
    * }
    */
    {
        LoadInst *pLoadSampled = new LoadInst(this->numGlobalSampledCounter, "", false, pLOR);
        pLoadSampled->setAlignment(8);
        pCmp = new ICmpInst(ICmpInst::ICMP_SGT, pLoadSampled, this->ConstantLong100, "cmp1");
        pBranch = BranchInst::Create(pIfBody, pElseBody, pCmp, pLOR);
        pCmp->insertBefore(pBranch);
    }
#endif
    /*
     * Append to ifBody:
     * counter--;
     * goto original loop header;
    */
    {
        pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pIfBody);
        pLoad1->setAlignment(4);
        pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1", pIfBody);
        pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pIfBody);
        pStore->setAlignment(4);

        BranchInst::Create(pHeader, pIfBody);
    }

    /*
     * Append to elseBody:
     *  sampled++;
     *  counter = gen_random();
     *  instrument delimiter
     *  goto cloned loop header (to be instrumented);
     */
    {
#ifdef GLOBAL_SAMPLED_COUNTER
        pLoad2 = new LoadInst(this->numGlobalSampledCounter, "", false, 8, pElseBody);
        pLoad2->setAlignment(8);
        pBinary = BinaryOperator::Create(Instruction::Add, pLoad2, this->ConstantLong1, "inc1", pElseBody);
        pStore = new StoreInst(pBinary, this->numGlobalSampledCounter, false, pElseBody);
#endif
        pLoad2 = new LoadInst(this->SAMPLE_RATE, "", false, 4, pElseBody);
        pLoad2->setAlignment(4);
        pCall = CallInst::Create(this->geo, pLoad2, "", pElseBody);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptySet);
        pStore = new StoreInst(pCall, this->numGlobalCounter, false, 4, pElseBody);
        pStore->setAlignment(4);

        pBranch = BranchInst::Create(pClonedLoopHeader, pElseBody);
        InlineHookDelimit(pBranch);
    }

    pLoopPreheader = pElseBody;
}

void LoopSampleInstrumentor::CloneInnerLoop(Loop *pLoop, ValueToValueMapTy &VMap, BasicBlock *&pClonedLoopHeader)
{

    Function *pFunction = pLoop->getHeader()->getParent();

    std::vector<BasicBlock *> ClonedBBs;

    std::vector<BasicBlock *> WorkList;
    std::set<BasicBlock *> Visited;

    SmallVector<BasicBlock *, 4> ExitBlocks;
    pLoop->getExitBlocks(ExitBlocks);

    // After LoopSimplify, an ExitBB only has predecessors from inside Loop
    std::set<BasicBlock *> PreExitBlocks;
    for (BasicBlock *BB : ExitBlocks)
    {
        for (auto it = pred_begin(BB), et = pred_end(BB); it != et; ++it)
        {
            PreExitBlocks.insert(*it);
        }
    }

    // WorkList stop propagation at ExitBB
    for (unsigned long i = 0; i < ExitBlocks.size(); ++i)
    {
        VMap[ExitBlocks[i]] = ExitBlocks[i];
        Visited.insert(ExitBlocks[i]);
    }

    WorkList.push_back(pLoop->getHeader());

    while (!WorkList.empty())
    {
        BasicBlock *pCurrent = WorkList.back();
        WorkList.pop_back();

        if (Visited.find(pCurrent) != Visited.end())
        {
            continue;
        }

        BasicBlock *NewBB = BasicBlock::Create(pCurrent->getContext(), "", pFunction);
        if (pCurrent->hasName())
        {
            NewBB->setName(pCurrent->getName() + ".CPI");
            ClonedBBs.push_back(NewBB);
        }

        if (pCurrent->hasAddressTaken())
        {
            errs() << "hasAddressTaken branch\n";
            exit(1);
        }

        for (auto &II : *pCurrent)
        {
            Instruction *Inst = &II;
            Instruction *NewInst = Inst->clone();
            if (Inst->hasName())
            {
                NewInst->setName(Inst->getName() + ".CPI");
            }
            VMap[Inst] = NewInst;
            NewBB->getInstList().push_back(NewInst);
        }

        const TerminatorInst *TI = pCurrent->getTerminator();
        for (unsigned i = 0, e = TI->getNumSuccessors(); i != e; ++i)
        {
            WorkList.push_back(TI->getSuccessor(i));
        }

        ClonedBBs.push_back(NewBB);
        VMap[pCurrent] = NewBB;
        Visited.insert(pCurrent);
    }

    // Remap values defined and used inside loop
    for (BasicBlock *ClonedBB : ClonedBBs)
    {
        for (Instruction &II : *ClonedBB)
        {
            this->RemapInstruction(&II, VMap);
        }
    }

    std::set<BasicBlock *> setExitBBs;
    for (BasicBlock *EB : ExitBlocks)
    {
        setExitBBs.insert(EB);
    }

    // Add PHI Nodes incoming from ClonedBBs in ExitBBs
    for (BasicBlock *EB : setExitBBs)
    {
        for (Instruction &I : *EB)
        {
            PHINode *pPHI = dyn_cast<PHINode>(&I);
            if (!pPHI)
            {
                continue;
            }
            for (unsigned long i = 0, e = pPHI->getNumIncomingValues(); i < e; ++i)
            {
                BasicBlock *incomingBB = pPHI->getIncomingBlock(i);
                if (VMap.find(incomingBB) == VMap.end())
                {
                    continue;
                }
                Value *incomingValue = pPHI->getIncomingValue(i);
                if (VMap.find(incomingValue) != VMap.end())
                {
                    incomingValue = VMap[incomingValue];
                }
                pPHI->addIncoming(incomingValue, cast<BasicBlock>(VMap[incomingBB]));
            }
        }
    }

    // Replace Instructions used outside Loop with PHI Nodes
    // 1. Collect Instructions defined in Loop but used outside Loop
    std::map<Instruction *, std::set<Instruction *>> mapAddPHI;
    for (Loop::block_iterator BB = pLoop->block_begin(); BB != pLoop->block_end(); ++BB)
    {
        BasicBlock *B = *BB;
        for (BasicBlock::iterator II = B->begin(); II != B->end(); ++II)
        {
            Instruction *I = &*II;
            for (Instruction::user_iterator UU = I->user_begin(); UU != I->user_end(); ++UU)
            {
                if (Instruction *IU = dyn_cast<Instruction>(*UU))
                {
                    if (!pLoop->contains(IU->getParent()))
                    {
                        if (mapAddPHI.find(I) == mapAddPHI.end())
                        {
                            set<Instruction *> setTmp;
                            mapAddPHI[I] = setTmp;
                        }

                        mapAddPHI[I].insert(IU);
                    }
                }
            }
        }
    }

    // 2. If they are used in ExitBBs, create PHINode in ExitBBs to replace them
    for (auto &kv : mapAddPHI)
    {
        Instruction *I = kv.first;
        if (VMap.find(I) == VMap.end())
        {
            continue;
        }

        for (Instruction *UI : kv.second)
        {
            // We have handled PHINode previously.
            if (isa<PHINode>(UI))
            {
                continue;
            }
            auto itExitBB = setExitBBs.find(UI->getParent());
            if (itExitBB == setExitBBs.end())
            {
                continue;
            }
            BasicBlock *pInsert = *itExitBB;

            // Assume Instruction is only defined in one PreExitBB (To be verified)
            BasicBlock *PB = nullptr;
            for (pred_iterator PI = pred_begin(pInsert), PE = pred_end(pInsert); PI != PE; ++PI)
            {
                BasicBlock *BB = *PI;
                if (pLoop->contains(BB))
                {
                    PB = BB;
                    break;
                }
            }

            assert(PB);

            PHINode *pPHI = PHINode::Create(I->getType(), 2, ".Created.PHI", pInsert->getFirstNonPHI());

            pPHI->addIncoming(I, PB);
            Instruction *ClonedI = cast<Instruction>(VMap[I]);
            BasicBlock *ClonedPB = cast<BasicBlock>(VMap[PB]);
            pPHI->addIncoming(ClonedI, ClonedPB);

            // Replace Uses of Instructions with PHINodes
            for (Instruction *IU : kv.second)
            {
                for (unsigned i = 0, e = IU->getNumOperands(); i < e; ++i)
                {
                    if (IU->getOperand(i) == I)
                    {
                        IU->setOperand(i, pPHI);
                    }
                }
            }
        }
    }

    pClonedLoopHeader = cast<BasicBlock>(VMap[pLoop->getHeader()]);
    // Insert shim BB between PreExit and Exit (To store Loop End Indvar)
}

void LoopSampleInstrumentor::RemapInstruction(Instruction *I, ValueToValueMapTy &VMap)
{
    for (unsigned op = 0, E = I->getNumOperands(); op != E; ++op)
    {
        Value *Op = I->getOperand(op);
        ValueToValueMapTy::iterator It = VMap.find(Op);
        if (It != VMap.end())
        {
            I->setOperand(op, It->second);
        }
    }

    if (auto PN = dyn_cast<PHINode>(I))
    {
        for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i)
        {
            ValueToValueMapTy::iterator It = VMap.find(PN->getIncomingBlock(i));
            if (It != VMap.end())
            {
                PN->setIncomingBlock(i, cast<BasicBlock>(It->second));
            }
        }
    }
}

bool LoopSampleInstrumentor::runOnModule(Module &M)
{
    SetupInit(M);

    Function *pFunction = SearchFunctionByName(M, strFileName, strFuncName, uSrcLine);
    if (!pFunction)
    {
        errs() << "Cannot find the function\n";
        return false;
    }

    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*pFunction).getDomTree();
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();

    Loop *pLoop = SearchLoopByLineNo(pFunction, &LI, uSrcLine);
    if (!pLoop)
    {
        errs() << "Cannot find the loop\n";
        return false;
    }

    set<BasicBlock *> setBBInLoop;
    for (BasicBlock *BB : pLoop->blocks())
    {
        setBBInLoop.insert(BB);
    }

    MonitoredRWInsts MI;
    for (BasicBlock *BB : setBBInLoop)
    {
        ++NumNoOptCost;
        for (Instruction &II : *BB)
        {
            MI.add(&II);
        }
    }

    NumNoOptRW += MI.size();

    if (!bNoOptInst)
    {
        removeByDomInfo(MI, DT);
    }

    MonitoredRWInsts HoistMI;

    if (!bNoOptInst)
    {
        for (auto &kv : MI.mapLoadID)
        {
            if (pLoop->isLoopInvariant(kv.first->getPointerOperand()))
            {
                HoistMI.mapLoadID[kv.first] = kv.second;
            }
        }
        for (auto &kv : MI.mapStoreID)
        {
            if (pLoop->isLoopInvariant(kv.first->getPointerOperand()))
            {
                HoistMI.mapStoreID[kv.first] = kv.second;
            }
        }

        // After Alias+Dominance checking and removing,
        // if operands of a LoadInst and a StoreInst are still Alias/equal,
        // then their sequence is unknown, thus should not be hoisted.
        for (auto &kvLoad : HoistMI.mapLoadID)
        {
            LoadInst *pLoad = kvLoad.first;
            for (auto &kvStore : HoistMI.mapStoreID)
            {
                StoreInst *pStore = kvStore.first;
                if (pLoad->getPointerOperand() == pStore->getPointerOperand())
                {
                    HoistMI.mapLoadID.erase(pLoad);
                    HoistMI.mapStoreID.erase(pStore);
                }
            }
        }

        NumHoistRW = HoistMI.size();
        MI.diff(HoistMI);
        NumOptRW += MI.size();
    }

    ValueToValueMapTy VMap;
    BasicBlock *pClonedLoopHeader = nullptr;
    CloneInnerLoop(pLoop, VMap, pClonedLoopHeader);
    assert(pClonedLoopHeader);

    BasicBlock *pLoopPreheader = nullptr;
    CreateIfElseBlock(pLoop, pClonedLoopHeader, pLoopPreheader);
    assert(pLoopPreheader);

    MonitoredRWInsts ClonedMI;
    mapFromOriginToCloned(VMap, MI, ClonedMI);
    // Instrument Loops
    //InstrumentMonitoredInsts(ClonedMI);

    if (!bNoOptInst)
    {
        MonitoredRWInsts ClonedHoistMI;
        mapFromOriginToCloned(VMap, HoistMI, ClonedHoistMI);
        Instruction *pTerm = pLoopPreheader->getTerminator();
    //    InstrumentHoistMonitoredInsts(ClonedHoistMI, pTerm);
    }

    std::set<BasicBlock *> setBBInClonedLoop;
    for (BasicBlock *B : setBBInLoop)
    {
        setBBInClonedLoop.insert(cast<BasicBlock>(VMap[B]));
    }
    InlineGlobalCostForLoop(setBBInClonedLoop, bNoOptCost);

    // Find Callees
    vector<Function *> vecWorkList;
    set<Function *> setCallees;
    ValueToValueMapTy VCalleeMap;
    std::map<Function *, std::set<Instruction *>> funcCallSiteMapping;
    FindCalleesInDepth(setBBInLoop, setCallees, funcCallSiteMapping);
    CloneFunctions(setCallees, VCalleeMap);
    RemapFunctionCalls(setCallees, funcCallSiteMapping, VCalleeMap);

    // Instrument Callees
    for (Function *Callee : setCallees)
    {
        DominatorTree &CalleeDT = getAnalysis<DominatorTreeWrapperPass>(*Callee).getDomTree();
        MonitoredRWInsts CalleeMI;
        for (BasicBlock &BB : *Callee)
        {
            ++NumNoOptCost;
            for (Instruction &II : BB)
            {
                CalleeMI.add(&II);
            }
        }
        NumNoOptRW += CalleeMI.size();
        if (!bNoOptInst)
        {
            removeByDomInfo(CalleeMI, CalleeDT);
        }
        NumOptRW += CalleeMI.size();
        // Instrument RW
        MonitoredRWInsts ClonedCalleeMI;
        mapFromOriginToCloned(VCalleeMap, CalleeMI, ClonedCalleeMI);
        //InstrumentMonitoredInsts(ClonedCalleeMI);
        Function *ClonedCallee = cast<Function>(VCalleeMap[Callee]);
        InlineGlobalCostForCallee(ClonedCallee, bNoOptCost);
    }

    if (strEntryFuncName != "")
    {
        InstrumentMain(strEntryFuncName);
    }
    else
    {
        InstrumentMain("main");
    }

    errs() << "Orig RW:" << NumNoOptRW << ", InPlace:" << NumOptRW << ", Hoist:" << NumHoistRW << "\n";
    errs() << "Orig Cost:" << NumNoOptCost << ", Opt Cost:" << NumOptCost << "\n";
    return true;
}

void LoopSampleInstrumentor::SetupInit(Module &M)
{
    this->pModule = &M;
    SetupTypes();
    SetupStructs();
    SetupConstants();
    SetupGlobals();
    SetupFunctions();
}

void LoopSampleInstrumentor::SetupTypes()
{
    this->VoidType = Type::getVoidTy(pModule->getContext());
    this->LongType = IntegerType::get(pModule->getContext(), 64);
    this->IntType = IntegerType::get(pModule->getContext(), 32);
    this->CharType = IntegerType::get(pModule->getContext(), 8);
    this->CharStarType = PointerType::get(this->CharType, 0);
}

void LoopSampleInstrumentor::SetupStructs()
{
    vector<Type *> struct_fields;

    assert(pModule->getTypeByName("struct.stMemRecord") == nullptr);
    this->struct_stMemRecord = StructType::create(pModule->getContext(), "struct.stMemRecord");
    struct_fields.clear();
    struct_fields.push_back(this->LongType); // address
    struct_fields.push_back(this->IntType);  // length
    struct_fields.push_back(this->IntType);  // id (+: Read, -: Write, Special:DELIMIT, LOOP_BEGIN, LOOP_END)
    if (this->struct_stMemRecord->isOpaque())
    {
        this->struct_stMemRecord->setBody(struct_fields, false);
    }
}

void LoopSampleInstrumentor::SetupConstants()
{
    // long: 0, 1, 16, 100
    this->ConstantLong0 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("0"), 10));
    this->ConstantLong1 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("1"), 10));
    this->ConstantLong16 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("16"), 10));
    this->ConstantLong100 = ConstantInt::get(pModule->getContext(), APInt(64, StringRef("100"), 10));

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

void LoopSampleInstrumentor::SetupGlobals()
{
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
    this->numGlobalCost = new GlobalVariable(*pModule, this->LongType, false, GlobalValue::ExternalLinkage, nullptr,
                                             "numGlobalCost");
    this->numGlobalCost->setAlignment(8);
    this->numGlobalCost->setInitializer(this->ConstantLong0);

#ifdef GLOBAL_SAMPLED_COUNTER
    // long numGlobalSampledCounter = 0;
    assert(pModule->getGlobalVariable("numGlobalSampledCounter") == nullptr);
    this->numGlobalSampledCounter = new GlobalVariable(*pModule, this->LongType, false, GlobalValue::ExternalLinkage, nullptr,
                                                       "numGlobalSampledCounter");
    this->numGlobalSampledCounter->setAlignment(8);
    this->numGlobalSampledCounter->setInitializer(this->ConstantLong0);
#endif
}

void LoopSampleInstrumentor::SetupFunctions()
{

    vector<Type *> ArgTypes;

    // getenv
    this->getenv = pModule->getFunction("getenv");
    if (!this->getenv)
    {
        ArgTypes.push_back(this->CharStarType);
        FunctionType *getenv_FuncTy = FunctionType::get(this->CharStarType, ArgTypes, false);
        this->getenv = Function::Create(getenv_FuncTy, GlobalValue::ExternalLinkage, "getenv", pModule);
        this->getenv->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // atoi
    this->function_atoi = pModule->getFunction("atoi");
    if (!this->function_atoi)
    {
        ArgTypes.clear();
        ArgTypes.push_back(this->CharStarType);
        FunctionType *atoi_FuncTy = FunctionType::get(this->IntType, ArgTypes, false);
        this->function_atoi = Function::Create(atoi_FuncTy, GlobalValue::ExternalLinkage, "atoi", pModule);
        this->function_atoi->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // geo
    this->geo = this->pModule->getFunction("geo");
    if (!this->geo)
    {
        ArgTypes.push_back(this->IntType);
        FunctionType *Geo_FuncTy = FunctionType::get(this->IntType, ArgTypes, false);
        this->geo = Function::Create(Geo_FuncTy, GlobalValue::ExternalLinkage, "geo", this->pModule);
        this->geo->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // InitMemHooks
    this->InitMemHooks = this->pModule->getFunction("InitMemHooks");
    if (!this->InitMemHooks)
    {
        FunctionType *InitHooks_FuncTy = FunctionType::get(this->CharStarType, ArgTypes, false);
        this->InitMemHooks = Function::Create(InitHooks_FuncTy, GlobalValue::ExternalLinkage, "InitMemHooks",
                                              this->pModule);
        this->InitMemHooks->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }

    // FinalizeMemHooks
    this->FinalizeMemHooks = this->pModule->getFunction("FinalizeMemHooks");
    if (!this->FinalizeMemHooks)
    {
        ArgTypes.push_back(this->LongType);
        FunctionType *FinalizeMemHooks_FuncTy = FunctionType::get(this->VoidType, ArgTypes, false);
        this->FinalizeMemHooks = Function::Create(FinalizeMemHooks_FuncTy, GlobalValue::ExternalLinkage,
                                                  "FinalizeMemHooks", this->pModule);
        this->FinalizeMemHooks->setCallingConv(CallingConv::C);
        ArgTypes.clear();
    }
}

void LoopSampleInstrumentor::InlineSetRecord(Value *address, Value *length, Value *id, Instruction *InsertBefore)
{

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

void LoopSampleInstrumentor::InlineHookDelimit(Instruction *InsertBefore)
{

    InlineSetRecord(this->ConstantLong0, this->ConstantInt0, this->ConstantDelimit, InsertBefore);
}

void LoopSampleInstrumentor::InlineHookLoad(LoadInst *pLoad, unsigned uID, Instruction *InsertBefore)
{

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

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}

void LoopSampleInstrumentor::InlineHookStore(StoreInst *pStore, unsigned uID, Instruction *InsertBefore)
{

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

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(addr, this->LongType, "", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}

void LoopSampleInstrumentor::InlineHookMemSet(MemSetInst *pMemSet, unsigned uID, Instruction *InsertBefore)
{

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

void LoopSampleInstrumentor::InlineHookMemTransfer(MemTransferInst *pMemTransfer, unsigned uID, Instruction *InsertBefore)
{

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

void LoopSampleInstrumentor::InlineHookFgetc(Instruction *pCall, unsigned uID, Instruction *InsertBefore)
{

    assert(pCall && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    int ID = uID;

    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(ConstantLong0, ConstantInt1, const_id, InsertBefore);
}

void LoopSampleInstrumentor::InlineHookFread(Instruction *pCall, unsigned uID, Instruction *InsertBefore)
{

    assert(pCall && InsertBefore);
    assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
    assert(InsertBefore == pCall->getNextNode());
    int ID = uID;

    CastInst *length = new TruncInst(pCall, this->IntType, "fread_len_conv", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(ConstantLong0, length, const_id, InsertBefore);
}

void LoopSampleInstrumentor::InlineHookOstream(Instruction *pCall, unsigned uID, Instruction *InsertBefore)
{

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

    ConstantInt *const_length = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
    CastInst *int64_address = new PtrToIntInst(pSecondArg, this->LongType, "", InsertBefore);
    ConstantInt *const_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(ID)), 10));
    InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}

void LoopSampleInstrumentor::InstrumentHoistMonitoredInsts(MonitoredRWInsts &MI, Instruction *InsertBefore)
{

    for (auto &kv : MI.mapLoadID)
    {
        LoadInst *pLoad = kv.first;
        unsigned uID = kv.second;
        InlineHookLoad(pLoad, uID, InsertBefore);
    }

    for (auto &kv : MI.mapStoreID)
    {
        StoreInst *pStore = kv.first;
        unsigned uID = kv.second;
        InlineHookStore(pStore, uID, InsertBefore);
    }
}

void LoopSampleInstrumentor::InstrumentMonitoredInsts(MonitoredRWInsts &MI)
{

    for (auto &kv : MI.mapLoadID)
    {
        LoadInst *pLoad = kv.first;
        unsigned uID = kv.second;
        InlineHookLoad(pLoad, uID, pLoad);
    }

    for (auto &kv : MI.mapStoreID)
    {
        StoreInst *pStore = kv.first;
        unsigned uID = kv.second;
        InlineHookStore(pStore, uID, pStore);
    }

    for (auto &kv : MI.mapMemSetID)
    {
        MemSetInst *pMemSet = kv.first;
        unsigned uID = kv.second;
        InlineHookMemSet(pMemSet, uID, pMemSet);
    }

    for (auto &kv : MI.mapMemTransferID)
    {
        MemTransferInst *pMemTransfer = kv.first;
        unsigned uID = kv.second;
        InlineHookMemTransfer(pMemTransfer, uID, pMemTransfer);
    }

    for (auto &kv : MI.mapFgetcID)
    {
        Instruction *pCall = kv.first;
        unsigned uID = kv.second;
        InlineHookFgetc(pCall, uID, pCall);
    }

    for (auto &kv : MI.mapFreadID)
    {
        Instruction *pCall = kv.first;
        unsigned uID = kv.second;
        InlineHookFread(pCall, uID, pCall->getNextNode());
    }

    for (auto &kv : MI.mapOstreamID)
    {
        Instruction *pCall = kv.first;
        unsigned uID = kv.second;
        InlineHookOstream(pCall, uID, pCall);
    }
}

void LoopSampleInstrumentor::FindDirectCallees(const std::set<BasicBlock *> &setBB, std::vector<Function *> &vecWorkList,
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

            if (Function *pCalled = getCalleeFunc(pInst))
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

void LoopSampleInstrumentor::FindCalleesInDepth(const std::set<BasicBlock *> &setBB, std::set<Function *> &setToDo,
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

void LoopSampleInstrumentor::InlineGlobalCostForLoop(std::set<BasicBlock *> &setBBInLoop, bool NoOptCost)
{
    Function *pFunction = NULL;

    set<BasicBlock *>::iterator itSetBegin = setBBInLoop.begin();
    set<BasicBlock *>::iterator itSetEnd = setBBInLoop.end();

    assert(itSetBegin != itSetEnd);

    if (NoOptCost)
    {
        for (BasicBlock *B : setBBInLoop)
        {
            TerminatorInst *TI = B->getTerminator();
            if (!TI)
            {
                continue;
            }
            LoadInst *pLoad = new LoadInst(this->numGlobalCost, "numGlobalCost", TI);
            BinaryOperator *pBinary = BinaryOperator::Create(Instruction::Add, pLoad, this->ConstantLong1, "numGlobalCost++", TI);
            StoreInst *pStore = new StoreInst(pBinary, this->numGlobalCost, false, TI);
            pStore->setAlignment(8);
        }
        return;
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

    NumOptCost += bbGraph.instrumentLocalCounterUpdate(numLocalCounter, this->numGlobalCost);
}

void LoopSampleInstrumentor::InlineGlobalCostForCallee(Function *pFunction, bool NoOptCost)
{
    if (pFunction->getName() == "JS_Assert")
    {
        return;
    }

    if (pFunction->begin() == pFunction->end())
    {
        return;
    }

    if (NoOptCost)
    {
        for (BasicBlock &B : *pFunction)
        {
            TerminatorInst *TI = B.getTerminator();
            if (!TI)
            {
                continue;
            }
            LoadInst *pLoad = new LoadInst(this->numGlobalCost, "numGlobalCost", TI);
            BinaryOperator *pBinary = BinaryOperator::Create(Instruction::Add, pLoad, this->ConstantInt1, "numGlobalCost++", TI);
            StoreInst *pStore = new StoreInst(pBinary, this->numGlobalCost, false, TI);
            pStore->setAlignment(8);
        }
        return;
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

    NumOptCost += bbGraph.instrumentLocalCounterUpdate(numLocalCounter, this->numGlobalCost);
}

void LoopSampleInstrumentor::InlineOutputCost(Instruction *InsertBefore)
{

    auto pLoad = new LoadInst(this->numGlobalCost, "", false, InsertBefore);
    InlineSetRecord(pLoad, this->ConstantInt0, this->ConstantInt0, InsertBefore);
}

void LoopSampleInstrumentor::InstrumentMain(StringRef funcName)
{

    AttributeList emptyList;

    Function *pFunctionMain = this->pModule->getFunction(funcName);

    if (!pFunctionMain)
    {
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

    for (auto &BB : *pFunctionMain)
    {
        for (auto &II : BB)
        {
            Instruction *pInst = &II;
            // Instrument FinalizeMemHooks before return/unreachable/exit...
            if (isa<ReturnInst>(pInst) || isExitInst(pInst))
            {
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

void LoopSampleInstrumentor::CloneFunctions(std::set<Function *> &setFunc, ValueToValueMapTy &originClonedMapping)
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

bool LoopSampleInstrumentor::RemapFunctionCalls(const std::set<Function *> &setFunc,
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

bool LoopSampleInstrumentor::CloneRemapCallees(const std::set<BasicBlock *> &setBB, std::set<Function *> &setCallee,
                                               ValueToValueMapTy &originClonedMapping, std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping)
{

    FindCalleesInDepth(setBB, setCallee, funcCallSiteMapping);

    DEBUG(dbgs() << "# of Functions to be cloned: " << setCallee.size() << "\n");

    CloneFunctions(setCallee, originClonedMapping);

    DEBUG(dbgs() << "# of Functions cloned: " << funcCallSiteMapping.size() << "\n");

    return RemapFunctionCalls(setCallee, funcCallSiteMapping, originClonedMapping);
}
