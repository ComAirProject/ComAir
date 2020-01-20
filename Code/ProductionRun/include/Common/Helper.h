#ifndef PRODUCTIONRUN_HELPER_H
#define PRODUCTIONRUN_HELPER_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"

using namespace llvm;

unsigned GetFunctionID(Function *F);

unsigned GetBasicBlockID(BasicBlock *BB);

unsigned GetInstructionID(Instruction *II);

unsigned GetLoopID(Loop* loop);

int GetInstructionInsertFlag(Instruction *II);

bool getIgnoreOptimizedFlag(Function *F);

bool IsIgnoreFunc(Function *F);

bool IsIgnoreInst(Instruction *I);

bool IsClonedFunc(Function *F);

int GetBBCostNum(BasicBlock *BB);

unsigned long getExitBlockSize(Function *F);

bool hasUnifiedUnreachableBlock(Function *F);

bool IsRecursiveCall(std::string callerName, std::string calleeName);

bool IsRecursiveCall(Function *F);

std::string printSrcCodeInfo(Instruction *pInst);

std::string printSrcCodeInfo(Function *F);

std::string getFileNameForInstruction(Instruction *pInst);

std::string getClonedFunctionName(Module *M, std::string FuncName);

#endif //PRODUCTIONRUN_HELPER_H
