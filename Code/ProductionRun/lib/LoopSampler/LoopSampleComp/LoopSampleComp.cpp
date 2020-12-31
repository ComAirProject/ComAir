#include "LoopSampler/LoopSampleComp/LoopSampleComp.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace loopsampler
{
    LoopSampleComp::LoopSampleComp(GlobalVariable *SAMPLE_RATE_,
                                   GlobalVariable *numGlobalCounter_,
                                   Function *geo_,
                                   ConstantInt *ConstantInt1_,
                                   ConstantInt *ConstantIntN1_) : SAMPLE_RATE(SAMPLE_RATE_),
                                                                  numGlobalCounter(numGlobalCounter_),
                                                                  geo(geo_),
                                                                  ConstantInt1(ConstantInt1_),
                                                                  ConstantIntN1(ConstantIntN1_)
    {
    }

    BasicBlock *LoopSampleComp::CloneInnerLoop(Loop *pLoop, ValueToValueMapTy &VMap)
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
                                std::set<Instruction *> setTmp;
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

        return cast<BasicBlock>(VMap[pLoop->getHeader()]);
        // Insert shim BB between PreExit and Exit (To store Loop End Indvar)
    }

    BasicBlock *LoopSampleComp::CreateIfElseBlock(Loop *pInnerLoop, BasicBlock *pClonedLoopHeader)
    {
        /*
         * if (counter > 1) {               // condition1: original preheader
         *     --counter;                   // unsampled, if.body: new preheader
         *     cloned loop                  // goto cloned loop header
         * } else {                         // condition1: orignal preheader
         *     counter = gen_random();      // sampled, else.body: new preheader
         *     instrument delimiter         //
         *     instrumented loop            // goto instrumented loop header
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

        pIfBody = BasicBlock::Create(pModule->getContext(), ".if.body", pInnerFunction, nullptr);
        pElseBody = BasicBlock::Create(pModule->getContext(), ".else.body", pInnerFunction, nullptr);

        pTerminator = pCondition1->getTerminator();

        /*
         * Append to condition1:
         *  if (counter > 1) {
         *    goto ifBody;  // unsampled
         *  } else {
         *    goto elseBody;  // sampled
         *  }
         */
        {
            pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pTerminator);
            pLoad1->setAlignment(4);
            pCmp = new ICmpInst(pTerminator, ICmpInst::ICMP_SGT, pLoad1, this->ConstantInt1, "gt1");
            pBranch = BranchInst::Create(pIfBody, pElseBody, pCmp);
            ReplaceInstWithInst(pTerminator, pBranch);
        }

        /*
         * Append to ifBody:
         * --counter;
         * goto cloned loop header;
        */
        {
            pLoad1 = new LoadInst(this->numGlobalCounter, "", false, pIfBody);
            pLoad1->setAlignment(4);
            pBinary = BinaryOperator::Create(Instruction::Add, pLoad1, this->ConstantIntN1, "dec1", pIfBody);
            pStore = new StoreInst(pBinary, this->numGlobalCounter, false, pIfBody);
            pStore->setAlignment(4);

            BranchInst::Create(pClonedLoopHeader, pIfBody);
        }

        /*
         * Append to elseBody:
         *  counter = gen_random();
         *  instrument delimiter
         *  goto original loop header (to be instrumented);
         */
        {
            pLoad2 = new LoadInst(this->SAMPLE_RATE, "", false, 4, pElseBody);
            pLoad2->setAlignment(4);
            pCall = CallInst::Create(this->geo, pLoad2, "", pElseBody);
            pCall->setCallingConv(CallingConv::C);
            pCall->setTailCall(false);
            pCall->setAttributes(emptySet);
            pStore = new StoreInst(pCall, this->numGlobalCounter, false, 4, pElseBody);
            pStore->setAlignment(4);

            pBranch = BranchInst::Create(pHeader, pElseBody);
            // InlineHookDelimit(pBranch);
        }

        return pElseBody;
    }
    
    void LoopSampleComp::RemapInstruction(Instruction *I, ValueToValueMapTy &VMap)
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

    void LoopSampleComp::CloneFunctions(std::set<Function *> &setFunc, ValueToValueMapTy &originClonedMapping)
    {
        for (Function *pOriginFunc : setFunc)
        {
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

    bool LoopSampleComp::RemapFunctionCalls(const std::set<Function *> &setFunc,
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
                else
                {
                    errs() << "Cannot find Cloned CallSite Instruction\n";
                    pInst->dump();
                }
            }
        }

        return true;
    }
} // namespace loopsampler