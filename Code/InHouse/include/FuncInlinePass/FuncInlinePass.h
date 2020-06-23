/**
 * Make function inline.
 * When lib-inline 1, inline func `aprof_query_insert_page_table` in runtime lib;
 * otherwise, inline instrumented hooks, e.g. `aprof_read`, `aprof_write`, etc.
 */

#ifndef INHOUSE_FUNCINLINEPASS_H
#define INHOUSE_FUNCINLINEPASS_H

#include "llvm/Pass.h"

namespace func_inline_pass {

    struct FuncInlinePass : public llvm::ModulePass {

        static char ID;

        FuncInlinePass();

        void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

        bool runOnModule(llvm::Module &M) override;
    };
}

#endif //INHOUSE_FUNCINLINEPASS_H
