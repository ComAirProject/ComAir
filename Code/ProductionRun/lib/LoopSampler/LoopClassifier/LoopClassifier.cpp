#include "LoopSampler/LoopClassifier/LoopClassifier.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"

#include <set>
#include <utility>

#include "optional.h"
#include "Common/ArrayLinkedIdentifier.h"
#include "Common/Helper.h"

using namespace llvm;

static RegisterPass<LoopClassifier>
    X("loop-classify", "classify loop into array/linkedlist and find indvar, also find stride for array",
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


char LoopClassifier::ID = 0;

LoopClassifier::LoopClassifier() : ModulePass(ID)
{
  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializeLoopInfoWrapperPassPass(Registry);
}

void LoopClassifier::getAnalysisUsage(AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<LoopInfoWrapperPass>();
}

std::experimental::optional<LoadInst *> isLinkedList(Loop *L);
std::experimental::optional<std::pair<Value *, LoadInst *>> isArray(Loop *L);

bool LoopClassifier::runOnModule(llvm::Module &M)
{
  std::string outputFilePath = formatv("loop_{0}_{1}", strFileName.getValue(), uSrcLine.getValue()).str();
  std::error_code ec;
  raw_fd_ostream outfile(outputFilePath, ec, sys::fs::OpenFlags::F_RW);
  Function *pFunction = SearchFunctionByName(M, strFileName, strFuncName, uSrcLine);
  if (!pFunction)
  {
    errs() << "Cannot find the input function\n";
    return false;
  }

  LoopInfo &LoopInfo =
      getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();
  Loop *pLoop = SearchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);
  if (!pLoop)
  {
    errs() << "Cannot find the loop\n";
    return false;
  }

  if (auto arr_data = isArray(pLoop))
  {
    errs() << "isArray\n";
    arr_data->first->dump();
    arr_data->second->dump();
    outfile << "isArray\n";
    arr_data->first->print(outfile);
    outfile << "\n" << GetInstructionID(cast<Instruction>(arr_data->second)) << "\n";
    arr_data->second->print(outfile);
  }

  if (auto list_data = isLinkedList(pLoop))
  {
    errs() << "isLinkedList\n";
    (*list_data)->dump();
    outfile << "isLinkedList\n";
    outfile << GetInstructionID(cast<Instruction>(*list_data)) << "\n";
    (*list_data)->print(outfile);
  }

  return false;
}

/// Decide if a loop is a linkedlist.
/// S = phi(load(GEP(S, 0, _))),
/// where S is of ptr type to Struct
/// and phi is in Loopheader
/// and load, GEP are in Loop
/// Used after passes loop-simplify and mem2reg
std::experimental::optional<LoadInst *> isLinkedList(Loop *L)
{
  BasicBlock *Header = L->getHeader();
  std::set<BasicBlock *> LoopBBs(L->getBlocks().begin(), L->getBlocks().end());
  for (auto &Phi : Header->phis())
  {
    if (!Phi.getType()->isPointerTy())
    {
      continue;
    }
    if (!Phi.getType()->getPointerElementType()->isStructTy())
    {
      continue;
    }
    for (auto &Incm : Phi.incoming_values())
    {
      if (LoadInst *LI = dyn_cast<LoadInst>(Incm))
      {
        if (LoopBBs.find(LI->getParent()) == LoopBBs.end())
        {
          continue;
        }
        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(LI->getPointerOperand()))
        {
          if (LoopBBs.find(GEP->getParent()) == LoopBBs.end())
          {
            continue;
          }
          if (ConstantInt *CI = dyn_cast<ConstantInt>(GEP->getOperand(1)))
          {
            if (!CI->isZero())
            {
              continue;
            }
            if (&Phi == GEP->getPointerOperand())
            {
              return LI;
            }
          }
        }
      }
    }
  }
  return {};
}

/// Decide if a loop is an array which inc/dec index.
/// R = phi(add/sub(R, LoopInvar))
/// (R = add/sub(R, LoopInvar))?
/// (R = castinst(R))?
/// load(GEP(_, R)),
/// where R is of integer type
/// and phi, add/sub, castinst, load, GEP are in loop
///
/// Return: Option<(Step, ArrayIdx)>
/// 1. Step: LoopInvariant
/// 2. ArrayIdx: Load idx ptr to array
std::experimental::optional<std::pair<Value *, LoadInst *>> isIncDecIndex(PHINode &Phi, const std::set<BasicBlock *> &LoopBBs, const Loop *L)
{
  if (!Phi.getType()->isIntegerTy())
  {
    return {};
  }

  Value *Step = nullptr;
  for (auto &Incm : Phi.incoming_values())
  {
    if (Instruction *I = dyn_cast<Instruction>(Incm))
    {
      if (LoopBBs.find(I->getParent()) == LoopBBs.end())
      {
        continue;
      }
      if (I->getOpcode() != Instruction::Add &&
          I->getOpcode() != Instruction::Sub)
      {
        continue;
      }
      if (&Phi != I->getOperand(0))
      {
        continue;
      }
      if (!L->isLoopInvariant(I->getOperand(1)))
      {
        continue;
      }
      Step = I->getOperand(1);

      std::vector<Instruction *> BinOps;
      BinOps.push_back(&Phi);
      for (auto *Usr : Phi.users())
      {
        if (Instruction *UsrI = dyn_cast<Instruction>(Usr))
        {
          if (LoopBBs.find(UsrI->getParent()) == LoopBBs.end())
          {
            continue;
          }
          if (I->getOpcode() != Instruction::Add &&
              I->getOpcode() != Instruction::Sub)
          {
            continue;
          }
          if (!L->isLoopInvariant(I->getOperand(1)))
          {
            continue;
          }
          BinOps.push_back(UsrI);
        }
      }

      std::vector<Instruction *> Casts(BinOps.begin(), BinOps.end());
      for (Instruction *BinOp : BinOps)
      {
        for (auto *Usr : BinOp->users())
        {
          if (Instruction *UsrI = dyn_cast<Instruction>(Usr))
          {
            if (LoopBBs.find(UsrI->getParent()) == LoopBBs.end())
            {
              continue;
            }
            if (UsrI->isCast())
            {
              Casts.push_back(UsrI);
            }
          }
        }
      }

      for (Instruction *Cast : Casts)
      {
        for (User *Usr : Cast->users())
        {
          if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Usr))
          {
            if (LoopBBs.find(GEP->getParent()) == LoopBBs.end())
            {
              continue;
            }
            if (GEP->getNumOperands() != 2)
            {
              continue;
            }
            if (GEP->getOperand(1) != Cast)
            {
              continue;
            }
            for (User *Usr : GEP->users())
            {
              if (LoadInst *LI = dyn_cast<LoadInst>(Usr))
              {
                return std::make_pair(Step, LI);
              }
            }
          }
        }
      }
    }
  }
  return {};
}

/// Decide if a loop is an array which inc/dec ptr.
/// R = phi(GEP(R, LoopIndvar))
/// load(R)/load(GEP2(R, 0, _))
/// where R is of ptr type
/// and phi, GEP, load, GEP2 are in loop
///
/// Return: Option<(Step, ArrayIdx)>
/// 1. Step: LoopInvariant
/// 2. ArrayIdx: Load idx ptr to array
std::experimental::optional<std::pair<Value *, LoadInst *>> isIncDecPtr(PHINode &Phi, const std::set<BasicBlock *> &LoopBBs, const Loop *L)
{
  if (!Phi.getType()->isPointerTy())
  {
    return {};
  }

  Value *Step = nullptr;
  for (auto &Incm : Phi.incoming_values())
  {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Incm))
    {
      if (LoopBBs.find(GEP->getParent()) == LoopBBs.end())
      {
        continue;
      }
      if (GEP->getNumOperands() != 2)
      {
        continue;
      }
      if (&Phi != GEP->getOperand(0))
      {
        continue;
      }
      if (!L->isLoopInvariant(GEP->getOperand(1)))
      {
        continue;
      }
      Step = GEP->getOperand(1);

      std::vector<Instruction *> PhiOrGEPs;
      PhiOrGEPs.push_back(&Phi);
      for (auto *Usr : Phi.users())
      {
        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Usr))
        {
          if (LoopBBs.find(GEP->getParent()) == LoopBBs.end())
          {
            continue;
          }
          if (GEP->getNumOperands() < 3)
          {
            continue;
          }
          if (ConstantInt *CI = dyn_cast<ConstantInt>(GEP->getOperand(1)))
          {
            if (!CI->isZero())
            {
              continue;
            }
            PhiOrGEPs.push_back(GEP);
          }
        }
      }

      for (Instruction *I : PhiOrGEPs)
      {
        for (User *Usr : I->users())
        {
          if (LoadInst *LI = dyn_cast<LoadInst>(Usr))
          {
            return std::make_pair(Step, LI);
          }
        }
      }
    }
  }
  return {};
}

/// Decide if a loop is an array.
/// 1. inc/dec index
/// 2. inc/dec ptr
std::experimental::optional<std::pair<Value *, LoadInst *>> isArray(Loop *L)
{
  BasicBlock *Header = L->getHeader();
  std::set<BasicBlock *> LoopBBs(L->getBlocks().begin(), L->getBlocks().end());
  for (auto &Phi : Header->phis())
  {
    if (auto Res = isIncDecIndex(Phi, LoopBBs, L))
    {
      errs() << "Array Idx\n";
      return Res;
    }
    else if (auto Res = isIncDecPtr(Phi, LoopBBs, L))
    {
      errs() << "Array Ptr\n";
      return Res;
    }
  }
  return {};
}
