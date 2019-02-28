#ifndef APROF_APROFHOOKPASS_H
#define APROF_APROFHOOKPASS_H

#include <fstream>
#include <llvm/Pass.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>

namespace Aprof {

    using std::ofstream;
    using namespace llvm;

    struct AprofHookPass : public ModulePass {

        static char ID;

        AprofHookPass();

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

    private:

        Module *pModule;

        IntegerType *IntType;
        IntegerType *LongType;
        Type *VoidType;
        PointerType *VoidPointerType;

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

        ConstantInt *ConstantLong0;
        ConstantInt *ConstantLong1;

        AllocaInst *BBAllocInst;
        AllocaInst *RmsAllocInst;

        ofstream funNameIDFile;
    };
}

#endif //APROF_APROFHOOKPASS_H
