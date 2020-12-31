#ifndef PRODUCTIONRUN_LOOPSAMPLECOMP_H
#define PRODUCTIONRUN_LOOPSAMPLECOMP_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <map>
#include <set>

namespace loopsampler
{
    class LoopSampleComp
    {
    public:
        LoopSampleComp(llvm::GlobalVariable *SAMPLE_RATE_,
                       llvm::GlobalVariable *numGlobalCounter_,
                       llvm::Function *geo_,
                       llvm::ConstantInt *ConstantInt1_,
                       llvm::ConstantInt *ConstantIntN1_);

        /// Clone Inner Loop and return the cloned loop header
        llvm::BasicBlock *CloneInnerLoop(llvm::Loop *pLoop, llvm::ValueToValueMapTy &VMap);

        /// Create If-Else blocks and return Preheader of the instrumented loop (Else block)
        llvm::BasicBlock *CreateIfElseBlock(llvm::Loop *pInnerLoop, llvm::BasicBlock *pClonedLoopHeader);

        void RemapInstruction(llvm::Instruction *I, llvm::ValueToValueMapTy &VMap);

        void CloneFunctions(std::set<llvm::Function *> &setFunc, llvm::ValueToValueMapTy &originClonedMapping);

        bool RemapFunctionCalls(const std::set<llvm::Function *> &setFunc,
                                std::map<llvm::Function *, std::set<llvm::Instruction *>> &funcCallSiteMapping,
                                llvm::ValueToValueMapTy &originClonedMapping);

    private:
        // Global variable
        llvm::GlobalVariable *SAMPLE_RATE;
        llvm::GlobalVariable *numGlobalCounter;
        // Function
        llvm::Function *geo;
        // Constant
        llvm::ConstantInt *ConstantInt1;
        llvm::ConstantInt *ConstantIntN1;
    };
} // namespace loopsampler

#endif //PRODUCTIONRUN_LOOPSAMPLECOMP_H