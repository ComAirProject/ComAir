#ifndef PRODUCTIONRUN_IDTAGGER_H
#define PRODUCTIONRUN_IDTAGGER_H

#include <vector>

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"

struct IDTagger: public llvm::ModulePass {

    static char ID;
    IDTagger();

    std::vector<llvm::Loop *> AllLoops;

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
    virtual bool runOnModule(llvm::Module &M);

    void tagLoops(llvm::Module &M);
};

#endif //PRODUCTIONRUN_IDTAGGER_H
