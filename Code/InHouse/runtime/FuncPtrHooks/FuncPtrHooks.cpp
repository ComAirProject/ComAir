#include "FuncPtrHooks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#define gettid() syscall(__NR_gettid)

#define BUFFERSIZE (1UL << 33)
#define FUNC_PTR_TRACER_LOG "func_ptr_tracer.log"

struct stTrace {
    long threadId;
    unsigned flag;
    unsigned funcOrInstId;
};

int fd = -1;
struct stTrace *pcBuffer = NULL;
unsigned long iBufferIndex = 0;
atomic_bool lock = false;
bool expected = false;

void func_ptr_hook_init() {

    fd = shm_open(FUNC_PTR_TRACER_LOG, O_RDWR | O_CREAT, 0777);

    if (fd == -1) {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        exit(-1);
    }
    if (ftruncate(fd, BUFFERSIZE) == -1) {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        exit(-1);
    }

    pcBuffer = (struct stTrace *) mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pcBuffer == NULL) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        exit(-1);
    }
}

void func_ptr_hook_enter_func(unsigned funcId) {

    while (!atomic_compare_exchange_weak(&lock, &expected, true)) {
        expected = false;
    }
    stTrace *pTrace = &pcBuffer[iBufferIndex];
    pTrace->threadId = gettid();
    pTrace->flag = 0;
    pTrace->funcOrInstId = funcId;
    ++iBufferIndex;
    lock = false;
}

void func_ptr_hook_exit_func(unsigned funcId) {

    while (!atomic_compare_exchange_weak(&lock, &expected, true)) {
        expected = false;
    }
    stTrace *pTrace = &pcBuffer[iBufferIndex];
    pTrace->threadId = gettid();
    pTrace->flag = 1;
    pTrace->funcOrInstId = funcId;
    ++iBufferIndex;
    lock = false;
}

void func_ptr_hook_call_inst(unsigned instId) {

    while (!atomic_compare_exchange_weak(&lock, &expected, true)) {
        expected = false;
    }
    stTrace *pTrace = &pcBuffer[iBufferIndex];
    pTrace->threadId = gettid();
    pTrace->flag = 2;
    pTrace->funcOrInstId = instId;
    ++iBufferIndex;
    lock = false;
}

void func_ptr_hook_final() {

    while (!atomic_compare_exchange_weak(&lock, &expected, true)) {
        expected = false;
    }
    stTrace *pTrace = &pcBuffer[iBufferIndex];
    pTrace->threadId = 0;
    pTrace->flag = 0;
    pTrace->funcOrInstId = 0;
    ++iBufferIndex;

    if (ftruncate(fd, sizeof(stTrace) * iBufferIndex) == -1) {
        fprintf(stderr, "ftruncate: %s\n", strerror(errno));
        exit(-1);
    }

    lock = false;

    close(fd);
}


