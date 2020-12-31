#ifndef PRODUCTIONRUN_SEARCHHELPER_H
#define PRODUCTIONRUN_SEARCHHELPER_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include <set>

namespace common
{
    using llvm::BasicBlock;
    using llvm::Function;
    using llvm::Instruction;
    using llvm::Loop;
    using llvm::LoopInfo;
    using llvm::Module;

    /// Search Function by Function name, FileName, and LineNo
    Function *SearchFunctionByName(Module &M, const char *FileName, const char *FunctionName, unsigned LineNo);

    /// Search BasicBlocks by LineNo
    void SearchBasicBlocksByLineNo(Function *F, unsigned LineNo, std::set<BasicBlock *> &BBs);

    /// Search Loop by LineNo
    Loop *SearchLoopByLineNo(Function *F, LoopInfo *LPI, unsigned LineNo);

    /// Search Instructioin by ID
    Instruction *SearchInstructionByID(std::set<BasicBlock *> &BBs, unsigned InstID);
} // namespace common

#endif //PRODUCTIONRUN_SEARCHHELPER_H