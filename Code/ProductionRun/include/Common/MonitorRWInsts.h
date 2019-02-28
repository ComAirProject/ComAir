#ifndef NEWCOMAIR_MONITORRWINSTS_H
#define NEWCOMAIR_MONITORRWINSTS_H

#include <set>
#include <map>
#include <vector>
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "Common/Helper.h"

using std::set;
using std::map;
using std::vector;
using llvm::Function;
using llvm::Instruction;
using llvm::LoadInst;
using llvm::StoreInst;
using llvm::MemSetInst;
using llvm::MemTransferInst;
using llvm::DominatorTree;
using llvm::ValueToValueMapTy;

struct MonitoredRWInsts {

    map<LoadInst *, unsigned> mapLoadID;
    map<StoreInst *, unsigned> mapStoreID;
    map<MemSetInst *, unsigned> mapMemSetID;
    map<MemTransferInst *, unsigned> mapMemTransferID;
    map<Instruction *, unsigned> mapFgetcID;
    map<Instruction *, unsigned> mapFreadID;
    map<Instruction *, unsigned> mapOstreamID;

    bool empty();
    bool add(Instruction *pInst);
    void clear();
    void print(llvm::raw_ostream& os);
    void dump();
    void diff(MonitoredRWInsts &rhs);
};

int mapFromOriginToCloned(ValueToValueMapTy &originClonedMapping, MonitoredRWInsts &origin, MonitoredRWInsts &cloned);

Function *getCalleeFunc(Instruction *pInst);
bool isMonitoredLoad(LoadInst *pLoad);
bool isMonitoredStore(StoreInst *pStore);
bool isReturnLikeInst(Instruction *pInst);
bool isExitInst(Instruction *pInst);

#endif //NEWCOMAIR_MONITORRWINSTS_H
