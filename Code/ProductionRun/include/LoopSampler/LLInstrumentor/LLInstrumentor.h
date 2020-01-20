#ifndef NEWCOMAIR_LLINSTRUMENTOR_H
#define NEWCOMAIR_LLINSTRUMENTOR_H

#include <vector>
#include <set>

#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Constants.h>
#include <llvm/Transforms/Utils/ValueMapper.h>
#include <llvm/Analysis/AliasSetTracker.h>

#include "Common/LocateInstrument.h"
#include "Common/MonitorRWInsts.h"

using namespace llvm;

struct IndvarInstIDStride {
    unsigned indvarInstID;
    int stride;
};

enum class LOOP_TYPE {
    OTHERS = 0,
    ARRAY = 1,
    LINKEDLIST = 2
};

struct LLInstrumentor : public ModulePass {

    static char ID;

    LLInstrumentor();

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    virtual bool runOnModule(Module &M);

private:

    // Setup
    void SetupInit(Module &M);

    void SetupTypes();

    void SetupStructs();

    void SetupConstants();

    void SetupGlobals();

    void SetupFunctions();

    // Search and optimize R/W Insts
    LOOP_TYPE getArrayListInsts(Loop *pLoop, MonitoredRWInsts &MI);

    void getMonitoredRWInsts(const std::set<BasicBlock *> &setBB, MonitoredRWInsts &MI);

    void getAliasInstInfo(std::unique_ptr<AliasSetTracker> CurAST, const MonitoredRWInsts &MI, vector<set<Instruction *>> &vecAI);

    void removeByDomInfo(DominatorTree &DT, vector<set<Instruction *>> &vecAI, MonitoredRWInsts &MI);

    void getInstsToBeHoisted(Loop *pLoop, AliasAnalysis &AA, MonitoredRWInsts &inplaceMI, MonitoredRWInsts &hoistMI);

    bool readIndvarStride(const char *filePath, std::vector<IndvarInstIDStride> &vecIndvarInstIDStride);

    bool cloneDependantInsts(Loop *pLoop, Instruction *pInst, Instruction *InsertBefore, ValueToValueMapTy &VMap);

    bool sinkInstsToLoopExit(Loop *pLoop, Instruction *pInst, Instruction *InsertBefore, ValueToValueMapTy &VMap);

    void insertBeforeExitLoop(Loop *pLoop, DominatorTree &DT, ValueToValueMapTy &VMap,
                              map<BasicBlock *, BasicBlock *> &mapExit2Inter);

    // Clone and re-structure
    bool CloneRemapCallees(const std::set<BasicBlock *> &setBB, std::set<Function *> &setCallee, ValueToValueMapTy &originClonedMapping, std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping);

    void FindCalleesInDepth(const std::set<BasicBlock *> &setBB, std::set<Function *> &setToDo,
                            std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping);

    void FindDirectCallees(const std::set<BasicBlock *> &setBB, std::vector<Function *> &vecWorkList,
                           std::set<Function *> &setToDo,
                           std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping);

    void CloneFunctions(std::set<Function *> &setFunc, ValueToValueMapTy &originClonedMapping);

    bool RemapFunctionCalls(const std::set<Function *> &setFunc,
                            std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping,
                            ValueToValueMapTy &originClonedMapping);

    void CreateIfElseBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded);

    void CreateIfElseIfBlock(Loop *pInnerLoop, std::vector<BasicBlock *> &vecAdded);

    // Instrument
    void InstrumentMonitoredInsts(MonitoredRWInsts &MI);

    void HoistOrSinkMonitoredInsts(MonitoredRWInsts &MI, Instruction *InsertBefore);

    void InstrumentCallees(std::set<Function *> setOriginFunc, ValueToValueMapTy &originClonedMapping);

    void InstrumentMain(StringRef funcName);

    // Inline instrument
    void InlineNumGlobalCost(Loop *pLoop, ValueToValueMapTy &VMap);

    void InlineGlobalCostForLoop(std::set<BasicBlock* > & setBBInLoop);

    void InlineGlobalCostForCallee(Function * pFunction);

    void InlineSetRecord(Value *address, Value *length, Value *id, Instruction *InsertBefore);

    void InlineHookDelimit(Instruction *InsertBefore);

    void InlineHookLoad(LoadInst *pLoad, unsigned uID, Instruction *InsertBefore);

    void InlineHookStore(StoreInst *pStore, unsigned uID, Instruction *InsertBefore);

    void InlineHookMemSet(MemSetInst *pMemSet, unsigned uID, Instruction *InsertBefore);

    void InlineHookMemTransfer(MemTransferInst *pMemTransfer, unsigned uID, Instruction *InsertBefore);

    void InlineHookFgetc(Instruction *pCall, unsigned uID, Instruction *InsertBefore);

    void InlineHookFread(Instruction *pCall, unsigned uID, Instruction *InsertBefore);

    void InlineHookOstream(Instruction *pCall, unsigned uID, Instruction *InsertBefore);

    void InlineOutputCost(Instruction *InsertBefore);

    void InlineHookLoopBegin(LoadInst *pLoad, unsigned uID, Instruction *InsertBefore);

    void InlineHookLoopEnd(LoadInst *pLoad, unsigned uID, Instruction *InsertBefore);

    void CloneInnerLoop(Loop *pLoop, vector<BasicBlock *> &vecAdd, ValueToValueMapTy &VMap, vector<BasicBlock *> &vecCloned);

    void RemapInstruction(Instruction *I, ValueToValueMapTy &VMap);

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


#endif //NEWCOMAIR_LLINSTRUMENTOR_H
