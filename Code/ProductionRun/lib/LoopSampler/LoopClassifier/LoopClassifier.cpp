#include "LoopSampler/LoopClassifier/LoopClassifier.h

#include "Common/ArrayLinkedIndentifier.h"
#include "Common/Helper.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"

using namespace llvm;

static RegisterPass<LoopClassifier>
    X("loop-classify", "classify loop and find indvar and its stride for array",
      true, false);

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

static const char *outputFilePath = "indvar.pre.info";

char LoopClassifier::ID = 0;

LoopClassifier::LoopClassifier() : ModulePass(ID) {
  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializePromoteLegacyPassPass(Registry);
}

void LoopClassifier::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.setPreservesCFG();
}

static bool promoteMemoryToRegister(Function &F, DominatorTree &DT,
                                    AssumptionCache &AC) {
  std::vector<AllocaInst *> Allocas;
  BasicBlock &BB = F.getEntryBlock(); // Get the entry node for the function
  bool Changed = false;

  while (true) {
    Allocas.clear();

    // Find allocas that are safe to promote, by looking at all instructions in
    // the entry node
    for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
      if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) // Is it an alloca?
        if (isAllocaPromotable(AI))
          Allocas.push_back(AI);

    if (Allocas.empty())
      break;

    PromoteMemToReg(Allocas, DT, &AC);
    NumPromoted += Allocas.size();
    Changed = true;
  }
  return Changed;
}

static bool mem2regFunc(Function &F) {
  if (skipFunction(F))
    return false;

  DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  AssumptionCache &AC =
      getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
  return promoteMemoryToRegister(F, DT, AC);
}

bool LoopClassifier::runOnModule(llvm::Module &M) {

  std::error_code ec;
  raw_fd_ostream myfile(outputFilePath, ec, sys::fs::OpenFlags::F_RW);

  Function *pFunction =
      searchFunctionByName(M, strFileName, strFuncName, uSrcLine);
  if (!pFunction) {
    errs() << "Cannot find the input function\n";
    return false;
  }

  LoopInfo &LoopInfo =
      getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();
  Loop *pLoop = searchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);
  if (!pLoop) {
    errs() << "Cannot find the loop\n";
    return false;
  }

  bool IsArray = false;
  bool IsLinkedList = false;

  std::set<Value *> setLoopValues;
  if (IsArrayAccessLoop(pLoop, setLoopValues) ||
      IsArrayAccessLoop1(pLoop, setLoopValues)) {
    IsArray = true;
  } else if (IsLinkedListAccessLoop(pLoop, setLoopValues)) {
    IsLinkedList = true;
  }

  if (isArray) {
  }

  return false;
}