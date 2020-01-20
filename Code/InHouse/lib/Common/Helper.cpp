#include "Common/Helper.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/IntrinsicInst.h"

#include "Common/Constant.h"

using namespace llvm;
using namespace std;

unsigned GetFunctionID(Function *F) {

    if (F->begin() == F->end()) {
        return INVALID_ID;
    }

    BasicBlock *EntryBB = &(F->getEntryBlock());

    if (EntryBB) {

        for (BasicBlock::iterator II = EntryBB->begin(); II != EntryBB->end(); II++) {
            Instruction *Inst = &*II;
            MDNode *Node = Inst->getMetadata("func_id");
            if (!Node) {
                continue;
            }
            assert(Node->getNumOperands() == 1);
            const Metadata *MD = Node->getOperand(0);
            if (auto *MDV = dyn_cast<ValueAsMetadata>(MD)) {
                Value *V = MDV->getValue();
                ConstantInt *CI = dyn_cast<ConstantInt>(V);
                assert(CI);
                uint64_t rawID = CI->getZExtValue();
                assert(MIN_ID <= rawID && rawID <= MAX_ID);
                return (unsigned)rawID;
            }
        }
    }

    return INVALID_ID;
}

unsigned GetBasicBlockID(BasicBlock *BB) {

    if (BB->begin() == BB->end()) {
        return INVALID_ID;
    }

    for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {
        Instruction *I = &*II;

        MDNode *Node = I->getMetadata("bb_id");
        if (!Node) {
            continue;
        }

        assert(Node->getNumOperands() == 1);
        const Metadata *MD = Node->getOperand(0);
        if (auto *MDV = dyn_cast<ValueAsMetadata>(MD)) {
            Value *V = MDV->getValue();
            ConstantInt *CI = dyn_cast<ConstantInt>(V);
            assert(CI);
            uint64_t rawID = CI->getZExtValue();
            assert(MIN_ID <= rawID && rawID <= MAX_ID);
            return (unsigned)rawID;
        }
    }

    return INVALID_ID;
}

unsigned GetInstructionID(Instruction *II) {

    MDNode *Node = II->getMetadata("ins_id");
    if (!Node) {
        return INVALID_ID;
    }

    assert(Node->getNumOperands() == 1);
    const Metadata *MD = Node->getOperand(0);
    if (auto *MDV = dyn_cast<ValueAsMetadata>(MD)) {
        Value *V = MDV->getValue();
        ConstantInt *CI = dyn_cast<ConstantInt>(V);
        assert(CI);
        uint64_t rawID = CI->getZExtValue();
        assert(MIN_ID <= rawID && rawID <= MAX_ID);
        return (unsigned)rawID;
    }

    return INVALID_ID;
}


unsigned GetLoopID(Loop* loop) {

    BasicBlock *pLoopHeader = loop->getHeader();

    for (BasicBlock::iterator II = pLoopHeader->begin(); II != pLoopHeader->end(); II++) {

        Instruction *Inst = &*II;
        MDNode *Node = Inst->getMetadata("loop_id");
        if (!Node) {
            continue;
        }

        assert(Node->getNumOperands() == 1);
        const Metadata *MD = Node->getOperand(0);
        if (auto *MDV = dyn_cast<ValueAsMetadata>(MD)) {
            Value *V = MDV->getValue();
            ConstantInt *CI = dyn_cast<ConstantInt>(V);
            assert(CI);
            uint64_t rawID = CI->getZExtValue();
            assert(MIN_ID <= rawID && rawID <= MAX_ID);
            return (unsigned)rawID;
        }
    }

    return INVALID_ID;
}

int GetInstructionInsertFlag(Instruction *II) {

    MDNode *Node = II->getMetadata(APROF_INSERT_FLAG);
    if (!Node) {
        return INVALID_ID;
    }

    assert(Node->getNumOperands() == 1);
    const Metadata *MD = Node->getOperand(0);
    if (auto *MDV = dyn_cast<ValueAsMetadata>(MD)) {
        Value *V = MDV->getValue();
        ConstantInt *CI = dyn_cast<ConstantInt>(V);
        assert(CI);
        return CI->getZExtValue();
    }

    return INVALID_ID;
}

bool getIgnoreOptimizedFlag(Function *F) {

    if (F->begin() == F->end()) {
        return false;
    }

    BasicBlock *entryBB = &*(F->begin());

    if (entryBB) {

        for (BasicBlock::iterator II = entryBB->begin(); II != entryBB->end(); II++) {

            Instruction *Inst = &*II;

            MDNode *Node = Inst->getMetadata(IGNORE_OPTIMIZED_FLAG);

            if (Node) {

                return true;
            }
        }

    }

    return false;

}

bool IsIgnoreFunc(Function *F) {

    if (!F) {
        return true;
    }

    if (F->isDeclaration()) {
        return true;
    }

    if (F->begin() == F->end()) {
        return true;
    }

    if (F->getSection().str() == ".text.startup") {
        return true;
    }

//    std::string FuncName = F->getName().str();
//
//    if (FuncName.substr(0, 5) == "aprof") {
//        return true;
//    }

    auto FuncID = GetFunctionID(F);

    if (FuncID == INVALID_ID) {

        return true;
    }

    return false;
}

bool IsIgnoreInst(Instruction *I) {

    if (!I) {
        return true;
    }

    if (isa<DbgInfoIntrinsic>(I)) {
        return true;
    }

    auto InstID = GetInstructionID(I);

    if (InstID == INVALID_ID) {
        return true;
    }

    return false;
}

bool IsClonedFunc(Function *F) {

    if (!F) {
        return false;
    }

    std::string funcName = F->getName().str();

    // FIXME::
    if (funcName == "main") {
        return true;
    }

    if (funcName.length() > 7 &&
        funcName.substr(0, 7) == CLONE_FUNCTION_PREFIX) {
        return true;
    }

    return false;

}

int GetBBCostNum(BasicBlock *BB) {

    MDNode *Node;

    for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {

        Instruction *Inst = &*II;
        MDNode *Node = Inst->getMetadata(BB_COST_FLAG);
        if (!Node) {
            continue;
        }

        assert(Node->getNumOperands() == 1);
        const Metadata *MD = Node->getOperand(0);
        if (auto *MDV = dyn_cast<ValueAsMetadata>(MD)) {
            Value *V = MDV->getValue();
            ConstantInt *CI = dyn_cast<ConstantInt>(V);
            assert(CI);
            return CI->getZExtValue();
        }
    }

    return INVALID_ID;

}

unsigned long getExitBlockSize(Function *pFunc) {

    std::vector<BasicBlock *> exitBlocks;

    for (Function::iterator itBB = pFunc->begin(); itBB != pFunc->end(); itBB++) {
        BasicBlock *BB = &*itBB;

        if (isa<ReturnInst>(BB->getTerminator())) {
            exitBlocks.push_back(BB);
        }
    }

    return exitBlocks.size();
}

/*
 * UnifiedUnreachableBlock, must run behind merge return pass.
 */
bool hasUnifiedUnreachableBlock(Function *F) {

    for (Function::iterator BI = F->begin(); BI != F->end(); BI++) {

        BasicBlock *BB = &*BI;

        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {

            Instruction *Inst = &*II;

            if (Inst->getOpcode() == Instruction::Br) {

                BranchInst *BrInst = dyn_cast<BranchInst>(Inst);

                if (BrInst->isConditional() || BrInst->getNumOperands() > 1) {  // TODO: contains?
                    continue;

                } else {

                    if (BrInst->getNumOperands() == 1) {
                        if (BrInst->getOperand(0)->getName() == "UnifiedUnreachableBlock") {
                            return true;
                        }
                    }
                }
            } else if (Inst->getOpcode() == Instruction::Unreachable) {
                return true;
            }
        }
    }

    return false;
}

bool IsRecursiveCall(std::string callerName, std::string calleeName) {

    long nameLength = calleeName.length();

    if (callerName == calleeName) {
        return true;
    }

    if (callerName.length() > 7 &&
        callerName.substr(0, 7) == CLONE_FUNCTION_PREFIX) {
        if (callerName.substr(7, nameLength) == calleeName) {
            return true;
        }
    }

    return false;
}

bool IsRecursiveCall(Function *F) {

    std::string FName = F->getName().str();
    for (Function::iterator BI = F->begin(); BI != F->end(); BI++) {

        BasicBlock *BB = &*BI;
        for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {
            Instruction *Inst = &*II;

            if (Inst->getOpcode() == Instruction::Call) {

                CallSite ci(Inst);
                Function *Callee = dyn_cast<Function>(
                        ci.getCalledValue()->stripPointerCasts());

                if (Callee) {
                    std::string funcName = Callee->getName().str();

                    // maybe clone function
                    if (IsRecursiveCall(FName, funcName)) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}


std::string getFileNameForInstruction(Instruction *pInst) {
    const DILocation *DIL = pInst->getDebugLoc();

    if (!DIL)
        return "";

    char pPath[400];

    std::string sFileName = DIL->getDirectory().str() + "/" + DIL->getFilename().str();
    realpath(sFileName.c_str(), pPath);
    sFileName = std::string(sFileName);
    return sFileName;

}

std::string printSrcCodeInfo(Instruction *pInst) {
    const DILocation *DIL = pInst->getDebugLoc();

    if (!DIL)
        return "";

    char pPath[400];

    std::string sFileName = DIL->getDirectory().str() + "/" + DIL->getFilename().str();
    realpath(sFileName.c_str(), pPath);
    sFileName = std::string(sFileName);
    unsigned int numLine = DIL->getLine();
    return sFileName + ": " + std::to_string(numLine);

}

std::string printSrcCodeInfo(Function *F) {
    if (F) {
        for (Function::iterator BI = F->begin(); BI != F->end(); BI++) {

            BasicBlock *BB = &*BI;
            for (BasicBlock::iterator II = BB->begin(); II != BB->end(); II++) {
                Instruction *Inst = &*II;

                std::string SrcCode = printSrcCodeInfo(Inst);
                if (SrcCode != "") {
                    return SrcCode;
                }
            }
        }
    }
    return "null";
}


std::string getClonedFunctionName(Module *M, std::string FuncName) {

    unsigned long nameLength = FuncName.length();

    for (Module::iterator FI = M->begin(); FI != M->end(); FI++) {
        Function *Func = &*FI;

        std::string func_name = Func->getName().str();
        if (func_name.length() > 7 &&
            func_name.substr(0, 7) == CLONE_FUNCTION_PREFIX) {
            if (func_name.substr(7, nameLength) == FuncName) {
                return func_name;
            }
        }
    }

    return "";
}

