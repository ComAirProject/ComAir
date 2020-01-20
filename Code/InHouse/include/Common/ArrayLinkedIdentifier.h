#ifndef INHOUSE_ARRAYLINKEDIDENTIFIER_H
#define INHOUSE_ARRAYLINKEDIDENTIFIER_H

#include <set>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace std;

Function *SearchFunctionByName(Module &M, string &strFileName, string &strFunctionName, unsigned uSourceCodeLine);

void SearchBasicBlocksByLineNo(Function *F, unsigned uLineNo, vector<BasicBlock *> &vecBasicBlocks);

Loop *SearchLoopByLineNo(Function *pFunction, LoopInfo *pLI, unsigned uLineNo);

bool isOneStarLoop(Loop *pLoop, set<BasicBlock *> &setBlocks);

bool IsArrayAccessLoop1(Loop *pLoop, set<Value *> &setArrayValue);

bool IsArrayAccessLoop(Loop *pLoop, set<Value *> &setArrayValue);

bool IsLinkedListAccessLoop(Loop *pLoop, set<Value *> &setLinkedValue);

void FindArrayIndexAndData(Instruction *Inst);

#endif //INHOUSE_ARRAYLINKEDIDENTIFIER_H
