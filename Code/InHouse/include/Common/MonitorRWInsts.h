#ifndef INHOUSE_MONITORRWINSTS_H
#define INHOUSE_MONITORRWINSTS_H

#include <map>

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"

namespace common {

    struct MonitoredRWInsts {

        std::map<llvm::LoadInst *, unsigned> mapLoadID;
        std::map<llvm::StoreInst *, unsigned> mapStoreID;
        std::map<llvm::MemSetInst *, unsigned> mapMemSetID;
        std::map<llvm::MemTransferInst *, unsigned> mapMemTransferID;
        std::map<llvm::Instruction *, unsigned> mapFgetcID;
        std::map<llvm::Instruction *, unsigned> mapFreadID;
        std::map<llvm::Instruction *, unsigned> mapOstreamID;

        unsigned size();

        bool empty();

        bool add(llvm::Instruction *pInst);

        void clear();

        void print(llvm::raw_ostream &os);

        void dump();

        void diff(MonitoredRWInsts &rhs);
    };

    llvm::Function *getCalleeFunc(llvm::Instruction *pInst);

    bool isMonitoredLoad(llvm::LoadInst *pLoad);

    bool isMonitoredStore(llvm::StoreInst *pStore);

    bool isUnreachableInst(llvm::Instruction *pInst);
}

#endif //INHOUSE_MONITORRWINSTS_H
