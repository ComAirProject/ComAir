#include "FuncInlinePass/FuncInlinePass.h"

#include <set>

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace func_inline_pass {

    static cl::opt<int> libInline("lib-inline",
                                  cl::desc("Inline Runtime Lib"),
                                  cl::init(1));

    int MAX_BB_SIZE = 100;

    static int getBBNum(Function &F) {

        int num = 0;
        for (BasicBlock &B : F) {
            ++num;
        }
        return num;
    }

    char FuncInlinePass::ID = 0;

    FuncInlinePass::FuncInlinePass() : ModulePass(ID) {}

    void FuncInlinePass::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
        AU.setPreservesAll();
    }

    bool FuncInlinePass::runOnModule(llvm::Module &M) {

        std::set<std::string> setRuntimeInlineFuncName = {
                "aprof_query_insert_page_table"
        };

        std::set<std::string> setHookInlineFuncName = {
                "aprof_init",
                "aprof_write",
                "aprof_read",
                "aprof_increment_rms",
                "aprof_call_before",
                "aprof_return",
                "aprof_final"
        };

        std::set<CallInst *> setCallInstToInline;

        for (Function &F : M) {
            if (getBBNum(F) > MAX_BB_SIZE) {
                continue;
            }
            for (BasicBlock &B : F) {
                for (Instruction &I : B) {
                    if (isa<CallInst>(&I)) {

                        CallSite cs(&I);
                        Function *callee = dyn_cast<Function>(cs.getCalledValue()->stripPointerCasts());

                        if (libInline == 1) {
                            if (callee && setRuntimeInlineFuncName.find(callee->getName().str()) !=
                                          setRuntimeInlineFuncName.end()) {
                                CallInst *pCall = dyn_cast<CallInst>(&I);
                                setCallInstToInline.insert(pCall);
                            }
                        } else {
                            if (callee &&
                                setHookInlineFuncName.find(callee->getName().str()) != setHookInlineFuncName.end()) {
                                CallInst *pCall = dyn_cast<CallInst>(&I);
                                setCallInstToInline.insert(pCall);
                            }
                        }
                    }
                }
            }
        }

        for (CallInst *pCall: setCallInstToInline) {

            InlineFunctionInfo IFI;
            bool changed = InlineFunction(pCall, IFI);

            if (!changed) {
                pCall->dump();
                errs() << "error inline!" << "\n";
                return false;
            }
        }

        return true;
    }
}

static RegisterPass<func_inline_pass::FuncInlinePass> X(
        "func-inline",
        "Inline Instrumented Function",
        true,
        true);




