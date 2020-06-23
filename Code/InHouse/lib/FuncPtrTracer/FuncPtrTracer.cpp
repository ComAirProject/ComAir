#include "FuncPtrTracer/FuncPtrTracer.h"

#include <set>

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include "Common/Constant.h"
#include "Common/Helper.h"
#include "Common/MonitorRWInsts.h"

using namespace llvm;

namespace func_ptr_tracer {

    static cl::opt <std::string> strEntryFunc("strEntryFunc",
                                             cl::desc("Entry Function Name"), cl::Optional,
                                             cl::value_desc("strEntryFunc"));

    char FuncPtrTracer::ID = 0;

    FuncPtrTracer::FuncPtrTracer() : ModulePass(ID) {}

    void FuncPtrTracer::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
        AU.setPreservesAll();
    }

    bool FuncPtrTracer::runOnModule(llvm::Module &M) {

        this->pModule = &M;

        SetupInit();

        appendToGlobalCtors(M, this->func_ptr_hook_init, 0, nullptr);

        for (Function &F : M) {
            if (IsIgnoreFunc(&F)) {
                continue;
            }
            errs().write_escaped(F.getName()) << '\n';
            unsigned funcID = GetFunctionID(&F);

            std::set<Instruction *> setReturnLoc;
            Instruction *prev = nullptr;
            for (BasicBlock &B : F) {
                for (Instruction &I : B) {
                    if (IsIgnoreInst(&I)) {
                        continue;
                    }
                    if (isa<ReturnInst>(&I)) {
                        setReturnLoc.insert(&I);
                    } else if (common::isUnreachableInst(&I)) {
                        setReturnLoc.insert(prev);
                    } else if (isa<CallInst>(&I) || isa<InvokeInst>(&I)) {
                        unsigned instID = GetInstructionID(&I);
                        CallSite cs(&I);
                        Function *callee = cs.getCalledFunction();
                        if (callee && IsIgnoreFunc(callee)) {
                            prev = &I;
                            continue;
                        }
                        InstrumentCallInst(instID, &I);
                    }

                    prev = &I;
                }
            }
            for (Instruction *BeforeExit : setReturnLoc) {
                InstrumentExitFunc(funcID, BeforeExit);
            }

            Instruction *InsertBefore = F.getEntryBlock().getFirstNonPHIOrDbgOrLifetime();
            InstrumentEnterFunc(funcID, InsertBefore);
        }

////        Function *pEntryFunc = M.getFunction("main");
//        Function *pEntryFunc = M.getFunction(strEntryFunc);
//        assert(pEntryFunc);
////        Instruction *pInitInst = pEntryFunc->getEntryBlock().getFirstNonPHIOrDbg();
////        InstrumentInit(pInitInst);
//        for (BasicBlock &B : *pEntryFunc) {
//            for (Instruction &I : B) {
//                if (isa<ReturnInst>(&I)) {
//                    InstrumentFinal(&I);
//                } else if (isExitInst(&I)) {
//                    InstrumentFinal(&I);
//                }
//            }
//        }

        return false;
    }

    void FuncPtrTracer::SetupInit() {
        SetupTypes();
        SetupFunctions();
    }

    void FuncPtrTracer::SetupTypes() {
        this->VoidType = Type::getVoidTy(pModule->getContext());
        this->IntType = IntegerType::get(pModule->getContext(), 32);
        this->LongType = IntegerType::get(pModule->getContext(), 64);
    }

    void FuncPtrTracer::SetupFunctions() {

        std::vector<Type *> ArgTypes;
        // void func_ptr_hook_init()
        this->func_ptr_hook_init = this->pModule->getFunction("func_ptr_hook_init");
        if (!this->func_ptr_hook_init) {
            FunctionType *func_ptr_hook_init_ty = FunctionType::get(this->VoidType, ArgTypes, false);
            this->func_ptr_hook_init = Function::Create(func_ptr_hook_init_ty, GlobalValue::ExternalLinkage, "func_ptr_hook_init", this->pModule);
            this->func_ptr_hook_init->setCallingConv(CallingConv::C);
            ArgTypes.clear();
        }
        // void func_ptr_hook_enter_func(int threadId, int funcId)
        this->func_ptr_hook_enter_func = this->pModule->getFunction("func_ptr_hook_enter_func");
        if (!this->func_ptr_hook_enter_func) {
            ArgTypes.push_back(this->IntType);
            FunctionType *func_ptr_hook_enter_func_ty = FunctionType::get(this->VoidType, ArgTypes, false);
            this->func_ptr_hook_enter_func = Function::Create(func_ptr_hook_enter_func_ty, GlobalValue::ExternalLinkage, "func_ptr_hook_enter_func", this->pModule);
            this->func_ptr_hook_enter_func->setCallingConv(CallingConv::C);
            ArgTypes.clear();
        }
        //void func_ptr_hook_exit_func(int threadId, int funcId);
        this->func_ptr_hook_exit_func = this->pModule->getFunction("func_ptr_hook_exit_func");
        if (!this->func_ptr_hook_exit_func) {
            ArgTypes.push_back(this->IntType);
            FunctionType *func_ptr_hook_exit_func_ty = FunctionType::get(this->VoidType, ArgTypes, false);
            this->func_ptr_hook_exit_func = Function::Create(func_ptr_hook_exit_func_ty, GlobalValue::ExternalLinkage, "func_ptr_hook_exit_func", this->pModule);
            this->func_ptr_hook_exit_func->setCallingConv(CallingConv::C);
            ArgTypes.clear();
        }
        //void func_ptr_hook_call_inst(int threadId, int instId);
        this->func_ptr_hook_call_inst = this->pModule->getFunction("func_ptr_hook_call_inst");
        if (!this->func_ptr_hook_call_inst) {
            ArgTypes.push_back(this->IntType);
            FunctionType *func_ptr_hook_call_inst_ty = FunctionType::get(this->VoidType, ArgTypes, false);
            this->func_ptr_hook_call_inst = Function::Create(func_ptr_hook_call_inst_ty, GlobalValue::ExternalLinkage, "func_ptr_hook_call_inst", this->pModule);
            this->func_ptr_hook_call_inst->setCallingConv(CallingConv::C);
            ArgTypes.clear();
        }
        //void func_ptr_hook_final()
        this->func_ptr_hook_final = this->pModule->getFunction("func_ptr_hook_final");
        if (!this->func_ptr_hook_final) {
            FunctionType *func_ptr_hook_final_ty = FunctionType::get(this->VoidType, ArgTypes, false);
            this->func_ptr_hook_final = Function::Create(func_ptr_hook_final_ty, GlobalValue::ExternalLinkage, "func_ptr_hook_final", this->pModule);
            this->func_ptr_hook_final->setCallingConv(CallingConv::C);
            ArgTypes.clear();
        }
    }

    void FuncPtrTracer::InstrumentInit(Instruction *InsertBefore) {

        CallInst *init = CallInst::Create(this->func_ptr_hook_init, "", InsertBefore);
        init->setCallingConv(CallingConv::C);
        init->setTailCall(false);
        AttributeList emtpyList;
        init->setAttributes(emtpyList);
    }

    void FuncPtrTracer::InstrumentEnterFunc(unsigned funcID, Instruction *InsertBefore) {

        ConstantInt *const_func_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(funcID)), 10));
        std::vector<Value *> params;
        params.push_back(const_func_id);
        CallInst *enter = CallInst::Create(this->func_ptr_hook_enter_func, params, "", InsertBefore);
        enter->setCallingConv(CallingConv::C);
        enter->setTailCall(false);
        AttributeList emptyList;
        enter->setAttributes(emptyList);
    }

    void FuncPtrTracer::InstrumentExitFunc(unsigned funcID, Instruction *InsertBefore) {

        ConstantInt *const_func_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(funcID)), 10));
        std::vector<Value *> params;
        params.push_back(const_func_id);
        CallInst *exit= CallInst::Create(this->func_ptr_hook_exit_func, params, "", InsertBefore);
        exit->setCallingConv(CallingConv::C);
        exit->setTailCall(false);
        AttributeList emptyList;
        exit->setAttributes(emptyList);
    }

    void FuncPtrTracer::InstrumentCallInst(unsigned instID, llvm::Instruction *InsertBefore) {

        ConstantInt *const_inst_id = ConstantInt::get(this->pModule->getContext(), APInt(32, StringRef(std::to_string(instID)), 10));
        std::vector<Value *> params;
        params.push_back(const_inst_id);
        CallInst *exit= CallInst::Create(this->func_ptr_hook_call_inst, params,"", InsertBefore);
        exit->setCallingConv(CallingConv::C);
        exit->setTailCall(false);
        AttributeList emptyList;
        exit->setAttributes(emptyList);
    }

    void FuncPtrTracer::InstrumentFinal(Instruction *InsertBefore) {

        CallInst *final = CallInst::Create(this->func_ptr_hook_final, "", InsertBefore);
        final->setCallingConv(CallingConv::C);
        final->setTailCall(false);
        AttributeList emtpyList;
        final->setAttributes(emtpyList);
    }
}

static RegisterPass<func_ptr_tracer::FuncPtrTracer> X(
        "trace",
        "Trace CallInst/InvokeInst ID and Function ID",
        false,
        true);

