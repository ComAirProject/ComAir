#ifndef PRODUCTIONRUN_LOOPSAMPLEINSTRUMENTOR_H
#define PRODUCTIONRUN_LOOPSAMPLEINSTRUMENTOR_H

#include "LoopSampler/LoopBaseInstrumentor/LoopBaseInstrumentor.h"
#include "LoopSampler/LoopSampleComp/LoopSampleComp.h"
//#include <llvm/Pass.h>
//#include <llvm/Analysis/LoopInfo.h>
//#include <llvm/Analysis/AliasAnalysis.h>
//#include <llvm/IR/Instruction.h>
//#include <llvm/IR/Constants.h>
//#include <llvm/Transforms/Utils/ValueMapper.h>
//#include <llvm/Analysis/AliasSetTracker.h>
//#include "Common/MonitorRWInsts.h"

namespace loopsampler {
struct LoopSampleInstrumentor : public LoopBaseInstrumentor
{
    static char ID;

    bool runOnModule(llvm::Module &M) override final;
};
}

#endif //PRODUCTIONRUN_LOOPSAMPLEINSTRUMENTOR_H
