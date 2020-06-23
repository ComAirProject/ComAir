#include "Common/MonitorRWInsts.h"

#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Operator.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "Common/Constant.h"

using namespace llvm;

Function *getCalleeFunc(Instruction *pInst)
{

    if (isa<DbgInfoIntrinsic>(pInst))
    {
        return nullptr;
    }
    else if (isa<InvokeInst>(pInst) || isa<CallInst>(pInst))
    {
        CallSite cs(pInst);
        Function *pCalled = cs.getCalledFunction();

        if (!pCalled)
        {
            return nullptr;
        }

        return pCalled;
    }

    return nullptr;
}

Function *getCalleeFuncWithBitcast(Instruction *pInst)
{

    if (isa<DbgInfoIntrinsic>(pInst))
    {
        return nullptr;
    }
    else if (isa<InvokeInst>(pInst) || isa<CallInst>(pInst))
    {
        CallSite cs(pInst);
        Function *pCalled = cs.getCalledFunction();

        if (!pCalled)
        {
            Value *pCalledValue = cs.getCalledValue();
            if (BitCastOperator *BCO = dyn_cast<BitCastOperator>(pCalledValue))
            {
                Value *Src = BCO->getOperand(0);
                if (Function *F = dyn_cast<Function>(Src))
                {
                    return F;
                }
            }
            return nullptr;
        }

        return pCalled;
    }

    return nullptr;
}

bool isMonitoredLoad(LoadInst *pLoad)
{

    Value *Op = pLoad->getOperand(0);
    Type *Ty = Op->getType();
    Type *Ty1 = Ty->getContainedType(0);
    if (!Ty1)
    {
        errs() << "Load Type NULL\n";
        pLoad->dump();
        return false;
    }
    if (!Ty1->isSized())
    {
        errs() << "Load Type not sized\n";
        pLoad->dump();
        return false;
    }
    while (Ty1 && isa<PointerType>(Ty1))
    {
        Ty1 = Ty1->getContainedType(0);
    }
    if (isa<FunctionType>(Ty1))
    {
        return false;
    }

    return true;
}

bool isMonitoredStore(StoreInst *pStore)
{

    Value *Op = pStore->getOperand(1);
    Type *Ty = Op->getType();
    Type *Ty1 = Ty->getContainedType(0);
    if (!Ty1)
    {
        errs() << "Store Type NULL\n";
        pStore->dump();
        return false;
    }
    if (!Ty1->isSized())
    {
        errs() << "Store Type not sized\n";
        pStore->dump();
        return false;
    }
    while (Ty1 && isa<PointerType>(Ty1))
    {
        Ty1 = Ty1->getContainedType(0);
    }
    if (isa<FunctionType>(Ty1))
    {
        return false;
    }

    return true;
}

bool isUnreachableInst(Instruction *pInst)
{

    if (BranchInst *pBranch = dyn_cast<BranchInst>(pInst))
    {
        if (pBranch->getNumOperands() == 1 && pBranch->getOperand(0)->getName() == "UnifiedUnreachableBlock")
        {
            return true;
        }
    }

    if (isa<UnreachableInst>(pInst))
    {
        return true;
    }

    return false;
}

bool isExitInst(Instruction *pInst)
{

    if (Function *pFunc = getCalleeFuncWithBitcast(pInst))
    {
        if (pFunc->getName() == "exit" ||
            pFunc->getName() == "_ZL9mysql_endi" ||
            pFunc->getName() == "_Z10end_threadP3THDb")
        {
            return true;
        }
    }
    return false;
}

int mapFromOriginToCloned(ValueToValueMapTy &originClonedMapping, MonitoredRWInsts &origin, MonitoredRWInsts &cloned)
{

    int countNoMap = 0;
    cloned.clear();

    for (auto &kv : origin.mapLoadID)
    {
        unsigned id = kv.second;
        auto it = originClonedMapping.find(kv.first);
        if (it != originClonedMapping.end())
        {
            auto pClonedInst = cast<LoadInst>(it->second);
            cloned.mapLoadID[pClonedInst] = id;
        }
        else
        {
            errs() << "No cloned mapping found:\n";
            kv.first->dump();
            ++countNoMap;
        }
    }

    for (auto &kv : origin.mapStoreID)
    {
        unsigned id = kv.second;
        auto it = originClonedMapping.find(kv.first);
        if (it != originClonedMapping.end())
        {
            auto pClonedInst = cast<StoreInst>(it->second);
            cloned.mapStoreID[pClonedInst] = id;
        }
        else
        {
            errs() << "No cloned mapping found:\n";
            kv.first->dump();
            ++countNoMap;
        }
    }

    for (auto &kv : origin.mapMemSetID)
    {
        unsigned id = kv.second;
        auto it = originClonedMapping.find(kv.first);
        if (it != originClonedMapping.end())
        {
            auto pClonedInst = cast<MemSetInst>(it->second);
            cloned.mapMemSetID[pClonedInst] = id;
        }
        else
        {
            errs() << "No cloned mapping found:\n";
            kv.first->dump();
            ++countNoMap;
        }
    }

    for (auto &kv : origin.mapMemTransferID)
    {
        unsigned id = kv.second;
        auto it = originClonedMapping.find(kv.first);
        if (it != originClonedMapping.end())
        {
            auto pClonedInst = cast<MemTransferInst>(it->second);
            cloned.mapMemTransferID[pClonedInst] = id;
        }
        else
        {
            errs() << "No cloned mapping found:\n";
            kv.first->dump();
            ++countNoMap;
        }
    }

    for (auto &kv : origin.mapOstreamID)
    {
        unsigned id = kv.second;
        auto it = originClonedMapping.find(kv.first);
        if (it != originClonedMapping.end())
        {
            auto pClonedInst = cast<Instruction>(it->second);
            cloned.mapOstreamID[pClonedInst] = id;
        }
        else
        {
            errs() << "No cloned mapping found:\n";
            kv.first->dump();
            ++countNoMap;
        }
    }

    return countNoMap;
}

unsigned MonitoredRWInsts::size()
{
    return mapLoadID.size() + mapStoreID.size() + mapMemTransferID.size() + mapMemSetID.size() + mapFreadID.size() +
           mapFgetcID.size() + mapOstreamID.size();
}

bool MonitoredRWInsts::add(Instruction *pInst)
{

    unsigned instID = GetInstructionID(pInst);
    if (instID == INVALID_ID)
    {
        return false;
    }
    if (LoadInst *pLoad = dyn_cast<LoadInst>(pInst))
    {
        if (isMonitoredLoad(pLoad))
        {
            mapLoadID[pLoad] = instID;
            return true;
        }
    }
    else if (StoreInst *pStore = dyn_cast<StoreInst>(pInst))
    {
        if (isMonitoredStore(pStore))
        {
            mapStoreID[pStore] = instID;
            return true;
        }
    }
    else if (MemSetInst *pMemSet = dyn_cast<MemSetInst>(pInst))
    {
        mapMemSetID[pMemSet] = instID;
        return true;
    }
    else if (MemTransferInst *pMemTransfer = dyn_cast<MemTransferInst>(pInst))
    {
        mapMemTransferID[pMemTransfer] = instID;
        return true;
    }
    else if (Function *pFunc = getCalleeFunc(pInst))
    {
        if (pFunc->isDeclaration())
        {
            std::string funcName = pFunc->getName().str();
            if (funcName == "fgetc")
            {
                mapFgetcID[pInst] = instID;
                return true;
            }
            else if (funcName == "fread")
            {
                mapFreadID[pInst] = instID;
                return true;
            }
            else if (funcName ==
                     "_ZStlsIcSt11char_traitsIcESaIcEERSt13basic_ostreamIT_T0_ES7_RKNSt7__cxx1112basic_stringIS4_S5_T1_EE")
            {
                mapOstreamID[pInst] = instID;
                return true;
            }
        }
    }
    return false;
}

void MonitoredRWInsts::clear()
{

    mapLoadID.clear();
    mapStoreID.clear();
    mapMemSetID.clear();
    mapMemTransferID.clear();
    mapFgetcID.clear();
    mapFreadID.clear();
    mapOstreamID.clear();
}

void MonitoredRWInsts::print(llvm::raw_ostream &os)
{

    for (auto &kv : mapLoadID)
    {
        os << "Load," << kv.second << "\n";
    }
    for (auto &kv : mapStoreID)
    {
        os << "Store," << kv.second << "\n";
    }
    for (auto &kv : mapMemSetID)
    {
        os << "MemSet," << kv.second << "\n";
    }
    for (auto &kv : mapMemTransferID)
    {
        os << "MemTransfer," << kv.second << "\n";
    }
    for (auto &kv : mapFgetcID)
    {
        os << "Fgetc," << kv.second << "\n";
    }
    for (auto &kv : mapFreadID)
    {
        os << "Fread," << kv.second << "\n";
    }
    for (auto &kv : mapOstreamID)
    {
        os << "Ostream," << kv.second << "\n";
    }
}

void MonitoredRWInsts::dump()
{

    print(errs());
}

void MonitoredRWInsts::diff(MonitoredRWInsts &rhs)
{

    for (auto &kv : rhs.mapLoadID)
    {
        mapLoadID.erase(kv.first);
    }
    for (auto &kv : rhs.mapStoreID)
    {
        mapStoreID.erase(kv.first);
    }
    for (auto &kv : rhs.mapMemSetID)
    {
        mapMemSetID.erase(kv.first);
    }
    for (auto &kv : rhs.mapMemTransferID)
    {
        mapMemTransferID.erase(kv.first);
    }
    for (auto &kv : rhs.mapFgetcID)
    {
        mapFgetcID.erase(kv.first);
    }
    for (auto &kv : rhs.mapFreadID)
    {
        mapFreadID.erase(kv.first);
    }
    for (auto &kv : rhs.mapOstreamID)
    {
        mapOstreamID.erase(kv.first);
    }
}

bool MonitoredRWInsts::empty()
{
    return mapLoadID.empty() && mapStoreID.empty() && mapMemSetID.empty() && mapMemTransferID.empty() &&
           mapFgetcID.empty() && mapFreadID.empty() && mapOstreamID.empty();
}
