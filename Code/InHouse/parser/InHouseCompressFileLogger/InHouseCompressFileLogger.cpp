#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <map>

struct stack_elem {
    unsigned funcId; // function id
    unsigned ts; // time stamp
    long rms;
    unsigned long cost;
};

/*---- share memory ---- */
#define BUFFERSIZE 1UL << 38
#define APROF_MEM_LOG "/media/boqin/New Volume/aprof_log.log"


// logger

typedef void (*log_LockFn)(void *udata, int lock);

static struct {
    void *udata;
    log_LockFn lock;
    FILE *fp;
    int level;
    int quiet;
} L;

enum {
    LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL
};

#define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

static void lock(void) {
    if (L.lock) {
        L.lock(L.udata, 1);
    }
}


static void unlock(void) {
    if (L.lock) {
        L.lock(L.udata, 0);
    }
}


void log_init(FILE *fp, int level, int enable) {
    L.fp = fp;
    L.level = level;
    L.quiet = enable ? 1 : 0;
}


void log_log(int level, const char *file, int line, const char *fmt, ...) {
    if (level < L.level) {
        return;
    }

    /* Acquire lock */
    lock();

    /* Log to file */
    if (L.fp) {
        va_list args;
        va_start(args, fmt);
        vfprintf(L.fp, fmt, args);
        va_end(args);
        fprintf(L.fp, "\n");
    }

    /* Release lock */
    unlock();
}

void read_shared_momery() {

    int fd = open(APROF_MEM_LOG, O_RDWR | O_CREAT, 0777);

    if (fd < 0) {
        fd = open(APROF_MEM_LOG, O_RDWR, 0777);
    }

    assert(fd >= 0);

    assert(ftruncate(fd, BUFFERSIZE) == 0);

    char *ptr = (char *)mmap(NULL, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);

    struct stack_elem Ele;
    int size_of_ele = sizeof(Ele);

    std::map<unsigned, std::map<long, unsigned long>> results;
    puts("start reading data....");
    memcpy(&Ele, ptr, sizeof(Ele));
    log_fatal("func_id,rms,cost");
    while (Ele.funcId > 0) {
        auto itFuncId = results.find(Ele.funcId);
        if (itFuncId != results.end()) {
            auto &mapRmsCost = itFuncId->second;
            auto itRms = mapRmsCost.find(Ele.rms);
            if (itRms != mapRmsCost.end()) {
                if (itRms->second < Ele.cost) {
                    itRms->second = Ele.cost;
                }
            } else {
                mapRmsCost[Ele.rms] = Ele.cost;
            }
        } else {
            results[Ele.funcId][Ele.rms] = Ele.cost;
        }
        ptr += size_of_ele;
        memcpy(&Ele, ptr, size_of_ele);
    }

    for (auto &funcId_RmsCost : results) {
        auto funcId = funcId_RmsCost.first;
        for (auto &rms_Cost : funcId_RmsCost.second) {
            auto rms = rms_Cost.first;
            auto cost = rms_Cost.second;
            log_fatal("%u,%ld,%lu",
                      funcId,
                      rms,
                      cost
            );
        }
    }

    puts("read over");
    close(fd);
}

int main() {

    char FILENAME[] = "aprof_logger_XXXXXX";
    int fd;
    fd = mkstemp(FILENAME);
    assert(fd > 0);

    int QUIET = 1;
    FILE *fp = fopen(FILENAME, "w");
    log_init(fp, 4, QUIET);

    read_shared_momery();
    return 0;
}