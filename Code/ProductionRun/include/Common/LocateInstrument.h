#ifndef NEWCOMAIR_COMMON_LOCATEINSTRUMENT_H
#define NEWCOMAIR_COMMON_LOCATEINSTRUMENT_H

#include <vector>
#include <map>

namespace llvm {
    class Value;

    class Type;

    class Instruction;

    class DominatorTree;
}

struct NCAddrType {
    llvm::Value *pAddr;
    llvm::Type *pType;

    NCAddrType() : pAddr(nullptr), pType(nullptr) {

    }

    bool operator<(const NCAddrType &o) const {
        if (pAddr < o.pAddr) {
            return true;
        } else if (pAddr == o.pAddr) {
            return pType < o.pType;
        } else {
            return false;
        }
    }
};

bool getNCAddrType(const llvm::Instruction *pInst, unsigned operandIdx, NCAddrType &addrType);

enum NCDomResult : unsigned {
    Uncertainty = 0,
    ExistsLoadDomAllStores,
    ExistsStoreDomAllLoads
};

NCDomResult existsDom(const llvm::DominatorTree &DT, const std::vector<llvm::Instruction *> &vecLoadInst,
                      const std::vector<llvm::Instruction *> &vecStoreInst);

enum NCInstrumentLocFlag : unsigned {
    Inplace = 0,
    Hoist,
    HoistSink,
};

typedef std::map<llvm::Instruction *, NCInstrumentLocFlag> MapLocFlagToInstrument;

#endif //NEWCOMAIR_COMMON_LOCATEINSTRUMENT_H
