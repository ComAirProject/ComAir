#include "Common/LocateInstrument.h"

#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Dominators.h>

bool getNCAddrType(const llvm::Instruction *pInst, unsigned oprandIdx, NCAddrType &addrType) {

    llvm::Value *value = pInst->getOperand(oprandIdx);
    llvm::Type *type0 = value->getType();
    llvm::Type *type1 = type0->getContainedType(0);
    llvm::Type *type2 = type1;

    while (llvm::isa<llvm::PointerType>(type2)) {
        type2 = type2->getContainedType(0);
    }

    if (!llvm::isa<llvm::FunctionType>(type2)) {
        if (type1->isSized()) {
            addrType.pAddr = value;
            addrType.pType = type1;
            return true;

        } else {
            llvm::errs() << "type1 isSized is false\n";
            pInst->dump();
            type1->dump();
            return false;
        }
    }

    llvm::errs() << "type2 is a FunctionType\n";
    pInst->dump();
    type1->dump();
    return false;
}

NCDomResult existsDom(const llvm::DominatorTree &DT, const std::vector<llvm::Instruction *> &vecLoadInst,
                      const std::vector<llvm::Instruction *> &vecStoreInst) {

    for (auto &storeInst : vecStoreInst) {
        bool storeDomAllLoads = true;
        for (auto &loadInst : vecLoadInst) {
            if (!DT.dominates(storeInst, loadInst)) {
                storeDomAllLoads = false;
                break;
            }
        }
        if (storeDomAllLoads) {
            return ExistsStoreDomAllLoads;
        }
    }

    for (auto &loadInst : vecLoadInst) {
        bool loadDomAllStores = true;
        for (auto &storeInst : vecStoreInst) {
            if (!DT.dominates(loadInst, storeInst)) {
                loadDomAllStores = false;
                break;
            }
        }
        if (loadDomAllStores) {
            return ExistsLoadDomAllStores;
        }
    }

    return Uncertainty;
}