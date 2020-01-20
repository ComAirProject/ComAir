#ifndef NEWCOMAIR_RECURSIVESAMPLER_RECURSIVEINSTRUMENTOR_H
#define NEWCOMAIR_RECURSIVESAMPLER_RECURSIVEINSTRUMENTOR_H

#include <vector>
#include <set>

#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Constants.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include "Common/LocateInstrument.h"

using namespace llvm;

struct RecursiveInstrumentor : public ModulePass {

    static char ID;

    RecursiveInstrumentor();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnModule(Module &M);

private:

    void SetupInit(Module &M);

    // Setup
    void SetupTypes();

    void SetupStructs();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    // Instrument
    void InstrumentMain();

    void InstrumentRecursiveFunction(Function *pRecursiveFunc);

    // Helper
    // bool ReadIndvarStride(const char *filePath, VecIndvarInstIDStrideTy &vecIndvarNameStride);

    bool SearchToBeInstrumented(Function *pRecursiveFunc, std::vector<Instruction *> &vecToInstrument);
//
    void CloneFunctionCalled(std::set<BasicBlock *> &setBlocksInRecursiveFunc, ValueToValueMapTy &VCalleeMap,
                             std::map<Function *, std::set<Instruction *> > &FuncCallSiteMapping);

    void CreateIfElseBlock(Function *pRecursiveFunc, std::vector<BasicBlock *> &vecAdded, ValueToValueMapTy &VMap);

    void CreateIfElseIfBlock(Function *pRecursiveFunc, std::vector<BasicBlock *> &vecAdded);

    void CloneRecursiveFunction();

    // copy operands and incoming values from old Inst to new Inst
    void RemapInstruction(Instruction *I, ValueToValueMapTy &VMap);

    // Instrument InlineHookLoad and InlineHookStore
    // void InstrumentRecordMemHooks(std::set<Instruction *> vecToInstrumentCloned);

    // Inline instrument
    void InstrumentRmsUpdater(Function *F);

    void InlineNumGlobalCost(Function *pFunction);

    void InlineSetRecord(Value *address, Value *length, Value *flag, Instruction *InsertBefore);

    void InlineHookDelimit(Instruction *InsertBefore);

    void InlineHookLoad(Value *addr, ConstantInt *const_length, Instruction *InsertBefore);

    void InlineHookStore(Value *addr, Type *type1, Instruction *InsertBefore);

    void InlineHookLoad(Value *addr, Type *type1, Instruction *InsertBefore);

    void InlineOutputCost(Instruction *InsertBefore);

    void InstrumentReturn(Function *Func);

    void InstrumentNewReturn(Function *Func);

    void InlineHookMem(MemTransferInst * pMem, Instruction *II);

    void InlineHookIOFunc(Function *F, Instruction *II);

    // Module
    Module *pModule;
    std::set<std::uint64_t> setInstID;

    // Type
    Type *VoidType;
    IntegerType *LongType;
    IntegerType *IntType;
    IntegerType *CharType;
    PointerType *CharStarType;
    PointerType *VoidPointerType;
    StructType *struct_stMemRecord;

    // Global Variable
    GlobalVariable *SAMPLE_RATE;
    GlobalVariable *numGlobalCounter;
    GlobalVariable *numGlobalCost;
    GlobalVariable *Record_CPI;
    GlobalVariable *pcBuffer_CPI;
    GlobalVariable *iBufferIndex_CPI;

    // Function
    // sample_rate_str = getenv("SAMPLE_RATE");
    Function *getenv;
    // sample_rate = atoi(sample_rate_str)
    Function *function_atoi;
    // random_gen
    Function *geo;
    // Init shared memory at the entry of main function.
    Function *InitMemHooks;
    // Finalize shared memory at the return/exit of main function.
    Function *FinalizeMemHooks;

    // Constant
    ConstantInt *ConstantLong0;
    ConstantInt *ConstantInt0;  // end
    ConstantInt *ConstantIntN1;
    ConstantInt *ConstantInt1;  // delimit
    ConstantInt *ConstantInt2;  // load
    ConstantInt *ConstantInt3;  // store
    ConstantInt *ConstantInt4;  // loop begin
    ConstantInt *ConstantInt5;  // loop end
    ConstantInt *ConstantLong16;
    ConstantInt *ConstantLong1;
    ConstantInt *ConstantLongN1;
    ConstantPointerNull *ConstantNULL;
    Constant *SAMPLE_RATE_ptr;

    Function *myNewF;

};


#endif //NEWCOMAIR_RECURSIVESAMPLER_RECURSIVEINSTRUMENTOR_H
