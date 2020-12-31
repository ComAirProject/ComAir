#include "Common/IDHelper.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/Support/Casting.h"
#include "Common/Constants.h"

using namespace llvm;

namespace common
{
    /// Instruction *I must be valid
    static unsigned GetIDHelper(Instruction *I, StringRef IDName)
    {
        MDNode *N = I->getMetadata(IDName);
        if (!N)
        {
            return INVALID_ID;
        }
        assert(N->getNumOperands() == 1);
        const Metadata *MD = N->getOperand(0);
        if (auto *MDV = dyn_cast<ValueAsMetadata>(MD))
        {
            Value *V = MDV->getValue();
            ConstantInt *CI = dyn_cast<ConstantInt>(V);
            assert(CI);
            uint64_t RawID = CI->getZExtValue();
            assert(MIN_ID <= RawID && RawID <= MAX_ID);
            return (unsigned)RawID;
        }
        return INVALID_ID;
    }

    unsigned GetFunctionID(Function *F)
    {
        if (!F || F->begin() == F->end())
        {
            return INVALID_ID;
        }
        BasicBlock *EntryBB = &F->getEntryBlock();
        if (!EntryBB)
        {
            return INVALID_ID;
        }
        for (Instruction &I : *EntryBB)
        {
            unsigned FuncID = GetIDHelper(&I, "func_id");
            if (FuncID != INVALID_ID)
            {
                return FuncID;
            }
        }
        return INVALID_ID;
    }

    unsigned GetBasicBlockID(BasicBlock *B)
    {
        if (!B || B->begin() == B->end())
        {
            return INVALID_ID;
        }
        for (Instruction &I : *B)
        {
            unsigned BBID = GetIDHelper(&I, "bb_id");
            if (BBID != INVALID_ID)
            {
                return BBID;
            }
        }
        return INVALID_ID;
    }

    unsigned GetInstructionID(Instruction *I)
    {
        if (!I)
        {
            return INVALID_ID;
        }
        return GetIDHelper(I, "ins_id");
    }

    unsigned GetLoopID(Loop *L)
    {
        if (!L)
        {
            return INVALID_ID;
        }
        BasicBlock *LoopHeader = L->getHeader();
        if (!LoopHeader)
        {
            return INVALID_ID;
        }
        for (Instruction &I : *LoopHeader)
        {
            unsigned LoopID = GetIDHelper(&I, "loop_id");
            if (LoopID != INVALID_ID)
            {
                return LoopID;
            }
        }
        return INVALID_ID;
    }
} // namespace common