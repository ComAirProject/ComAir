#include "ParseRecord.h"

#include <string.h>

#include <unordered_set>
#include <set>
#include <vector>
#include <algorithm>
#include <limits.h>
#include <assert.h>

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
constexpr unsigned MAX_ID = INT_MAX - 3;

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

void parseRecord(char *pcBuffer) {

    if (pcBuffer == nullptr) {
        printf("NULL buffer\n");
        return;
    }

    bool endFlag = false;

    for (unsigned long i = 16UL; !endFlag; i += 16UL) {
        memcpy(&record, &pcBuffer[i], sizeof(record));

        if (record.id == INVALID_ID) {
            endFlag = true;
            unsigned long cost = 0;
            if (record.address != 0UL && record.length == 0U) {
                cost = record.address;
            } else if (record.address == 0UL && record.length != 0U) {
                cost = record.length;
            } else {
                fprintf(stderr, "cost == 0\n");
            }

            calcMiCi();

            if (sumOfRi == 0) {
                printf("%u,%lu\n", 0, cost);
            } else {
                printf("%lu,%lu\n", sumOfMiCi / sumOfRi, cost);
            }
        } else if (record.id == DELIMIT) {
            calcMiCi();
        } else if (record.id > 0) {
            if (record.address == 0UL) {
                // IO Function update RMS
                oneLoopIOFuncSize += record.length;
            } else {
                for (unsigned j = 0; j < record.length; j += 8) {
                    if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
                        oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstLoad;
                    }
                }
            }
        } else {  // record.id < 0
            for (unsigned j = 0; j < record.length; j += 8) {
                if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
                    oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstStore;
                }
            }
        }

//        switch (flag) {
//            case RecordFlag::Terminator: {
//                endFlag = true;
//                    if (record.address != 0UL && record.length == 0U) {
//                        auto cost = record.address;
//                        DEBUG_PRINT(("cost: %u\n", cost));
//                        calcMiCi();
//                        if (sumOfRi == 0) {
//                            DEBUG_PRINT(("sumOfMiCi=%lu, sumOfRi=%lu, cost=%lu\n", sumOfMiCi, sumOfRi, cost));
//                            printf("%u,%lu\n", 0, cost);
//                        } else {
//                            DEBUG_PRINT(
//                                    ("sumOfMiCi=%lu, sumOfRi=%lu, N=%lu, cost=%lu\n", sumOfMiCi, sumOfRi, sumOfMiCi /
//                                                                                                          sumOfRi, cost));
//                            printf("%lu,%lu\n", sumOfMiCi / sumOfRi, cost);
//                        }
//                    } else {
//                        fprintf(stderr, "cost=%lu, record=%u\n", record.address, record.length);
//                        fprintf(stderr, "Wrong cost format\n");
//                    }
//                break;
//            }
//            case RecordFlag::Delimiter: {
//
//                    calcMiCi();
//
//                break;
//            }
//            case RecordFlag::LoadFlag: {
//
//                if (record.address == 0UL) {
//                    // IO Function update RMS
//                    oneLoopIOFuncSize += record.length;
//                } else {
//                    for (unsigned j = 0; j < record.length; j += 8) {
//                        if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
//                            oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstLoad;
//                        }
//                    }
//                }
//                break;
//            }
//            case RecordFlag::StoreFlag: {
//                for (unsigned j = 0; j < record.length; j += 8) {
//                    if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
//                        oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstStore;
//                    }
//                }
//                break;
//            }
//            default: {
//                break;
//            }
//        }
    }
}