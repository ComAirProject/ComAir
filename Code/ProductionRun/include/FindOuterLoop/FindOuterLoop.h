#ifndef COMAIR_FINDOUTERLOOP_H
#define COMAIR_FINDOUTERLOOP_H

#include <fstream>

#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"


using namespace llvm;
using namespace std;


struct FindOuterLoop : public ModulePass {
    static char ID;
    Module *pModule;

    FindOuterLoop();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnModule(Module &M);

    void SetupInit();
    void CollectFunctions();
    void DumpFindFunctions(std::set<Function *> findFunc);
    std::set<Function *> FindCandidateFuncsByFuncType(FunctionType *ft);
    void Execute(std::map<std::string, long> FuncNameCostMap);
    Function* getTargetFunctionName(Value *callVal);

    /* global variables */
    std::vector<Function *> AllFuncs;

};

#endif //COMAIR_FINDOUTERLOOP_H
