#ifndef PRODUCTIONRUN_CONSTANTS_H
#define PRODUCTIONRUN_CONSTANTS_H

#include <limits.h>

namespace common
{
    constexpr unsigned INVALID_ID = 0;
    constexpr unsigned MIN_ID = 1;
    constexpr unsigned DELIMIT = INT_MAX;
    constexpr unsigned LOOP_BEGIN = INT_MAX - 1;
    constexpr unsigned LOOP_END = INT_MAX - 2;
    constexpr unsigned LOOP_STRIDE = INT_MAX - 3;
    constexpr unsigned LOOP_NEGATIVE_STRIDE = INT_MAX - 4;
    constexpr unsigned MAX_ID = INT_MAX;
} // namespace common

#endif //PRODUCTIONRUN_CONSTANTS_H