#ifndef INHOUSE_INHOUSEHOOKPASS_H
#define INHOUSE_INHOUSEHOOKPASS_H

#include <set>
#include <vector>

#include "llvm/Pass.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "Common/MonitorRWInsts.h"

namespace inhouse_hook_pass
{

struct InHouseHookPass : public llvm::ModulePass
{

    static char ID;

    InHouseHookPass();

    void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

    bool runOnModule(llvm::Module &M) override;

    // setup
    void SetupInit();

    void SetupTypes();

    void SetupConstants();

    void SetupFunctions();

    // get function lists from input files
    void GetFuncList(const std::set<unsigned> &setFuncIDList, std::set<llvm::Function *> &setFuncList);

    // get all functions
    void GetFuncList(std::set<llvm::Function *> &setFuncList);

    // collect Instructions (Must be original)
    static void CollectInstructions(llvm::Function *F, common::MonitoredRWInsts &MI, llvm::Instruction *&FuncEntryInst,
                                    std::map<llvm::BasicBlock *, llvm::Instruction *> &mapBBEntryInst,
                                    std::set<llvm::Instruction *> &setFuncExitInst);

    // instrument
    void InstrumentEntryFunc(llvm::Function *EntryFunc);

    void InstrumentFunc(llvm::Function *F);

    void InstrumentInit(llvm::Instruction *InsertBefore);

    void InstrumentCallBefore(unsigned FuncID, llvm::Instruction *InsertBefore);

    void InstrumentCostAlloc(llvm::AllocaInst *&BBAllocInst, llvm::Instruction *InsertBefore);

    void InstrumentParams(llvm::Function *F, llvm::AllocaInst *BBAllocInst, llvm::Instruction *InsertBefore);

    void InstrumentCostAdd(llvm::AllocaInst *BBAllocInst, std::map<llvm::BasicBlock *, llvm::Instruction *> &mapBBEntryInst);

    void InstrumentRW(common::MonitoredRWInsts &MI);

    void InstrumentReturn(llvm::AllocaInst *BBAllocInst, std::set<llvm::Instruction *> &setFuncExitInst);

    void InstrumentFinal(std::set<llvm::Instruction *> &setFuncExitInst);

    void InstrumentLoad(llvm::LoadInst *pLoad, llvm::Instruction *InsertBefore);

    void InstrumentStore(llvm::StoreInst *pStore, llvm::Instruction *InsertBefore);

    void InstrumentMemTransfer(llvm::MemTransferInst *pMemTransfer, llvm::Instruction *InsertBefore);

    void InstrumentMemSet(llvm::MemSetInst *pMemSet, llvm::Instruction *InsertBefore);

    void InstrumentFgetc(llvm::Instruction *pCall, llvm::Instruction *InsertBefore);

    void InstrumentFread(llvm::Instruction *pCall, llvm::Instruction *InsertAfter);

    void InstrumentOstream(llvm::Instruction *pCall, llvm::Instruction *InsertBefore);

    void FindDirectCallees(const std::set<llvm::BasicBlock *> &setBB, std::vector<llvm::Function *> &vecWorkList,
                           std::set<llvm::Function *> &setToDo,
                           std::map<llvm::Function *, std::set<llvm::Instruction *>> &funcCallSiteMapping);

    void FindCalleesInDepth(const std::set<llvm::BasicBlock *> &setBB, std::set<llvm::Function *> &setToDo,
                            std::map<llvm::Function *, std::set<llvm::Instruction *>> &funcCallSiteMapping);

    void CloneFunctions(std::set<llvm::Function *> &setFunc, llvm::ValueToValueMapTy &originClonedMapping);

    bool RemapFunctionCalls(const std::set<llvm::Function *> &setFunc,
                            std::map<llvm::Function *, std::set<llvm::Instruction *>> &funcCallSiteMapping,
                            llvm::ValueToValueMapTy &originClonedMapping);

private:
    llvm::Module *pModule;

    // types
    llvm::IntegerType *IntType;
    llvm::IntegerType *LongType;
    llvm::Type *VoidType;

    // constants
    llvm::ConstantInt *ConstantLong0;
    llvm::ConstantInt *ConstantLong1;

    // inhouse aprof hooks
    // void aprof_init();
    llvm::Function *aprof_init;
    // void aprof_write(unsigned long start_addr, unsigned long length);
    llvm::Function *aprof_write;
    //void aprof_read(unsigned long start_addr, unsigned long length);
    llvm::Function *aprof_read;
    // void aprof_increment_rms(unsigned long length);
    llvm::Function *aprof_increment_rms;
    // void aprof_call_before(unsigned funcId);
    llvm::Function *aprof_call_before;
    // void aprof_return(unsigned long numCost);
    llvm::Function *aprof_return;
    // void aprof_final();
    llvm::Function *aprof_final;
};
} // namespace inhouse_hook_pass

#endif //INHOUSE_INHOUSEHOOKPASS_H
