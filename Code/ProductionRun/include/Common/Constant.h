#ifndef PRODUCTIONRUN_CONSTANT_H
#define PRODUCTIONRUN_CONSTANT_H

#include <limits.h>

#define APROF_INSERT_FLAG "aprof_insert_flag"

enum {
    INIT = 0,
    WRITE = 1,
    READ = 2,
    CALL_BEFORE = 3,
    RETURN = 4,
    INCREMENT_RMS = 5,
    COST_UPDATE = 6,
};

#define BB_COST_FLAG "bb_cost_flag"
#define IGNORE_OPTIMIZED_FLAG "ignore_optimized_flag"

#define CLONE_FUNCTION_PREFIX "$aprof$"

#define ARRAY_LIST_INSERT "aprof_array_list_hook"

constexpr unsigned INVALID_ID = 0;
constexpr unsigned MIN_ID = 1;
constexpr unsigned DELIMIT = INT_MAX;
constexpr unsigned LOOP_BEGIN = INT_MAX - 1;
constexpr unsigned LOOP_END = INT_MAX - 2;
constexpr unsigned LOOP_STRIDE = INT_MAX - 3;
constexpr unsigned LOOP_NEGATIVE_STRIDE = INT_MAX - 4;
constexpr unsigned MAX_ID = INT_MAX;

#endif //PRODUCTIONRUN_CONSTANT_H
