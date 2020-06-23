#include "FuncPtrParser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct stTrace {
    long threadId;
    unsigned flag;
    unsigned funcOrInstId;
};

#define BUFFERSIZE (1UL << 33)
#define FUNC_PTR_TRACER_LOG "func_ptr_tracer.log"

int main() {

    int fd = shm_open(FUNC_PTR_TRACER_LOG, O_RDWR, 0777);
    if (fd == -1) {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        exit(-1);
    }
    struct stTrace *pcBuffer = (struct stTrace *)mmap(NULL, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);
    for (long iBufferIndex = 0; iBufferIndex < BUFFERSIZE/sizeof(stTrace); ++iBufferIndex) {
        struct stTrace *pTrace = &pcBuffer[iBufferIndex];
        if (pTrace->threadId == 0) {
            break;
        }
        printf("%ld, %u, %u\n", pTrace->threadId, pTrace->flag, pTrace->funcOrInstId);
    }

    return 0;
}