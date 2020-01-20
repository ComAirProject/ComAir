#ifndef PRODUCTIONRUN_FUNCPTRREPLACER_H
#define PRODUCTIONRUN_FUNCPTRREPLACER_H

#include "llvm/Pass.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"

namespace func_ptr_replacer {

    struct FuncPtrReplacer : public llvm::ModulePass {

        static char ID;

        FuncPtrReplacer();

        void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

        bool runOnModule(llvm::Module &M) override;

//        // setup
//        void SetupInit();
//
//        void SetupTypes();
//
//        void SetupFunctions();
//
//        // instrument
//        void InstrumentInit(llvm::Instruction *InsertBefore);
//
//        void InstrumentEnterFunc(unsigned funcID, llvm::Instruction *InsertBefore);
//
//        void InstrumentExitFunc(unsigned funcID, llvm::Instruction *InsertBefore);
//
//        void InstrumentCallInst(unsigned instID, llvm::Instruction *InsertBefore);
//
//        void InstrumentFinal(llvm::Instruction *InsertBefore);

    private:

        llvm::Module *pModule;

        // types
        llvm::IntegerType *IntType;
        llvm::IntegerType *LongType;
        llvm::Type *VoidType;
        llvm::PointerType *VoidPointerType;
    };
}

#endif //PRODUCTIONRUN_FUNCPTRREPLACER_H
