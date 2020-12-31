#include "LoopSampler/LoopSampleInstrumentor/LoopSampleInstrumentor.h"

#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ADT/Statistic.h"

#include "Common/Constants.h"
#include "Common/MonitoredInsts.h"
#include "Common/SearchHelper.h"
#include "LoopSampler/LoopSampleComp/LoopSampleComp.h"

#include <fstream>
#include <vector>
#include <map>
#include <set>

using namespace llvm;

#define DEBUG_TYPE "LoopSampleInstrumentor"

STATISTIC(NumNoOptRW, "Num of Non-optimized Read/Write Instrument Sites");
STATISTIC(NumOptRW, "Num of Optimized Read/Write Instrument Sites");
STATISTIC(NumHoistRW, "Num of Hoisted Read/Write Instrument Sites");
STATISTIC(NumNoOptCost, "Num of Non-optimized Cost Instrument Sites");
STATISTIC(NumOptCost, "Num of Optimized Cost Instrument Sites");

static RegisterPass<loopsampler::LoopSampleInstrumentor> X("opt-loop-instrument",
                                                           "instrument a loop with sampling",
                                                           false, false);
namespace loopsampler
{
    static cl::opt<unsigned> uSrcLine("noLine",
                                      cl::desc("Source Code Line Number for the Loop"), cl::Optional,
                                      cl::value_desc("uSrcCodeLine"));

    static cl::opt<std::string> strFileName("strFile",
                                            cl::desc("File Name for the Loop"), cl::Optional,
                                            cl::value_desc("strFileName"));

    static cl::opt<std::string> strFuncName("strFunc",
                                            cl::desc("Function Name"), cl::Optional,
                                            cl::value_desc("strFuncName"));

    static cl::opt<std::string> strEntryFuncName("strEntryFunc",
                                                 cl::desc("Entry Function Name"), cl::Optional,
                                                 cl::value_desc("strEntryFuncName"));

    static cl::opt<bool> bNoOptInst("bNoOptInst", cl::desc("No Opt for Num of Instrument Sites"), cl::Optional,
                                    cl::value_desc("bNoOptInst"));

    static cl::opt<bool> bNoOptCost("bNoOptCost", cl::desc("No Opt for Merging Cost"), cl::Optional,
                                    cl::value_desc("bNoOptCost"));

    static cl::opt<bool> bNoOptLoop("bNoOptLoop", cl::desc("No Opt for Loop"), cl::Optional, cl::value_desc("bNoOptLoop"));

    char LoopSampleInstrumentor::ID = 0;

    bool LoopSampleInstrumentor::runOnModule(Module &M)
    {
        SetupInit(M);

        Function *pFunction = common::SearchFunctionByName(M, strFileName.c_str(), strFuncName.c_str(), uSrcLine);
        if (!pFunction)
        {
            errs() << "Cannot find the function\n";
            return false;
        }

        DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*pFunction).getDomTree();
        LoopInfo &LPI = getAnalysis<LoopInfoWrapperPass>(*pFunction).getLoopInfo();

        Loop *pLoop = common::SearchLoopByLineNo(pFunction, &LPI, uSrcLine);
        if (!pLoop)
        {
            errs() << "Cannot find the loop\n";
            return false;
        }

        std::set<BasicBlock *> setBBInLoop(pLoop->blocks().begin(), pLoop->blocks().end());

        common::MonitoredRWInsts MI;
        for (BasicBlock *BB : setBBInLoop)
        {
           ++NumNoOptCost;
           for (Instruction &II : *BB)
           {
               MI.add(&II);
           }
        }

        NumNoOptRW += MI.size();

        LoopSampleComp LSC(this->SAMPLE_RATE,
                           this->numGlobalCounter,
                           this->geo,
                           this->ConstantInt1,
                           this->ConstantIntN1);

        ValueToValueMapTy VMap;
        BasicBlock *pClonedLoopHeader = LSC.CloneInnerLoop(pLoop, VMap);
        assert(pClonedLoopHeader);

        BasicBlock *pLoopPreheader = LSC.CreateIfElseBlock(pLoop, pClonedLoopHeader);
        assert(pLoopPreheader);

        // Instrument Loop
        InstrumentMonitoredInsts(MI);

        InlineGlobalCostForLoop(setBBInLoop);

        // Find Callees
        std::vector<Function *> vecWorkList;
        std::set<Function *> setCallees;
        std::map<Function *, std::set<Instruction *>> funcCallSiteMapping;
        FindCalleesInDepth(setBBInLoop, setCallees, funcCallSiteMapping);
        LSC.CloneFunctions(setCallees, VMap);
        LSC.RemapFunctionCalls(setCallees, funcCallSiteMapping, VMap);

        // Instrument Callees
        for (Function *Callee : setCallees) {
            // DominatorTree &CalleeDT = getAnalysis<DominatorTreeWrapperPass>(*Callee).getDomTree();
            common::MonitoredRWInsts CalleeMI;
            for (BasicBlock &BB : *Callee)
            {
                ++NumNoOptCost;
                for (Instruction &II : BB)
                {
                    CalleeMI.add(&II);
                }
            }
            NumNoOptRW += CalleeMI.size();
            // if (!bNoOptInst)
            // {
            //     removeByDomInfo(CalleeMI, CalleeDT);
            // }
            NumOptRW += CalleeMI.size();
            InstrumentMonitoredInsts(CalleeMI);
            InlineGlobalCostForCallee(Callee);
        }

        if (strEntryFuncName != "")
        {
            InstrumentMain(strEntryFuncName);
        }
        else
        {
            InstrumentMain("main");
        }

        return false;
    }
}