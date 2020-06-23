/// LoopClassifier: Classify Loop into Array and LinkedList.
/// If Loop is Array, then provide the Induction Variable and Stride.
/// If Loop is LinkedList, then provide only the Induction Variable.
#ifndef PRODUCTIONRUN_LOOPCLASSIFIER_H
#define PRODUCTIONRUN_LOOPCLASSIFIER_H

#include "llvm/Pass.h"

struct LoopClassifier : public llvm::ModulePass {

    static char ID;

    LoopClassifier();

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    virtual bool runOnModule(llvm::Module &M);
};

#endif //PRODUCTIONRUN_LOOPCLASSIFIER_H
