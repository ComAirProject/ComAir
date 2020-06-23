#include "LoopSampler/LoopRWCostInstrumentor/LoopRWCostInstrumentor.h"

#include <fstream>
#include <map>
#include <set>
#include <vector>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "Common/ArrayLinkedIdentifier.h"
#include "Common/BBProfiling.h"
#include "Common/Constant.h"
#include "Common/Helper.h"
#include "Common/LoadStoreMem.h"
#include "Common/MonitorRWInsts.h"

using namespace llvm;
using std::map;
using std::set;
using std::vector;

#define DEBUG_TYPE "LoopRWCostInstrumentor"

STATISTIC(NumNoOptRW, "Num of Non-optimized Read/Write Instrument Sites");
STATISTIC(NumOptRW, "Num of Optimized Read/Write Instrument Sites");
STATISTIC(NumHoistRW, "Num of Hoisted Read/Write Instrument Sites");
STATISTIC(NumNoOptCost, "Num of Non-optimized Cost Instrument Sites");
STATISTIC(NumOptCost, "Num of Optimized Cost Instrument Sites");

static RegisterPass<LoopRWCostInstrumentor>
    X("opt-loop-instrument", "instrument a loop w/o RW or Cost optimization",
      false, false);

static cl::opt<unsigned>
    uSrcLine("noLine", cl::desc("Source Code Line Number for the Loop"),
             cl::Optional, cl::value_desc("uSrcCodeLine"));

static cl::opt<std::string> strFileName("strFile",
                                        cl::desc("File Name for the Loop"),
                                        cl::Optional,
                                        cl::value_desc("strFileName"));

static cl::opt<std::string> strFuncName("strFunc", cl::desc("Function Name"),
                                        cl::Optional,
                                        cl::value_desc("strFuncName"));

static cl::opt<bool> bRW("bRW",
                         cl::desc("Only Instrument Sites instead of BBs"),
                         cl::Optional, cl::value_desc("bRW"));

static cl::opt<bool> bNoOptInst("bNoOptInst",
                                cl::desc("No Opt for Num of Instrument Sites"),
                                cl::Optional, cl::value_desc("bNoOptInst"));

static cl::opt<bool> bNoOptCost("bNoOptCost",
                                cl::desc("No Opt for Merging Cost"),
                                cl::Optional, cl::value_desc("bNoOptCost"));

static cl::opt<bool> bNoOptLoop("bNoOptLoop", cl::desc("No Opt for Loop"),
                                cl::Optional, cl::value_desc("bNoOptLoop"));

char LoopRWCostInstrumentor::ID = 0;

LoopRWCostInstrumentor::LoopRWCostInstrumentor() : ModulePass(ID)
{
  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializeDominatorTreeWrapperPassPass(Registry);
  //    initializeAAResultsWrapperPassPass(Registry);
  initializeLoopInfoWrapperPassPass(Registry);
}

void LoopRWCostInstrumentor::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
}

static void removeByDomInfo(MonitoredRWInsts &MI, DominatorTree &DT)
{
  std::map<Value *, std::set<Instruction *>> AliasedInsts;
  for (auto &kv : MI.mapLoadID)
  {
    Value *V = kv.first->getPointerOperand();
    if (AliasedInsts.find(V) == AliasedInsts.end())
    {
      AliasedInsts[V] = std::set<Instruction *>();
    }
    AliasedInsts[V].insert(kv.first);
  }
  for (auto &kv : MI.mapStoreID)
  {
    Value *V = kv.first->getPointerOperand();
    if (AliasedInsts.find(V) == AliasedInsts.end())
    {
      AliasedInsts[V] = std::set<Instruction *>();
    }
    AliasedInsts[V].insert(kv.first);
  }

  for (auto &kv : AliasedInsts)
  {
    if (!common::hasNonLoadStoreUse(kv.first) && kv.second.size() > 1)
    {
      for (Instruction *rhs : kv.second)
      {
        for (Instruction *lhs : kv.second)
        {
          if (lhs != rhs)
          {
            if (DT.dominates(lhs, rhs))
            {
              if (LoadInst *LI = dyn_cast<LoadInst>(rhs))
              {
                MI.mapLoadID.erase(LI);
              }
              else if (StoreInst *SI = dyn_cast<StoreInst>(rhs))
              {
                MI.mapStoreID.erase(SI);
              }
              break;
            }
          }
        }
      }
    }
  }
}

bool LoopRWCostInstrumentor::runOnModule(Module &M)
{
  SetupInit(M);

  Function *pFunction =
      SearchFunctionByName(M, strFileName, strFuncName, uSrcLine);
  if (!pFunction)
  {
    errs() << "Cannot find the function\n";
    return false;
  }

  DominatorTree &DT =
      getAnalysis<DominatorTreeWrapperPass>(*pFunction).getDomTree();
  LoopInfo &LoopInfo =
      getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();

  Loop *pLoop = SearchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);
  if (!pLoop)
  {
    errs() << "Cannot find the loop\n";
    return false;
  }

  set<BasicBlock *> setBBInLoop;
  for (BasicBlock *BB : pLoop->blocks())
  {
    setBBInLoop.insert(BB);
  }

  if (bRW)
  {
    MonitoredRWInsts MI;
    for (BasicBlock *BB : setBBInLoop)
    {
      ++NumNoOptCost;
      for (Instruction &II : *BB)
      {
        MI.add(&II);
      }
    }

    NumNoOptRW += MI.size();

    removeByDomInfo(MI, DT);

    MonitoredRWInsts HoistMI;
    for (auto &kv : MI.mapLoadID)
    {
      if (pLoop->isLoopInvariant(kv.first->getPointerOperand()))
      {
        HoistMI.mapLoadID[kv.first] = kv.second;
      }
    }
    for (auto &kv : MI.mapStoreID)
    {
      if (pLoop->isLoopInvariant(kv.first->getPointerOperand()))
      {
        HoistMI.mapStoreID[kv.first] = kv.second;
      }
    }

    // After Alias+Dominance checking and removing,
    // if operands of a LoadInst and a StoreInst are still Alias/equal,
    // then their sequence is unknown, thus should not be hoisted.
    for (auto &kvLoad : HoistMI.mapLoadID)
    {
      LoadInst *pLoad = kvLoad.first;
      for (auto &kvStore : HoistMI.mapStoreID)
      {
        StoreInst *pStore = kvStore.first;
        if (pLoad->getPointerOperand() == pStore->getPointerOperand())
        {
          HoistMI.mapLoadID.erase(pLoad);
          HoistMI.mapStoreID.erase(pStore);
        }
      }
    }

    NumHoistRW = HoistMI.size();
    MI.diff(HoistMI);
    NumOptRW += MI.size();

    // Instrument Loops
    InstrumentMonitoredInsts(MI);

    BasicBlock *PreHeader = pLoop->getLoopPreheader();
    Instruction *First = &*PreHeader->getFirstInsertionPt();
    InlineHookDelimit(First);
    Instruction *pTerm = PreHeader->getTerminator();
    InstrumentHoistMonitoredInsts(HoistMI, pTerm);
  }

  if (!bRW)
  {
    InlineGlobalCostForLoop(setBBInLoop);
  }

  // Find Callees
  vector<Function *> vecWorkList;
  set<Function *> setCallees;
  std::map<Function *, std::set<Instruction *>> funcCallSiteMapping;
  FindCalleesInDepth(setBBInLoop, setCallees, funcCallSiteMapping);

  // Instrument Callees
  for (Function *Callee : setCallees)
  {
    if (bRW)
    {
      DominatorTree &CalleeDT =
          getAnalysis<DominatorTreeWrapperPass>(*Callee).getDomTree();
      MonitoredRWInsts CalleeMI;
      for (BasicBlock &BB : *Callee)
      {
        ++NumNoOptCost;
        for (Instruction &II : BB)
        {
          CalleeMI.add(&II);
        }
      }
      NumNoOptRW += CalleeMI.size();
      if (!bNoOptInst)
      {
        removeByDomInfo(CalleeMI, CalleeDT);
      }
      NumOptRW += CalleeMI.size();
      // Instrument RW
      InstrumentMonitoredInsts(CalleeMI);
    }
    if (!bRW)
    {
      InlineGlobalCostForCallee(Callee);
    }
  }

  InstrumentMain("main");

  errs() << "Orig RW:" << NumNoOptRW << ", InPlace:" << NumOptRW
         << ", Hoist:" << NumHoistRW << "\n";
  errs() << "Orig Cost:" << NumNoOptCost << ", Opt Cost:" << NumOptCost << "\n";
  return true;
}

void LoopRWCostInstrumentor::SetupInit(Module &M)
{
  this->pModule = &M;
  SetupTypes();
  SetupStructs();
  SetupConstants();
  SetupGlobals();
  SetupFunctions();
}

void LoopRWCostInstrumentor::SetupTypes()
{
  this->VoidType = Type::getVoidTy(pModule->getContext());
  this->LongType = IntegerType::get(pModule->getContext(), 64);
  this->IntType = IntegerType::get(pModule->getContext(), 32);
  this->CharType = IntegerType::get(pModule->getContext(), 8);
  this->CharStarType = PointerType::get(this->CharType, 0);
}

void LoopRWCostInstrumentor::SetupStructs()
{
  vector<Type *> struct_fields;

  assert(pModule->getTypeByName("struct.stMemRecord") == nullptr);
  this->struct_stMemRecord =
      StructType::create(pModule->getContext(), "struct.stMemRecord");
  struct_fields.clear();
  struct_fields.push_back(this->LongType); // address
  struct_fields.push_back(this->IntType);  // length
  struct_fields.push_back(this->IntType);  // id (+: Read, -: Write,
                                           // Special:DELIMIT, LOOP_BEGIN,
                                           // LOOP_END)
  if (this->struct_stMemRecord->isOpaque())
  {
    this->struct_stMemRecord->setBody(struct_fields, false);
  }
}

void LoopRWCostInstrumentor::SetupConstants()
{
  // long: 0, 1, 16
  this->ConstantLong0 =
      ConstantInt::get(pModule->getContext(), APInt(64, StringRef("0"), 10));
  this->ConstantLong1 =
      ConstantInt::get(pModule->getContext(), APInt(64, StringRef("1"), 10));
  this->ConstantLong16 =
      ConstantInt::get(pModule->getContext(), APInt(64, StringRef("16"), 10));

  // int: -1, 0, 1, 2, 3, 4, 5, 6
  this->ConstantIntN1 =
      ConstantInt::get(pModule->getContext(), APInt(32, StringRef("-1"), 10));
  this->ConstantInt0 =
      ConstantInt::get(pModule->getContext(), APInt(32, StringRef("0"), 10));
  this->ConstantInt1 =
      ConstantInt::get(pModule->getContext(), APInt(32, StringRef("1"), 10));
  this->ConstantInt2 =
      ConstantInt::get(pModule->getContext(), APInt(32, StringRef("2"), 10));
  this->ConstantInt3 =
      ConstantInt::get(pModule->getContext(), APInt(32, StringRef("3"), 10));
  this->ConstantInt4 =
      ConstantInt::get(pModule->getContext(), APInt(32, StringRef("4"), 10));
  this->ConstantInt5 =
      ConstantInt::get(pModule->getContext(), APInt(32, StringRef("5"), 10));
  // int: delimit, loop_begin, loop_end
  this->ConstantDelimit = ConstantInt::get(
      pModule->getContext(), APInt(32, StringRef(std::to_string(DELIMIT)), 10));
  this->ConstantLoopBegin =
      ConstantInt::get(pModule->getContext(),
                       APInt(32, StringRef(std::to_string(LOOP_BEGIN)), 10));
  this->ConstantLoopEnd =
      ConstantInt::get(pModule->getContext(),
                       APInt(32, StringRef(std::to_string(LOOP_END)), 10));

  // char*: NULL
  this->ConstantNULL = ConstantPointerNull::get(this->CharStarType);
}

void LoopRWCostInstrumentor::SetupGlobals()
{
  // int numGlobalCounter = 1;
  // TODO: CommonLinkage or ExternalLinkage
  assert(pModule->getGlobalVariable("numGlobalCounter") == nullptr);
  this->numGlobalCounter = new GlobalVariable(*pModule, this->IntType, false,
                                              GlobalValue::ExternalLinkage,
                                              nullptr, "numGlobalCounter");
  this->numGlobalCounter->setAlignment(4);
  this->numGlobalCounter->setInitializer(this->ConstantInt1);

  // int SAMPLE_RATE = 0;
  assert(pModule->getGlobalVariable("SAMPLE_RATE") == nullptr);
  this->SAMPLE_RATE =
      new GlobalVariable(*pModule, this->IntType, false,
                         GlobalValue::CommonLinkage, nullptr, "SAMPLE_RATE");
  this->SAMPLE_RATE->setAlignment(4);
  this->SAMPLE_RATE->setInitializer(this->ConstantInt0);

  // char *pcBuffer_CPI = nullptr;
  assert(pModule->getGlobalVariable("pcBuffer_CPI") == nullptr);
  this->pcBuffer_CPI =
      new GlobalVariable(*pModule, this->CharStarType, false,
                         GlobalValue::ExternalLinkage, nullptr, "pcBuffer_CPI");
  this->pcBuffer_CPI->setAlignment(8);
  this->pcBuffer_CPI->setInitializer(this->ConstantNULL);

  // long iBufferIndex_CPI = 0;
  assert(pModule->getGlobalVariable("iBufferIndex_CPI") == nullptr);
  this->iBufferIndex_CPI = new GlobalVariable(*pModule, this->LongType, false,
                                              GlobalValue::ExternalLinkage,
                                              nullptr, "iBufferIndex_CPI");
  this->iBufferIndex_CPI->setAlignment(8);
  this->iBufferIndex_CPI->setInitializer(this->ConstantLong0);

  // struct_stLogRecord Record_CPI
  assert(pModule->getGlobalVariable("Record_CPI") == nullptr);
  this->Record_CPI =
      new GlobalVariable(*pModule, this->struct_stMemRecord, false,
                         GlobalValue::ExternalLinkage, nullptr, "Record_CPI");
  this->Record_CPI->setAlignment(16);
  ConstantAggregateZero *const_struct =
      ConstantAggregateZero::get(this->struct_stMemRecord);
  this->Record_CPI->setInitializer(const_struct);

  // const char *SAMPLE_RATE_ptr = "SAMPLE_RATE";
  ArrayType *ArrayTy12 = ArrayType::get(this->CharType, 12);
  GlobalVariable *pArrayStr = new GlobalVariable(
      *pModule, ArrayTy12, true, GlobalValue::PrivateLinkage, nullptr, "");
  pArrayStr->setAlignment(1);
  Constant *ConstArray =
      ConstantDataArray::getString(pModule->getContext(), "SAMPLE_RATE", true);
  vector<Constant *> vecIndex;
  vecIndex.push_back(this->ConstantInt0);
  vecIndex.push_back(this->ConstantInt0);
  this->SAMPLE_RATE_ptr =
      ConstantExpr::getGetElementPtr(ArrayTy12, pArrayStr, vecIndex);
  pArrayStr->setInitializer(ConstArray);

  // long numGlobalCost = 0;
  assert(pModule->getGlobalVariable("numGlobalCost") == nullptr);
  this->numGlobalCost = new GlobalVariable(*pModule, this->LongType, false,
                                           GlobalValue::ExternalLinkage,
                                           nullptr, "numGlobalCost");
  this->numGlobalCost->setAlignment(8);
  this->numGlobalCost->setInitializer(this->ConstantLong0);
}

void LoopRWCostInstrumentor::SetupFunctions()
{

  vector<Type *> ArgTypes;

  // getenv
  this->getenv = pModule->getFunction("getenv");
  if (!this->getenv)
  {
    ArgTypes.push_back(this->CharStarType);
    FunctionType *getenv_FuncTy =
        FunctionType::get(this->CharStarType, ArgTypes, false);
    this->getenv = Function::Create(getenv_FuncTy, GlobalValue::ExternalLinkage,
                                    "getenv", pModule);
    this->getenv->setCallingConv(CallingConv::C);
    ArgTypes.clear();
  }

  // atoi
  this->function_atoi = pModule->getFunction("atoi");
  if (!this->function_atoi)
  {
    ArgTypes.clear();
    ArgTypes.push_back(this->CharStarType);
    FunctionType *atoi_FuncTy =
        FunctionType::get(this->IntType, ArgTypes, false);
    this->function_atoi = Function::Create(
        atoi_FuncTy, GlobalValue::ExternalLinkage, "atoi", pModule);
    this->function_atoi->setCallingConv(CallingConv::C);
    ArgTypes.clear();
  }

  // geo
  this->geo = this->pModule->getFunction("geo");
  if (!this->geo)
  {
    ArgTypes.push_back(this->IntType);
    FunctionType *Geo_FuncTy =
        FunctionType::get(this->IntType, ArgTypes, false);
    this->geo = Function::Create(Geo_FuncTy, GlobalValue::ExternalLinkage,
                                 "geo", this->pModule);
    this->geo->setCallingConv(CallingConv::C);
    ArgTypes.clear();
  }

  // InitMemHooks
  this->InitMemHooks = this->pModule->getFunction("InitMemHooks");
  if (!this->InitMemHooks)
  {
    FunctionType *InitHooks_FuncTy =
        FunctionType::get(this->CharStarType, ArgTypes, false);
    this->InitMemHooks =
        Function::Create(InitHooks_FuncTy, GlobalValue::ExternalLinkage,
                         "InitMemHooks", this->pModule);
    this->InitMemHooks->setCallingConv(CallingConv::C);
    ArgTypes.clear();
  }

  // FinalizeMemHooks
  this->FinalizeMemHooks = this->pModule->getFunction("FinalizeMemHooks");
  if (!this->FinalizeMemHooks)
  {
    ArgTypes.push_back(this->LongType);
    FunctionType *FinalizeMemHooks_FuncTy =
        FunctionType::get(this->VoidType, ArgTypes, false);
    this->FinalizeMemHooks =
        Function::Create(FinalizeMemHooks_FuncTy, GlobalValue::ExternalLinkage,
                         "FinalizeMemHooks", this->pModule);
    this->FinalizeMemHooks->setCallingConv(CallingConv::C);
    ArgTypes.clear();
  }
}

void LoopRWCostInstrumentor::InlineSetRecord(Value *address, Value *length,
                                             Value *id,
                                             Instruction *InsertBefore)
{

  LoadInst *pLoadPointer;
  LoadInst *pLoadIndex;
  BinaryOperator *pBinary;
  CastInst *pCastInst;
  PointerType *pPointerType;

  // char *pc = (char *)pcBuffer_CPI[iBufferIndex_CPI];
  pLoadPointer = new LoadInst(this->pcBuffer_CPI, "", false, InsertBefore);
  pLoadPointer->setAlignment(8);
  pLoadIndex = new LoadInst(this->iBufferIndex_CPI, "", false, InsertBefore);
  pLoadIndex->setAlignment(8);
  GetElementPtrInst *getElementPtr = GetElementPtrInst::Create(
      this->CharType, pLoadPointer, pLoadIndex, "", InsertBefore);
  // struct_stMemRecord *ps = (struct_stMemRecord *)pc;
  pPointerType = PointerType::get(this->struct_stMemRecord, 0);
  pCastInst = new BitCastInst(getElementPtr, pPointerType, "", InsertBefore);

  // ps->address = address;
  vector<Value *> ptr_address_indices;
  ptr_address_indices.push_back(this->ConstantInt0);
  ptr_address_indices.push_back(this->ConstantInt0);
  GetElementPtrInst *pAddress =
      GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst,
                                ptr_address_indices, "", InsertBefore);
  auto pStoreAddress = new StoreInst(address, pAddress, false, InsertBefore);
  pStoreAddress->setAlignment(8);

  // ps->length = length;
  vector<Value *> ptr_length_indices;
  ptr_length_indices.push_back(this->ConstantInt0);
  ptr_length_indices.push_back(this->ConstantInt1);
  GetElementPtrInst *pLength =
      GetElementPtrInst::Create(this->struct_stMemRecord, pCastInst,
                                ptr_length_indices, "", InsertBefore);
  auto pStoreLength = new StoreInst(length, pLength, false, InsertBefore);
  pStoreLength->setAlignment(8);

  // ps->id = id;
  vector<Value *> ptr_id_indices;
  ptr_id_indices.push_back(this->ConstantInt0);
  ptr_id_indices.push_back(this->ConstantInt2);
  GetElementPtrInst *pFlag = GetElementPtrInst::Create(
      this->struct_stMemRecord, pCastInst, ptr_id_indices, "", InsertBefore);
  auto pStoreFlag = new StoreInst(id, pFlag, false, InsertBefore);
  pStoreFlag->setAlignment(4);

  // iBufferIndex_CPI += 16
  pBinary =
      BinaryOperator::Create(Instruction::Add, pLoadIndex, this->ConstantLong16,
                             "iBufferIndex+=16:", InsertBefore);
  auto pStoreIndex =
      new StoreInst(pBinary, this->iBufferIndex_CPI, false, InsertBefore);
  pStoreIndex->setAlignment(8);
}

void LoopRWCostInstrumentor::InlineHookDelimit(Instruction *InsertBefore)
{

  InlineSetRecord(this->ConstantLong0, this->ConstantInt0,
                  this->ConstantDelimit, InsertBefore);
}

void LoopRWCostInstrumentor::InlineHookLoad(LoadInst *pLoad, unsigned uID,
                                            Instruction *InsertBefore)
{

  assert(pLoad && InsertBefore);
  assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
  int ID = uID;

  DataLayout *dl = new DataLayout(this->pModule);

  Value *addr = pLoad->getOperand(0);
  assert(addr);
  Type *type = addr->getType();
  assert(type);
  Type *type1 = type->getContainedType(0);
  assert(type1);

  ConstantInt *const_length = ConstantInt::get(
      this->pModule->getContext(),
      APInt(32, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
  CastInst *int64_address =
      new PtrToIntInst(addr, this->LongType, "", InsertBefore);
  ConstantInt *const_id =
      ConstantInt::get(this->pModule->getContext(),
                       APInt(32, StringRef(std::to_string(ID)), 10));
  InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}

void LoopRWCostInstrumentor::InlineHookStore(StoreInst *pStore, unsigned uID,
                                             Instruction *InsertBefore)
{

  assert(pStore && InsertBefore);
  assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
  // Write: Negative ID
  int ID = -uID;

  DataLayout *dl = new DataLayout(this->pModule);

  Value *addr = pStore->getOperand(1);
  assert(addr);
  Type *type = addr->getType();
  assert(type);
  Type *type1 = type->getContainedType(0);
  assert(type1);

  ConstantInt *const_length = ConstantInt::get(
      this->pModule->getContext(),
      APInt(32, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
  CastInst *int64_address =
      new PtrToIntInst(addr, this->LongType, "", InsertBefore);
  ConstantInt *const_id =
      ConstantInt::get(this->pModule->getContext(),
                       APInt(32, StringRef(std::to_string(ID)), 10));
  InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}

void LoopRWCostInstrumentor::InlineHookMemSet(MemSetInst *pMemSet, unsigned uID,
                                              Instruction *InsertBefore)
{

  assert(pMemSet && InsertBefore);
  assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
  // Write: Negative ID
  int ID = -uID;

  DataLayout *dl = new DataLayout(this->pModule);

  Value *dest = pMemSet->getDest();
  assert(dest);
  Value *len = pMemSet->getLength();
  assert(len);
  CastInst *int64_address =
      new PtrToIntInst(dest, this->LongType, "", InsertBefore);
  CastInst *int32_len =
      new TruncInst(len, this->IntType, "memset_len_conv", InsertBefore);
  ConstantInt *const_id =
      ConstantInt::get(this->pModule->getContext(),
                       APInt(32, StringRef(std::to_string(ID)), 10));
  InlineSetRecord(int64_address, int32_len, const_id, InsertBefore);
}

void LoopRWCostInstrumentor::InlineHookMemTransfer(
    MemTransferInst *pMemTransfer, unsigned uID, Instruction *InsertBefore)
{

  assert(pMemTransfer && InsertBefore);
  assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
  // Read Src: positive ID
  int srcID = uID;
  // Write Dest: Negative ID
  int dstID = -uID;

  DataLayout *dl = new DataLayout(this->pModule);

  Value *src = pMemTransfer->getSource();
  assert(src);
  Value *dest = pMemTransfer->getDest();
  assert(dest);
  Value *len = pMemTransfer->getLength();
  assert(len);
  CastInst *src_int64_address =
      new PtrToIntInst(src, this->LongType, "", InsertBefore);
  CastInst *dest_int64_address =
      new PtrToIntInst(dest, this->LongType, "", InsertBefore);
  CastInst *int32_len =
      new TruncInst(len, this->IntType, "memtransfer_len_conv", InsertBefore);
  ConstantInt *const_src_id =
      ConstantInt::get(this->pModule->getContext(),
                       APInt(32, StringRef(std::to_string(srcID)), 10));
  ConstantInt *const_dst_id =
      ConstantInt::get(this->pModule->getContext(),
                       APInt(32, StringRef(std::to_string(dstID)), 10));
  InlineSetRecord(src_int64_address, int32_len, const_src_id, InsertBefore);
  InlineSetRecord(dest_int64_address, int32_len, const_dst_id, InsertBefore);
}

void LoopRWCostInstrumentor::InlineHookFgetc(Instruction *pCall, unsigned uID,
                                             Instruction *InsertBefore)
{

  assert(pCall && InsertBefore);
  assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
  int ID = uID;

  ConstantInt *const_id =
      ConstantInt::get(this->pModule->getContext(),
                       APInt(32, StringRef(std::to_string(ID)), 10));
  InlineSetRecord(ConstantLong0, ConstantInt1, const_id, InsertBefore);
}

void LoopRWCostInstrumentor::InlineHookFread(Instruction *pCall, unsigned uID,
                                             Instruction *InsertBefore)
{

  assert(pCall && InsertBefore);
  assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
  assert(InsertBefore == pCall->getNextNode());
  int ID = uID;

  CastInst *length =
      new TruncInst(pCall, this->IntType, "fread_len_conv", InsertBefore);
  ConstantInt *const_id =
      ConstantInt::get(this->pModule->getContext(),
                       APInt(32, StringRef(std::to_string(ID)), 10));
  InlineSetRecord(ConstantLong0, length, const_id, InsertBefore);
}

void LoopRWCostInstrumentor::InlineHookOstream(Instruction *pCall, unsigned uID,
                                               Instruction *InsertBefore)
{

  assert(pCall && InsertBefore);
  assert(uID != INVALID_ID && MIN_ID <= uID && uID <= MAX_ID);
  int ID = uID;

  DataLayout *dl = new DataLayout(this->pModule);

  CallSite cs(pCall);
  Value *pSecondArg = cs.getArgument(1);
  assert(pSecondArg);

  Type *type = pSecondArg->getType();
  assert(type);
  Type *type1 = type->getContainedType(0);
  assert(type1);

  ConstantInt *const_length = ConstantInt::get(
      this->pModule->getContext(),
      APInt(32, StringRef(std::to_string(dl->getTypeAllocSize(type1))), 10));
  CastInst *int64_address =
      new PtrToIntInst(pSecondArg, this->LongType, "", InsertBefore);
  ConstantInt *const_id =
      ConstantInt::get(this->pModule->getContext(),
                       APInt(32, StringRef(std::to_string(ID)), 10));
  InlineSetRecord(int64_address, const_length, const_id, InsertBefore);
}

void LoopRWCostInstrumentor::InstrumentHoistMonitoredInsts(
    MonitoredRWInsts &MI, Instruction *InsertBefore)
{

  for (auto &kv : MI.mapLoadID)
  {
    LoadInst *pLoad = kv.first;
    unsigned uID = kv.second;
    InlineHookLoad(pLoad, uID, InsertBefore);
  }

  for (auto &kv : MI.mapStoreID)
  {
    StoreInst *pStore = kv.first;
    unsigned uID = kv.second;
    InlineHookStore(pStore, uID, InsertBefore);
  }
}

void LoopRWCostInstrumentor::InstrumentMonitoredInsts(MonitoredRWInsts &MI)
{

  for (auto &kv : MI.mapLoadID)
  {
    LoadInst *pLoad = kv.first;
    unsigned uID = kv.second;
    InlineHookLoad(pLoad, uID, pLoad);
  }

  for (auto &kv : MI.mapStoreID)
  {
    StoreInst *pStore = kv.first;
    unsigned uID = kv.second;
    InlineHookStore(pStore, uID, pStore);
  }

  for (auto &kv : MI.mapMemSetID)
  {
    MemSetInst *pMemSet = kv.first;
    unsigned uID = kv.second;
    InlineHookMemSet(pMemSet, uID, pMemSet);
  }

  for (auto &kv : MI.mapMemTransferID)
  {
    MemTransferInst *pMemTransfer = kv.first;
    unsigned uID = kv.second;
    InlineHookMemTransfer(pMemTransfer, uID, pMemTransfer);
  }

  for (auto &kv : MI.mapFgetcID)
  {
    Instruction *pCall = kv.first;
    unsigned uID = kv.second;
    InlineHookFgetc(pCall, uID, pCall);
  }

  for (auto &kv : MI.mapFreadID)
  {
    Instruction *pCall = kv.first;
    unsigned uID = kv.second;
    InlineHookFread(pCall, uID, pCall->getNextNode());
  }

  for (auto &kv : MI.mapOstreamID)
  {
    Instruction *pCall = kv.first;
    unsigned uID = kv.second;
    InlineHookOstream(pCall, uID, pCall);
  }
}

void LoopRWCostInstrumentor::FindDirectCallees(
    const std::set<BasicBlock *> &setBB, std::vector<Function *> &vecWorkList,
    std::set<Function *> &setToDo,
    std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping)
{

  for (BasicBlock *BB : setBB)
  {
    if (isa<UnreachableInst>(BB->getTerminator()))
    {
      continue;
    }

    for (Instruction &II : *BB)
    {
      Instruction *pInst = &II;

      if (Function *pCalled = getCalleeFunc(pInst))
      {
        if (pCalled->getName() == "JS_Assert")
        {
          continue;
        }
        if (pCalled->begin() == pCalled->end())
        {
          continue;
        }
        if (pCalled->getSection().str() == ".text.startup")
        {
          continue;
        }
        if (!pCalled->isDeclaration())
        {
          funcCallSiteMapping[pCalled].insert(pInst);

          if (setToDo.find(pCalled) == setToDo.end())
          {
            setToDo.insert(pCalled);
            vecWorkList.push_back(pCalled);
          }
        }
      }
    }
  }
}

void LoopRWCostInstrumentor::FindCalleesInDepth(
    const std::set<BasicBlock *> &setBB, std::set<Function *> &setToDo,
    std::map<Function *, std::set<Instruction *>> &funcCallSiteMapping)
{

  // Function stack to recursively handle callees
  std::vector<Function *> vecWorkList;

  // Find in the outmost BBs
  FindDirectCallees(setBB, vecWorkList, setToDo, funcCallSiteMapping);

  while (!vecWorkList.empty())
  {

    Function *pCurrent = vecWorkList.back();
    vecWorkList.pop_back();

    // Store BBs in current func into set
    std::set<BasicBlock *> setCurrentBB;
    for (BasicBlock &BB : *pCurrent)
    {
      setCurrentBB.insert(&BB);
    }
    FindDirectCallees(setCurrentBB, vecWorkList, setToDo, funcCallSiteMapping);
  }
}

void LoopRWCostInstrumentor::InlineGlobalCostForLoop(
    std::set<BasicBlock *> &setBBInLoop)
{
  Function *pFunction = NULL;

  set<BasicBlock *>::iterator itSetBegin = setBBInLoop.begin();
  set<BasicBlock *>::iterator itSetEnd = setBBInLoop.end();

  assert(itSetBegin != itSetEnd);

  pFunction = (*itSetBegin)->getParent();

  BBProfilingGraph bbGraph = BBProfilingGraph(*pFunction);
  bbGraph.init();

  bbGraph.splitGivenBlock(setBBInLoop);
  bbGraph.calculateSpanningTree();

  BBProfilingEdge *pQueryEdge = bbGraph.addQueryChord();
  bbGraph.calculateChordIncrements();

  Instruction *entryInst = pFunction->getEntryBlock().getFirstNonPHI();
  AllocaInst *numLocalCounter =
      new AllocaInst(this->LongType, 0, "LOCAL_COST_BB", entryInst);
  numLocalCounter->setAlignment(8);
  StoreInst *pStore = new StoreInst(ConstantInt::get(this->LongType, 0),
                                    numLocalCounter, false, entryInst);
  pStore->setAlignment(8);

  NumOptCost += bbGraph.instrumentLocalCounterUpdate(numLocalCounter,
                                                     this->numGlobalCost);
}

void LoopRWCostInstrumentor::InlineGlobalCostForCallee(Function *pFunction)
{
  if (pFunction->getName() == "JS_Assert")
  {
    return;
  }

  if (pFunction->begin() == pFunction->end())
  {
    return;
  }

  BBProfilingGraph bbGraph = BBProfilingGraph(*pFunction);
  bbGraph.init();
  bbGraph.splitNotExitBlock();

  bbGraph.calculateSpanningTree();
  BBProfilingEdge *pQueryEdge = bbGraph.addQueryChord();
  bbGraph.calculateChordIncrements();

  Instruction *entryInst = pFunction->getEntryBlock().getFirstNonPHI();
  AllocaInst *numLocalCounter =
      new AllocaInst(this->LongType, 0, "LOCAL_COST_BB", entryInst);
  numLocalCounter->setAlignment(8);
  StoreInst *pStore = new StoreInst(ConstantInt::get(this->LongType, 0),
                                    numLocalCounter, false, entryInst);
  pStore->setAlignment(8);

  NumOptCost += bbGraph.instrumentLocalCounterUpdate(numLocalCounter,
                                                     this->numGlobalCost);
}

void LoopRWCostInstrumentor::InlineOutputCost(Instruction *InsertBefore)
{

  auto pLoad = new LoadInst(this->numGlobalCost, "", false, InsertBefore);
  InlineSetRecord(pLoad, this->ConstantInt0, this->ConstantInt0, InsertBefore);
}

void LoopRWCostInstrumentor::InstrumentMain(StringRef funcName)
{

  AttributeList emptyList;

  Function *pFunctionMain = this->pModule->getFunction(funcName);

  if (!pFunctionMain)
  {
    errs() << "Cannot find the main function\n";
    return;
  }

  // Instrument to entry block
  {
    CallInst *pCall;
    StoreInst *pStore;

    BasicBlock &entryBlock = pFunctionMain->getEntryBlock();

    // Location: before FirstNonPHI of main function.
    Instruction *firstInst = pFunctionMain->getEntryBlock().getFirstNonPHI();

    // Instrument InitMemHooks
    pCall = CallInst::Create(this->InitMemHooks, "", firstInst);
    pCall->setCallingConv(CallingConv::C);
    pCall->setTailCall(false);
    pCall->setAttributes(emptyList);

    // Store to pcBuffer_CPI
    pStore = new StoreInst(pCall, this->pcBuffer_CPI, false, firstInst);
    pStore->setAlignment(8);

    // Instrument getenv
    vector<Value *> vecParam;
    vecParam.push_back(SAMPLE_RATE_ptr);
    pCall = CallInst::Create(this->getenv, vecParam, "", firstInst);
    pCall->setCallingConv(CallingConv::C);
    pCall->setTailCall(false);
    AttributeList func_get_env_PAL;
    {
      SmallVector<AttributeList, 4> Attrs;
      AttributeList PAS;
      {
        AttrBuilder B;
        B.addAttribute(Attribute::NoUnwind);
        PAS = AttributeList::get(pModule->getContext(), ~0U, B);
      }
      Attrs.push_back(PAS);
      func_get_env_PAL = AttributeList::get(pModule->getContext(), Attrs);
    }
    pCall->setAttributes(func_get_env_PAL);

    // Instrument atoi
    pCall = CallInst::Create(this->function_atoi, pCall, "", firstInst);
    pCall->setCallingConv(CallingConv::C);
    pCall->setTailCall(false);
    AttributeList func_atoi_PAL;
    {
      SmallVector<AttributeList, 4> Attrs;
      AttributeList PAS;
      {
        AttrBuilder B;
        B.addAttribute(Attribute::NoUnwind);
        B.addAttribute(Attribute::ReadOnly);
        PAS = AttributeList::get(pModule->getContext(), ~0U, B);
      }

      Attrs.push_back(PAS);
      func_atoi_PAL = AttributeList::get(pModule->getContext(), Attrs);
    }
    pCall->setAttributes(func_atoi_PAL);

    // SAMPLE_RATE = atoi(getenv("SAMPLE_RATE"));
    pStore = new StoreInst(pCall, SAMPLE_RATE, false, firstInst);
    pStore->setAlignment(4);
  }

  CallInst *pCall;

  for (auto &BB : *pFunctionMain)
  {
    for (auto &II : BB)
    {
      Instruction *pInst = &II;
      // Instrument FinalizeMemHooks before return/unreachable/exit...
      if (isa<ReturnInst>(pInst) || isExitInst(pInst))
      {
        // Output numGlobalCost before exit.
        InlineOutputCost(pInst);

        auto pLoad = new LoadInst(this->iBufferIndex_CPI, "", false, pInst);
        pLoad->setAlignment(8);
        pCall = CallInst::Create(this->FinalizeMemHooks, pLoad, "", pInst);
        pCall->setCallingConv(CallingConv::C);
        pCall->setTailCall(false);
        pCall->setAttributes(emptyList);
      }
    }
  }
}
