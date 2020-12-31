#ifndef PRODUCTIONRUN_SEARCHHELPER_H
#define PRODUCTIONRUN_SEARCHHELPER_H

#include "Common/SearchHelper.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include <map>
#include <set>
#include <utility>
#include "Common/IDHelper.h"

using namespace llvm;

namespace common
{
    Function *SearchFunctionByName(Module &M, const char *FileName, const char *FunctionName, unsigned LineNo)
    {
        for (Function &F : M)
        {
            if (!F.getName().contains(FunctionName))
            {
                continue;
            }
            // FileName: <StartLine, EndLine>
            std::map<StringRef, std::pair<unsigned, unsigned>> FunctionBounds;
            for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
            {
                const DILocation *DIL = I->getDebugLoc();
                if (!DIL)
                {
                    continue;
                }
                StringRef CurrFileName = DIL->getFilename();
                unsigned CurrLineNo = DIL->getLine();
                auto It = FunctionBounds.find(CurrFileName);
                if (It == FunctionBounds.end())
                {
                    FunctionBounds[CurrFileName] = std::make_pair(CurrLineNo, CurrLineNo);
                }
                else
                {
                    if (It->second.first > CurrLineNo)
                    {
                        It->second.first = CurrLineNo;
                    }
                    if (It->second.second < CurrLineNo)
                    {
                        It->second.second = CurrLineNo;
                    }
                }
            }
            for (const auto &kv : FunctionBounds)
            {
                if (!kv.first.contains(FileName))
                {
                    continue;
                }
                if (kv.second.first <= LineNo && LineNo <= kv.second.second)
                {
                    return &F;
                }
            }
        }
        return nullptr;
    }

    void SearchBasicBlocksByLineNo(Function *F, unsigned LineNo, std::set<BasicBlock *> &BBs)
    {
        for (BasicBlock &B : *F)
        {
            for (Instruction &I : B)
            {
                const DILocation *DIL = I.getDebugLoc();
                if (DIL != NULL && LineNo == DIL->getLine())
                {
                    BBs.insert(&B);
                    break;
                }
            }
        }
    }

    Loop *SearchLoopByLineNo(Function *F, LoopInfo *LPI, unsigned LineNo)
    {
        std::set<BasicBlock *> BBs;
        SearchBasicBlocksByLineNo(F, LineNo, BBs);

        unsigned Depth = 0;
        BasicBlock *TargetBB = nullptr;
        for (BasicBlock *BB : BBs)
        {
            if (LPI->getLoopDepth(BB) > Depth)
            {
                Depth = LPI->getLoopDepth(BB);
                TargetBB = BB;
            }
        }

        if (!TargetBB)
        {
            std::set<Loop *> Loops;
            for (BasicBlock &B : *F)
            {
                if (LPI->getLoopDepth(&B) > 0)
                {
                    Loops.insert(LPI->getLoopFor(&B));
                }
            }
            if (Loops.size() == 1)
            {
                return *(Loops.begin());
            }
            return nullptr;
        }

        return LPI->getLoopFor(TargetBB);
    }

    Instruction *SearchInstructionByID(std::set<BasicBlock *> &BBs, unsigned InstID)
    {
        for (BasicBlock *B : BBs)
        {
            for (Instruction &I : *B)
            {
                if (GetInstructionID(&I) == InstID)
                {
                    return &I;
                }
            }
        }
        return nullptr;
    }
} // namespace common

#endif //PRODUCTIONRUN_SEARCHHELPER_H