#ifndef PRODUCTIONRUN_RECURSIVESAMPLEINSTRUMENTOR_H
#define PRODUCTIONRUN_RECURSIVESAMPLEINSTRUMENTOR_H

#include <set>
#include <map>

#include "llvm/Pass.h"
#include "Common/MonitorRWInsts.h"

struct RecursiveSampleInstrumentor : public llvm::ModulePass
{

    static char ID;

    RecursiveSampleInstrumentor();

    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const;

    virtual bool runOnModule(llvm::Module &M);

    void SetupInit(llvm::Module &M);

    // Setup
    void SetupTypes();

    void SetupStructs();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    // Instrument

    void InstrumentMain(StringRef funcName);

    void FindDirectCallees(const std::set<llvm::BasicBlock *> &setBB, std::vector<llvm::Function *> &vecWorkList,
                           std::set<llvm::Function *> &setToDo,
                           std::map<llvm::Function *, std::set<Instruction *>> &funcCallSiteMapping);

    void FindCalleesInDepth(const std::set<llvm::BasicBlock *> &setBB, std::set<llvm::Function *> &setToDo,
                            std::map<llvm::Function *, std::set<Instruction *>> &funcCallSiteMapping);

    void InlineGlobalCostForCallee(llvm::Function *pFunction, bool NoOptCost);

    void InlineSetRecord(Value *address, Value *length, Value *id, Instruction *InsertBefore);

    void InlineHookDelimit(Instruction *InsertBefore);

    void InlineHookLoad(llvm::LoadInst *pLoad, unsigned uID, llvm::Instruction *InsertBefore);

    void InlineHookStore(llvm::StoreInst *pStore, unsigned uID, llvm::Instruction *InsertBefore);

    void InlineHookMemSet(llvm::MemSetInst *pMemSet, unsigned uID, llvm::Instruction *InsertBefore);

    void InlineHookMemTransfer(llvm::MemTransferInst *pMemTransfer, unsigned uID, llvm::Instruction *InsertBefore);

    void InlineHookFgetc(llvm::Instruction *pCall, unsigned uID, llvm::Instruction *InsertBefore);

    void InlineHookFread(llvm::Instruction *pCall, unsigned uID, llvm::Instruction *InsertBefore);

    void InlineHookOstream(llvm::Instruction *pCall, unsigned uID, llvm::Instruction *InsertBefore);

    void InstrumentMonitoredInsts(MonitoredRWInsts &MI);

    void InlineOutputCost(llvm::Instruction *InsertBefore);

    void CreateIfElseBlock(Function *RecurFunc, llvm::Function *&SampledRecurFunc, llvm::ValueToValueMapTy &SampledVMap);

    void RemapInstruction(Instruction *I, ValueToValueMapTy &VMap);

    bool RemapFunctionCalls(const std::set<Function *> &setFunc,
                            std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping,
                            ValueToValueMapTy &originClonedMapping);

    void CloneFunctions(std::set<Function *> &setFunc, ValueToValueMapTy &originClonedMapping);

    bool CloneRemapCallees(const std::set<BasicBlock *> &setBB, std::set<Function *> &setCallee,
                           ValueToValueMapTy &originClonedMapping, std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping);

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
    ConstantInt *ConstantInt0; // end
    ConstantInt *ConstantIntN1;
    ConstantInt *ConstantInt1; // delimit
    ConstantInt *ConstantInt2; // load
    ConstantInt *ConstantInt3; // store
    ConstantInt *ConstantInt4; // loop begin
    ConstantInt *ConstantInt5; // loop end
    ConstantInt *ConstantLong16;
    ConstantInt *ConstantLong1;
    ConstantInt *ConstantLongN1;
    ConstantInt *ConstantDelimit;
    ConstantPointerNull *ConstantNULL;
    Constant *SAMPLE_RATE_ptr;

    Function *myNewF;
};

#endif //PRODUCTIONRUN_RECURSIVESAMPLEINSTRUMENTOR_H
