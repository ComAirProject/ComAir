#ifndef PRODUCTIONRUN_LOOPCLASSIFIER_H
#define PRODUCTIONRUN_LOOPCLASSIFIER_H

/*
 * LoopClassifier:
 * Classify Loop into Array and LinkedList
 * If Loop is Array, then provide the Induction Variable and Stride
 */

#include <map>
#include <vector>
#include "llvm/Pass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Instruction.h"

struct InstSCEV {
    const llvm::Instruction *pInst;
    const llvm::SCEV *pSCEV;
};

struct LoopClassifier : public llvm::ModulePass {

    static char ID;

    LoopClassifier();

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    virtual bool runOnModule(llvm::Module &M);

private:

    std::vector<InstSCEV> searchInstSCEV(llvm::Loop *pLoop, llvm::ScalarEvolution &SE);

    const llvm::SCEV *searchStride(const llvm::SCEV *scev, llvm::ScalarEvolution &SE);

};

#endif //PRODUCTIONRUN_LOOPCLASSIFIER_H
