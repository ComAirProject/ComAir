#ifndef NEWCOMAIR_COMMON_SEARCH_H
#define NEWCOMAIR_COMMON_SEARCH_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"

#include <vector>
#include <string>

using llvm::Function;
using llvm::BasicBlock;
using llvm::Module;
using llvm::LoopInfo;
using llvm::Loop;
using std::vector;
using std::string;

BasicBlock *SearchBlockByName(Function *pFunction, string sName);

Function *SearchFunctionByName(Module &M, string strFunctionName);

Function *SearchFunctionByName(Module &M, string &strFileName, string &strFunctionName, unsigned uSourceCodeLine);

void SearchBasicBlocksByLineNo(Function *F, unsigned uLineNo, vector<BasicBlock *> &vecBasicBlocks);

Loop *SearchLoopByLineNo(Function *pFunction, LoopInfo *pLI, unsigned uLineNo);

#endif //NEWCOMAIR_COMMON_SEARCH_H
