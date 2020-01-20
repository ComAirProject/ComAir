#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"

#include "llvm/Transforms/IPO/FunctionAttrs.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include "Optimization/Optimize.h"

using namespace std;

static RegisterPass<OptimizeManager> X("apply-optimize",
                                       "apply some optimize pass to compile bit code", true, true);

static cl::opt<bool>
        DisableOptimizations("disable-opt",
                             cl::desc("Do not run any optimization passes"));

char OptimizeManager::ID = 0;


void OptimizeManager::getAnalysisUsage(AnalysisUsage &AU) const {

}

OptimizeManager::OptimizeManager() : ModulePass(ID) {

}

void OptimizeManager::AddPass(Pass *p) {
    this->PM.add(p);
}


void OptimizeManager::SetUpOptimizePasses() {

    AddPass(createCFGSimplificationPass()); // Clean up disgusting code
    AddPass(createPromoteMemoryToRegisterPass()); // Kill useless allocas
    AddPass(createGlobalOptimizerPass());      // Optimize out global vars
    AddPass(createGlobalDCEPass());            // Remove unused fns and globs
    AddPass(createIPConstantPropagationPass());// IP Constant Propagation
    AddPass(createDeadArgEliminationPass());   // Dead argument elimination
    AddPass(createInstructionCombiningPass()); // Clean up after IPCP & DAE
    AddPass(createCFGSimplificationPass());    // Clean up after IPCP & DAE

    AddPass(createCFGSimplificationPass());    // Merge & remove BBs
    AddPass(createReassociatePass());          // Reassociate expressions
    AddPass(createLoopRotatePass());
    AddPass(createLICMPass());                 // Hoist loop invariants
    AddPass(createLoopUnswitchPass());         // Unswitch loops.
    // FIXME : Removing instcombine causes nestedloop regression.
    AddPass(createInstructionCombiningPass());
    AddPass(createIndVarSimplifyPass());       // Canonicalize indvars
    AddPass(createLoopDeletionPass());         // Delete dead loops
    AddPass(createLoopUnrollPass());           // Unroll small loops
    AddPass(createInstructionCombiningPass()); // Clean up after the unroller
    AddPass(createGVNPass());                  // Remove redundancies
    AddPass(createMemCpyOptPass());            // Remove memcpy / form memset
    AddPass(createSCCPPass());                 // Constant prop with SCCP

    // Run inst combine after redundancy elimination to exploit opportunities
    // opened up by them.
    AddPass(createInstructionCombiningPass());

    AddPass(createDeadStoreEliminationPass()); // Delete dead stores
    AddPass(createAggressiveDCEPass());        // Delete dead instructions
    AddPass(createCFGSimplificationPass());    // Merge & remove BBs
    AddPass(createStripDeadPrototypesPass());  // Get rid of dead prototypes
    AddPass(createConstantMergePass());        // Merge dup global constants
}

bool OptimizeManager::runOnModule(Module &M) {
    SetUpOptimizePasses();
    this->PM.run(M);
//    this->PB.
}