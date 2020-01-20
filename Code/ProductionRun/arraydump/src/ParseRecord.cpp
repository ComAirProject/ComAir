#include "ParseRecord.h"

#include <string.h>

#include <unordered_set>
#include <set>
#include <stack>
#include <cmath>
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

void parseRecord(char *pcBuffer, unsigned stride) {

    if (pcBuffer == nullptr) {
        printf("NULL buffer\n");
        return;
    }

    bool endFlag = false;

    std::stack<unsigned long> loopBeginExit;

    for (unsigned long i = 16UL; !endFlag; i += 16UL) {
        memcpy(&record, &pcBuffer[i], sizeof(record));
        //fprintf(stderr, "%ld, %d, %d\n", record.address, record.length, record.id);

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
            if (!loopBeginExit.empty()) {
                fprintf(stderr, "Loop Begin Exit Does Not Match\n");
                break;
            }
            calcMiCi();
        } else if (record.id == LOOP_BEGIN) {
            // push record.address
            loopBeginExit.push(record.address);
        } else if (record.id == LOOP_END) {
            // check stack
            // minus, get rms, directly add to rms
            unsigned long loopEndAddr = record.address;
            unsigned long loopBeginAddr = loopBeginExit.top();
            loopBeginExit.pop();
            unsigned long addrDiff = 0;

            unsigned long loopSmallAddr = loopBeginAddr;
            unsigned long loopBigAddr = loopEndAddr;
            if (loopBeginAddr > loopEndAddr) {
                loopSmallAddr = loopEndAddr;
                loopBigAddr = loopBeginAddr;
            }

            unsigned numByteOfType = record.length / 8;
            for (unsigned long j = loopSmallAddr; j <= loopBigAddr; j += stride) {
                for (unsigned long k = j; k < j + numByteOfType; ++k) {
                    if (oneLoopRecord[k] == OneLoopRecordFlag::Uninitialized) {
                        oneLoopRecord[k] = OneLoopRecordFlag::FirstLoad;
                    }
                }
            }

        } else if (record.id > 0) {
            if (record.address == 0UL) {
                // IO Function update RMS
                oneLoopIOFuncSize += record.length;
            } else {
                unsigned numByteOfType = record.length / 8;
                for (unsigned j = 0; j < numByteOfType; ++j) {
                    if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
                        oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstLoad;
                    }
                }
            }
        } else {  // record.id < 0
            unsigned numByteOfType = record.length / 8;
            for (unsigned j = 0; j < numByteOfType; ++j) {
                if (oneLoopRecord[record.address + j] == OneLoopRecordFlag::Uninitialized) {
                    oneLoopRecord[record.address + j] = OneLoopRecordFlag::FirstStore;
                }
            }
        }
    }
}