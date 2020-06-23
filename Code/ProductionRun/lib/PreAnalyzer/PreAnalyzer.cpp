#include "PreAnalyzer/PreAnalyzer.h"

#include <fstream>
#include <vector>
#include <map>
#include <set>

#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Transforms/Utils/CtorUtils.h"

#include "Common/Helper.h"

using namespace llvm;
using std::vector;
using std::map;
using std::set;

#define DEBUG_TYPE "PreAnalyzer"

static RegisterPass<PreAnalyzer> X("-analyze",
                                        "analyze function",
                                        false, false);

static cl::opt<std::string> strFuncName("strFunc",
                                        cl::desc("Function Name"), cl::Optional,
                                        cl::value_desc("strFuncName"));

char PreAnalyzer::ID = 0;

PreAnalyzer::PreAnalyzer() : ModulePass(ID) {
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeDominatorTreeWrapperPassPass(Registry);
    initializeLoopInfoWrapperPassPass(Registry);
}

void PreAnalyzer::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
}

std::set<Loop *> LoopSet; /*Set storing loop and subloop */

void getSubLoopSet(Loop *lp) {

    vector<Loop *> workList;
    if (lp != NULL) {
        workList.push_back(lp);
    }

    while (workList.size() != 0) {

        Loop *loop = workList.back();
        LoopSet.insert(loop);
        workList.pop_back();

        if (loop != nullptr && !loop->empty()) {

            std::vector<Loop *> &subloopVect = lp->getSubLoopsVector();
            if (subloopVect.size() != 0) {
                for (std::vector<Loop *>::const_iterator SI = subloopVect.begin(); SI != subloopVect.end(); SI++) {
                    if (*SI != NULL) {
                        if (LoopSet.find(*SI) == LoopSet.end()) {
                            workList.push_back(*SI);
                        }
                    }
                }

            }
        }
    }
}

void getLoopSet(Loop *lp) {
    if (lp != NULL && lp->getHeader() != NULL && !lp->empty()) {
        LoopSet.insert(lp);
        const std::vector<Loop *> &subloopVect = lp->getSubLoops();
        if (!subloopVect.empty()) {
            for (std::vector<Loop *>::const_iterator subli = subloopVect.begin(); subli != subloopVect.end(); subli++) {
                Loop *subloop = *subli;
                getLoopSet(subloop);
            }
        }
    }
}

bool PreAnalyzer::runOnModule(Module &M) {
    return false;
}




