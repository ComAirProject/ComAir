#ifndef COMAIR_APROFHOOKPASS_H
#define COMAIR_APROFHOOKPASS_H

#include <fstream>

#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"


using namespace llvm;
using namespace std;


struct AprofHook : public ModulePass {
    static char ID;

    AprofHook();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnModule(Module &M);

    void SetupInit();

    void SetupTypes();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    void SetupHooks();

    void InstrumentInit(Instruction *);

    void InstrumentHooks(Function *, bool isOptimized = true);

    void InstrumentWrite(StoreInst *, Instruction *);

    void InstrumentRead(LoadInst *, Instruction *);

    void InstrumentAlloc(Value *, Instruction *);

    void InstrumentCallBefore(Function *pFunction);

    void InstrumentReturn(Instruction *m);

    void InstrumentFinal(Function *mainFunc);

    void InstrumentCostUpdater(Function *pFunction, bool isOptimized = true);

    void InstrumentRmsUpdater(Function *pFunction);

    void InstrumentRmsUpdater(Function *Callee, Instruction *pInst);

    void ProcessMemIntrinsic(MemIntrinsic *memInst);

    /* Module */
    Module *pModule;
    /* ********** */

    /* Type */
    IntegerType *IntType;
    IntegerType *LongType;
    Type *VoidType;
    PointerType *VoidPointerType;
    /* ********** */

    /* Function */
    // int aprof_init()
    Function *aprof_init;
    // void aprof_increment_cost()
    Function *aprof_increment_cost;
    // void aprof_increment_rms
    Function *aprof_increment_rms;
    // void aprof_increment_rms
    Function *aprof_increment_rms_for_args;
    // void aprof_write(void *memory_addr, unsigned int length)
    Function *aprof_write;
    // void aprof_read(void *memory_addr, unsigned int length)
    Function *aprof_read;
    // void aprof_call_before(char *funcName)
    Function *aprof_call_before;
    // void aprof_return()
    Function *aprof_return;
    // void aprof_final()
    Function *aprof_final;
    /* ********** */

    /* Constant */
    ConstantInt *ConstantLong0;
    ConstantInt *ConstantLong1;
    /* ********** */

    /* Alloc Inst */
    AllocaInst *BBAllocInst;
    AllocaInst *RmsAllocInst;
    /* **** */

    /* OutPut File */
    ofstream funNameIDFile;
    /* */

};

#endif //COMAIR_APROFHOOKPASS_H
