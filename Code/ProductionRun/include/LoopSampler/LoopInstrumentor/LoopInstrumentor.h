#ifndef PRODUCTIONRUN_INSTRUMENTOR_H
#define PRODUCTIONRUN_INSTRUMENTOR_H

#include <vector>
#include <set>

#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Constants.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <llvm/Analysis/AliasSetTracker.h>
#include "Common/MonitorRWInsts.h"

using namespace std;
using namespace llvm;

struct LoopInstrumentor : public ModulePass
{

    static char ID;

    LoopInstrumentor();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnModule(Module &M);

    void SetupInit(Module &M);

    void SetupTypes();

    void SetupStructs();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    void InlineSetRecord(Value *address, Value *length, Value *id, Instruction *InsertBefore);

    void InlineHookDelimit(Instruction *InsertBefore);

    void InlineHookLoad(LoadInst *pLoad, unsigned uID, Instruction *InsertBefore);

    void InlineHookStore(StoreInst *pStore, unsigned uID, Instruction *InsertBefore);

    void InlineHookMemSet(MemSetInst *pMemSet, unsigned uID, Instruction *InsertBefore);

    void InlineHookMemTransfer(MemTransferInst *pMemTransfer, unsigned uID, Instruction *InsertBefore);

    void InlineHookFgetc(Instruction *pCall, unsigned uID, Instruction *InsertBefore);

    void InlineHookFread(Instruction *pCall, unsigned uID, Instruction *InsertBefore);

    void InlineHookOstream(Instruction *pCall, unsigned uID, Instruction *InsertBefore);

    void InstrumentHoistMonitoredInsts(MonitoredRWInsts &MI, Instruction *InsertBefore);

    void InstrumentMonitoredInsts(MonitoredRWInsts &MI);

    void FindCalleesInDepth(const std::set<BasicBlock *> &setBB, std::set<Function *> &setToDo,
                            std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping);

    void FindDirectCallees(const std::set<BasicBlock *> &setBB, std::vector<Function *> &vecWorkList,
                           std::set<Function *> &setToDo,
                           std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping);

    void InlineGlobalCostForLoop(std::set<BasicBlock *> &setBBInLoop, bool NoOptCost);

    void InlineGlobalCostForCallee(Function *pFunction, bool NoOptCost);

    void InlineOutputCost(Instruction *InsertBefore);

    void InstrumentMain(StringRef funcName);

private:
    Module *pModule;
    // Type
    Type *VoidType;
    IntegerType *LongType;
    IntegerType *IntType;
    IntegerType *CharType;
    PointerType *CharStarType;
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
    ConstantInt *ConstantLong1;
    ConstantInt *ConstantInt0;
    ConstantInt *ConstantIntN1;
    ConstantInt *ConstantInt1;
    ConstantInt *ConstantInt2;
    ConstantInt *ConstantInt3;
    ConstantInt *ConstantInt4;
    ConstantInt *ConstantInt5;
    ConstantInt *ConstantDelimit;
    ConstantInt *ConstantLoopBegin;
    ConstantInt *ConstantLoopEnd;
    ConstantInt *ConstantLong16;
    ConstantPointerNull *ConstantNULL;
    Constant *SAMPLE_RATE_ptr;
};

#endif //PRODUCTIONRUN_INSTRUMENTOR_H
