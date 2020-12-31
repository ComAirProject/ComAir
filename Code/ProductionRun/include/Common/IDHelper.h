#ifndef PRODUCTIONRUN_IDHELPER_H
#define PRODUCTIONRUN_IDHELPER_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"

namespace common
{
    using llvm::BasicBlock;
    using llvm::Function;
    using llvm::Instruction;
    using llvm::Loop;
    /// Get Function ID
    unsigned GetFunctionID(Function *F);
    /// Get BasicBlock ID
    unsigned GetBasicBlockID(BasicBlock *B);
    /// Get Instruction ID
    unsigned GetInstructionID(Instruction *I);
    /// Get Loop ID
    unsigned GetLoopID(Loop *L);
} // namespace common

#endif //PRODUCTIONRUN_IDHELPER_H