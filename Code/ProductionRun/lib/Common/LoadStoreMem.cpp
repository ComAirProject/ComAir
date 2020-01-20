#include "Common/LoadStoreMem.h"

#include "llvm/IR/Instructions.h"

using namespace llvm;

namespace common {

    bool hasNonLoadStoreUse(llvm::Value *V) {
        for (auto UI = V->user_begin(); UI != V->user_end(); ++UI) {
            if (!(isa<LoadInst>(*UI) || isa<StoreInst>(*UI))) {
                return true;
            }
        }
        return false;
    }


}