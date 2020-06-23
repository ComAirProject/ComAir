#ifndef PRODUCTIONRUN_PREANALYZER_H
#define PRODUCTIONRUN_PREANALYZER_H

#include <vector>
#include <set>

#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Constants.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

using namespace std;
using namespace llvm;

struct PreAnalyzer : public ModulePass {

    static char ID;

    PreAnalyzer();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnModule(Module &M);
};

#endif //PRODUCTIONRUN_PREANALYZER_H
