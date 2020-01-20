#ifndef COMAIR_OPTIMIZE_H
#define COMAIR_OPTIMIZE_H

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"


using namespace llvm;

struct OptimizeManager : public ModulePass {
    static char ID;

    OptimizeManager();
    llvm::legacy::PassManager PM;
    llvm::PassBuilder PB;

    void SetUpOptimizePasses();
    void AddPass(Pass *p);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &M);

};


#endif //COMAIR_OPTIMIZE_H
