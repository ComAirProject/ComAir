#ifndef PRODUCTIONRUN_LOADSTOREMEM_H
#define PRODUCTIONRUN_LOADSTOREMEM_H

#include <map>
#include <set>

#include "llvm/IR/Value.h"

namespace common {

    bool hasNonLoadStoreUse(llvm::Value *V);

//    bool mergeLoadStore(std::map<llvm::LoadInst *, unsigned> &mapLI,
//            std::set<llvm::StoreInst *, unsigned> &mapSI,
//            std::map<Value *, std::set<Instruction *>> &mapMerged);
}

#endif //PRODUCTIONRUN_LOADSTOREMEM_H
