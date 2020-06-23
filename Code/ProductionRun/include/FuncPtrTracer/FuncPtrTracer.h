/**
 * Trace function pointer.
 * Output: /dev/shm/func_ptr_tracer.log
 * Format:
 * |long     |unsigned int          |unsigned int|
 * |thread_id|CallInst/InvokeInst ID|Function ID |
 * Context: After applying IDAssigner
 */

#ifndef PRODUCTIONRUN_FUNCPTRTRACER_H
#define PRODUCTIONRUN_FUNCPTRTRACER_H

#include "llvm/Pass.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"

namespace func_ptr_tracer {

    struct FuncPtrTracer : public llvm::ModulePass {

        static char ID;

        FuncPtrTracer();

        void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

        bool runOnModule(llvm::Module &M) override;

        // setup
        void SetupInit();

        void SetupTypes();

        void SetupFunctions();

        // instrument
        void InstrumentInit(llvm::Instruction *InsertBefore);

        void InstrumentEnterFunc(unsigned funcID, llvm::Instruction *InsertBefore);

        void InstrumentExitFunc(unsigned funcID, llvm::Instruction *InsertBefore);

        void InstrumentCallInst(unsigned instID, llvm::Instruction *InsertBefore);

        void InstrumentFinal(llvm::Instruction *InsertBefore);

    private:

        llvm::Module *pModule;

        // types
        llvm::IntegerType *IntType;
        llvm::IntegerType *LongType;
        llvm::Type *VoidType;
        llvm::PointerType *VoidPointerType;

        // func ptr tracer hooks
        // void func_ptr_hook_init();
        llvm::Function *func_ptr_hook_init;
        // void func_ptr_hook_enter_func(int threadId, int funcId);
        llvm::Function *func_ptr_hook_enter_func;
        // void func_ptr_hook_exit_func(int threadId, int funcId);
        llvm::Function *func_ptr_hook_exit_func;
        // void func_ptr_hook_call_inst(int threadId, int instId);
        llvm::Function *func_ptr_hook_call_inst;
        // void func_ptr_hook_final();
        llvm::Function *func_ptr_hook_final;
    };
}

#endif //PRODUCTIONRUN_FUNCPTRTRACER_H
