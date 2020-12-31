#include "ParseRecord.h"

#include <string.h>

#include <unordered_set>
#include <set>
#include <vector>
#include <algorithm>
#include <limits.h>
#include <assert.h>
// #define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

constexpr unsigned INVALID_ID = 0;
constexpr unsigned MIN_ID = 1;
constexpr unsigned DELIMIT = INT_MAX;
constexpr unsigned LOOP_BEGIN = INT_MAX - 1;
constexpr unsigned LOOP_END = INT_MAX - 2;
constexpr unsigned LOOP_STRIDE = INT_MAX - 3;
constexpr unsigned LOOP_NEGATIVE_STRIDE = INT_MAX - 4;
constexpr unsigned MAX_ID = INT_MAX;

struct_stMemRecord record{0UL, 0U, INVALID_ID};

OneLoopRecordTy oneLoopRecord;  // <address, OneLoopRecordFlag>
std::set<unsigned long> oneLoopDistinctAddr;  // Ci: ith Distinct First Load Address
unsigned long oneLoopIOFuncSize = 0;  // when used, clear
unsigned long allIOFuncSize = 0;
std::vector<unsigned long> intersect;

std::set<unsigned long> allDistinctAddr;  // Mi: i-1 Distinct First Load Addresses

unsigned long sumOfMiCi = 0;
unsigned long sumOfRi = 0;

static void calcMiCi() {

    oneLoopDistinctAddr.clear();
    for (auto &kv : oneLoopRecord) {
        if (kv.second == OneLoopRecordFlag::FirstLoad) {
            oneLoopDistinctAddr.insert(kv.first);
        }
    }
    oneLoopRecord.clear();

    if (allDistinctAddr.empty()) {
        allDistinctAddr.insert(oneLoopDistinctAddr.begin(), oneLoopDistinctAddr.end());
        allIOFuncSize = oneLoopIOFuncSize;
    }

    sumOfMiCi += (allDistinctAddr.size() + allIOFuncSize) * (oneLoopDistinctAddr.size() + oneLoopIOFuncSize);

    DEBUG_PRINT(
            ("sumOfMiCi: %lu, Mi: %lu+%lu, Ci: %lu+%lu\n", sumOfMiCi, allDistinctAddr.size(), allIOFuncSize, oneLoopDistinctAddr.size(), oneLoopIOFuncSize));

    intersect.clear();
    std::set_intersection(allDistinctAddr.begin(), allDistinctAddr.end(), oneLoopDistinctAddr.begin(),
                          oneLoopDistinctAddr.end(), std::inserter(intersect, intersect.begin()));
    sumOfRi += intersect.size();

    DEBUG_PRINT(("sumOfRi: %lu, Ri: %lu\n", sumOfRi, intersect.size()));

    allDistinctAddr.insert(oneLoopDistinctAddr.begin(), oneLoopDistinctAddr.end());
    oneLoopDistinctAddr.clear();

    allIOFuncSize += oneLoopIOFuncSize;
    oneLoopIOFuncSize = 0;
}

static void calcUniqAddr() {
    oneLoopDistinctAddr.clear();
    for (auto &kv : oneLoopRecord) {
        if (kv.second == OneLoopRecordFlag::FirstLoad) {
            oneLoopDistinctAddr.insert(kv.first);
        }
    }
    oneLoopRecord.clear();

    allDistinctAddr.insert(oneLoopDistinctAddr.begin(), oneLoopDistinctAddr.end());
    oneLoopDistinctAddr.clear();

    allIOFuncSize += oneLoopIOFuncSize;
    oneLoopIOFuncSize = 0;
}

void parseRecordNoSample(char *pcBuffer) {
    if (!pcBuffer) {
       fprintf(stderr, "NULL buffer\n");
       return; 
    }

    struct_stMemRecord *records = (struct_stMemRecord *)pcBuffer;

    int stride = 0;
    unsigned long arrayBeginAddress = 0UL;
    unsigned arrayBeginLength = 0U;
    bool endFlag = false;
    // Start from 1 to skip the first Delimiter
    for (unsigned long i = 0; !endFlag; ++i) {
        struct_stMemRecord *record = &records[i];
        DEBUG_PRINT(("%lu, %u, %d\n", record->address, record->length, record->id));

        if (record->id == INVALID_ID) {
            endFlag = true;

            unsigned long cost = 0;
            if (record->address != 0UL && record->length == 0U) {
                cost = record->address;
            } else if (record->address == 0UL && record->length != 0U) {
                cost = record->length;
            } else {
                fprintf(stderr, "cost == 0\n");
            }

            calcUniqAddr();
            printf("%lu,%lu\n", allDistinctAddr.size(), cost);
        } else if (record->id == DELIMIT) {
            calcUniqAddr();
            DEBUG_PRINT(("allDistinctAddr: %lu\n", allDistinctAddr.size()));
        } else if (record->id == LOOP_STRIDE) {
            stride = (int)record->length;
            assert(stride > 0);
        } else if (record->id == LOOP_NEGATIVE_STRIDE) {
            stride = -(int)record->length;
            assert(stride < 0);
        } else if (record->id == LOOP_BEGIN) {
            assert(stride != 0 && "LOOP_BEGIN");
            arrayBeginAddress = record->address;
            arrayBeginLength = record->length;
        } else if (record->id == LOOP_END) {
            assert(stride != 0 && "LOOP_END");
            assert(arrayBeginAddress != 0UL);
            assert(arrayBeginLength == record->length);
            for (unsigned long j = arrayBeginAddress; j <= record->address; j += stride * arrayBeginLength) {
                for (unsigned long k = j; k < j + arrayBeginLength; ++k) {
                    oneLoopRecord.insert({k, OneLoopRecordFlag::FirstLoad});
                }
            }
        } else if (record->id > 0) {
            if (record->address == 0UL) {
                // IO Function update RMS
                oneLoopIOFuncSize += record->length;
            } else {
                for (unsigned long j = record->address; j < record->address+record->length; ++j) {
                    oneLoopRecord.insert({j, OneLoopRecordFlag::FirstLoad});
                }
            }
        } else {  // record.id < 0
            for (unsigned long j = record->address; j < record->address+record->length; ++j) {
                oneLoopRecord.insert({j, OneLoopRecordFlag::FirstStore});
            }
        }
    }
}

void parseRecord(char *pcBuffer) {
    if (pcBuffer == nullptr) {
        printf("NULL buffer\n");
        return;
    }

    struct_stMemRecord *records = (struct_stMemRecord *)pcBuffer;

    bool endFlag = false;
    // Start from 1 to skip the first Delimiter
    for (unsigned long i = 1; !endFlag; ++i) {
        struct_stMemRecord *record = &records[i];
        DEBUG_PRINT(("%lu, %u, %d\n", record->address, record->length, record->id));

        if (record->id == INVALID_ID) {
            endFlag = true;
            unsigned long cost = 0;
            if (record->address != 0UL && record->length == 0U) {
                cost = record->address;
            } else if (record->address == 0UL && record->length != 0U) {
                cost = record->length;
            } else {
                fprintf(stderr, "cost == 0\n");
            }

            calcMiCi();

            if (sumOfRi == 0) {
                printf("%u,%lu\n", 0, cost);
            } else {
                printf("%lu,%lu\n", sumOfMiCi / sumOfRi, cost);
            }
        } else if (record->id == DELIMIT) {
            calcMiCi();
        } else if (record->id > 0) {
            if (record->address == 0UL) {
                // IO Function update RMS
                oneLoopIOFuncSize += record->length;
            } else {
                for (unsigned long j = record->address; j < record->address+record->length; ++j) {
                    oneLoopRecord.insert({j, OneLoopRecordFlag::FirstLoad});
                }
            }
        } else {  // record.id < 0
            for (unsigned long j = record->address; j < record->address+record->length; ++j) {
                oneLoopRecord.insert({j, OneLoopRecordFlag::FirstStore});
            }
        }
    }
}

void parseRecordDebug(char *pcBuffer) {
    if (!pcBuffer) {
       fprintf(stderr, "NULL buffer\n");
       return; 
    }
    struct_stMemRecord *records = (struct_stMemRecord *)pcBuffer;
    for (unsigned long i = 0; ; ++i) {
        struct_stMemRecord *curr = &records[i];
        fprintf(stderr, "%lu, %u, %d\n", curr->address, curr->length, curr->id);
        if (curr->id == INVALID_ID) {
            break;
        }
    }
}