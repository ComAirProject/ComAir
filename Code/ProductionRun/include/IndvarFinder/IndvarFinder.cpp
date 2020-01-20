#include "LoopSampler/IndvarFinder/IndvarFinder.h"

#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/Support/FileSystem.h>
#include "Common/ArrayLinkedIndentifier.h"
#include "Common/Helper.h"

using namespace llvm;
using std::map;

static RegisterPass<IndvarFinder> X("indvar-find",
                                    "find indvar and its stride in loop",
                                    true, false);

static cl::opt<unsigned> uSrcLine("noLine",
                                  cl::desc("Source Code Line Number for the Loop"), cl::Optional,
                                  cl::value_desc("uSrcCodeLine"));

static cl::opt<std::string> strFileName("strFile",
                                        cl::desc("File Name for the Loop"), cl::Optional,
                                        cl::value_desc("strFileName"));

static cl::opt<std::string> strFuncName("strFunc",
                                        cl::desc("Function Name"), cl::Optional,
                                        cl::value_desc("strFuncName"));

static const char *outputFilePath = "indvar.pre.info";

char IndvarFinder::ID = 0;

IndvarFinder::IndvarFinder() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeScalarEvolutionWrapperPassPass(Registry);
}

void IndvarFinder::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<ScalarEvolutionWrapperPass>();
}

bool IndvarFinder::runOnModule(Module &M) {

    std::error_code ec;
    raw_fd_ostream myfile(outputFilePath, ec, sys::fs::OpenFlags::F_RW);

    Function *pFunction = searchFunctionByName(M, strFileName, strFuncName, uSrcLine);
    if (!pFunction) {
        errs() << "Cannot find the input function\n";
        return false;
    }

    LoopInfo &LoopInfo = getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();
    ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>(*pFunction).getSE();

    Loop *pLoop = searchLoopByLineNo(pFunction, &LoopInfo, uSrcLine);
    if (!pLoop) {
        errs() << "Cannot find the loop\n";
        return false;
    }

    auto vecInstSCEV = searchInstSCEV(pLoop, SE);

    for (auto &is : vecInstSCEV) {
        auto stride = searchStride(is.pSCEV, SE);
        if (!stride) {
            errs() << "Cannot find the Stride\n";
            continue;
        }
        const Instruction *pInst = is.pInst;

        int indvarInstID = GetInstructionID(const_cast<Instruction *>(pInst));
        if (indvarInstID == -1) {
            continue;
        }
        errs() << indvarInstID << '\t';
        errs() << *stride << '\n';
        pInst->dump();

        myfile << indvarInstID << '\t';
        myfile << *stride << '\n';
    }

    myfile.close();

    return false;
}

std::vector<InstSCEV> IndvarFinder::searchInstSCEV(llvm::Loop *pLoop, llvm::ScalarEvolution &SE) {

    std::vector<InstSCEV> vecInstSCEV;

    for (auto &BB : pLoop->getBlocks()) {
        for (auto &II : *BB) {
            Instruction *pInst = &II;

            Value *operand = nullptr;
            errs() << GetInstructionID(pInst);
            pInst->dump();
            if (pInst->getOpcode() == Instruction::Load) {
                operand = pInst->getOperand(0);
            } else if (pInst->getOpcode() == Instruction::Store) {
                operand = pInst->getOperand(1);
            }

            if (operand) {
                if (SE.isSCEVable(operand->getType())) {
                    const SCEV *pSCEV = SE.getSCEV(operand);
                    vecInstSCEV.push_back(InstSCEV{pInst, pSCEV});
                }
            }
        }
    }

    return vecInstSCEV;
}

const llvm::SCEV *IndvarFinder::searchStride(const llvm::SCEV *scev, llvm::ScalarEvolution &SE) {

    struct FindStride {
        bool foundStride = false;
        const SCEV *stride = nullptr;

        explicit FindStride(ScalarEvolution &SE) : _SE(SE) {

        }

        bool follow(const SCEV *scev) {
            auto addrec = dyn_cast<SCEVAddRecExpr>(scev);
            if (addrec) {
                stride = addrec->getStepRecurrence(_SE);
                foundStride = true;
            }
            return true;
        }

        bool isDone() const {
            return foundStride;
        }

    private:
        ScalarEvolution &_SE;
    };

    FindStride f(SE);
    SCEVTraversal<FindStride> st(f);
    st.visitAll(scev);

    if (f.foundStride) {
        return f.stride;
    } else {
        return nullptr;
    }
}