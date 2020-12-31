#ifndef PRODUCTIONRUN_MONITOREDINSTS_H
#define PRODUCTIONRUN_MONITOREDINSTS_H

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

namespace common
{
    struct MonitoredRWInsts
    {
        std::map<llvm::LoadInst *, unsigned> mapLoadID;
        std::map<llvm::StoreInst *, unsigned> mapStoreID;
        std::map<llvm::MemSetInst *, unsigned> mapMemSetID;
        std::map<llvm::MemTransferInst *, unsigned> mapMemTransferID;
        std::map<llvm::Instruction *, unsigned> mapFgetcID;
        std::map<llvm::Instruction *, unsigned> mapFreadID;
        std::map<llvm::Instruction *, unsigned> mapOstreamID;

        unsigned long size();
        bool empty();
        bool add(llvm::Instruction *pInst);
        void clear();
        void print(llvm::raw_ostream &os);
        void dump();
        void diff(MonitoredRWInsts &rhs);
    };

    bool isMonitoredLoad(llvm::LoadInst *pLoad);
    bool isMonitoredStore(llvm::StoreInst *pStore);
    llvm::Function *getCalleeFunc(llvm::Instruction *pInst);
    llvm::Function *getCalleeFuncWithBitcast(llvm::Instruction *pInst);
    bool isUnreachableInst(llvm::Instruction *pInst);
    bool isExitInst(llvm::Instruction *pInst);

} // namespace common
#endif //PRODUCTIONRUN_MONITOREDINSTS_H