#ifndef PRODUCTIONRUN_LOOPBASEINSTRUMENTOR_H
#define PRODUCTIONRUN_LOOPBASEINSTRUMENTOR_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <set>
#include "Common/MonitoredInsts.h"

namespace loopsampler
{
    struct LoopBaseInstrumentor : public llvm::ModulePass
    {
        static char ID;

        LoopBaseInstrumentor();

        virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

        virtual bool runOnModule(llvm::Module &M);

        void SetupInit(llvm::Module &M);

        void SetupTypes();

        void SetupStructs();

        void SetupConstants();

        void SetupGlobals();

        void SetupFunctions();

        void InlineSetRecord(llvm::Value *address, llvm::Value *length, llvm::Value *id, llvm::Instruction *InsertBefore);

        void InlineHookDelimit(llvm::Instruction *InsertBefore);

        void InlineHookLoad(llvm::LoadInst *pLoad, unsigned uID, llvm::Instruction *InsertBefore);

        void InlineHookStore(llvm::StoreInst *pStore, unsigned uID, llvm::Instruction *InsertBefore);

        void InlineHookMemSet(llvm::MemSetInst *pMemSet, unsigned uID, llvm::Instruction *InsertBefore);

        void InlineHookMemTransfer(llvm::MemTransferInst *pMemTransfer, unsigned uID, llvm::Instruction *InsertBefore);

        void InlineHookFgetc(llvm::Instruction *pCall, unsigned uID, llvm::Instruction *InsertBefore);

        void InlineHookFread(llvm::Instruction *pCall, unsigned uID, llvm::Instruction *InsertBefore);

        void InlineHookOstream(llvm::Instruction *pCall, unsigned uID, llvm::Instruction *InsertBefore);

        void InstrumentMonitoredInsts(common::MonitoredRWInsts &MI);

        void FindCalleesInDepth(const std::set<llvm::BasicBlock *> &setBB, std::set<llvm::Function *> &setToDo,
                                std::map<llvm::Function *, std::set<llvm::Instruction *>> &funcCallSiteMapping);

        void FindDirectCallees(const std::set<llvm::BasicBlock *> &setBB, std::vector<llvm::Function *> &vecWorkList,
                               std::set<llvm::Function *> &setToDo,
                               std::map<llvm::Function *, std::set<llvm::Instruction *>> &funcCallSiteMapping);

        virtual void InlineGlobalCostForLoop(std::set<llvm::BasicBlock *> &setBBInLoop);

        virtual void InlineGlobalCostForCallee(llvm::Function *pFunction);

        void InlineOutputCost(llvm::Instruction *InsertBefore);

        void InstrumentMain(llvm::StringRef funcName);

    protected:
        llvm::Module *pModule;
        // Type
        llvm::Type *VoidType;
        llvm::IntegerType *LongType;
        llvm::IntegerType *IntType;
        llvm::IntegerType *CharType;
        llvm::PointerType *CharStarType;
        llvm::StructType *struct_stMemRecord;

        // Global Variable
        llvm::GlobalVariable *SAMPLE_RATE;
        llvm::GlobalVariable *numGlobalCounter;
        llvm::GlobalVariable *numGlobalCost;
        llvm::GlobalVariable *Record_CPI;
        llvm::GlobalVariable *pcBuffer_CPI;
        llvm::GlobalVariable *iBufferIndex_CPI;

        // Function
        // sample_rate_str = getenv("SAMPLE_RATE");
        llvm::Function *getenv;
        // sample_rate = atoi(sample_rate_str)
        llvm::Function *function_atoi;
        // random_gen
        llvm::Function *geo;
        // Init shared memory at the entry of main function.
        llvm::Function *InitMemHooks;
        // Finalize shared memory at the return/exit of main function.
        llvm::Function *FinalizeMemHooks;

        // Constant
        llvm::ConstantInt *ConstantLong0;
        llvm::ConstantInt *ConstantLong1;
        llvm::ConstantInt *ConstantInt0;
        llvm::ConstantInt *ConstantIntN1;
        llvm::ConstantInt *ConstantInt1;
        llvm::ConstantInt *ConstantInt2;
        llvm::ConstantInt *ConstantInt3;
        llvm::ConstantInt *ConstantInt4;
        llvm::ConstantInt *ConstantInt5;
        llvm::ConstantInt *ConstantDelimit;
        llvm::ConstantInt *ConstantLoopBegin;
        llvm::ConstantInt *ConstantLoopEnd;
        llvm::ConstantInt *ConstantLoopStride;
        llvm::ConstantInt *ConstantLoopNegativeStride;
        llvm::ConstantInt *ConstantLong16;
        llvm::ConstantPointerNull *ConstantNULL;
        llvm::Constant *SAMPLE_RATE_ptr;
    };
} // namespace loopsampler
#endif //PRODUCTIONRUN_LOOPBASEINSTRUMENTOR_H