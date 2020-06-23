#include "SharedMemReader.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <fstream>

#include "ParseRecord.h"

int openSharedMem(const char *sharedMemName, int &fd, char *&pcBuffer) {

    fd = shm_open(sharedMemName, O_RDWR, 07777);
    //fd = open(sharedMemName, O_RDWR, 07777);
    if (fd == -1) {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        return errno;
    }
    if (ftruncate(fd, BUFFERSIZE) == -1) {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        return errno;
    }
    pcBuffer = (char *) mmap(nullptr, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pcBuffer == nullptr) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return errno;
    }

    return 0;
}

int closeSharedMem(const char *sharedMemName, int fd, bool clearData) {
    if (clearData) {
        if (ftruncate(fd, 0) == -1) {
            fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
            return errno;
        }
        if (shm_unlink(sharedMemName) == -1) {
            fprintf(stderr, "shm_unlink failed: %s\n", strerror(errno));
            return errno;
        }
    }
    if (close(fd) == -1) {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        return errno;
    }
    return 0;
}

int main(int argc, char *argv[]) {

    unsigned stride = 4;
    if (argc > 1) {
        int iStride = atoi(argv[1]);
        if (iStride < 0) {
            stride = -iStride;
        } else {
            stride = iStride;
        }
    }

    static char g_LogFileName[] = "newcomair_123456789";

    char *sharedMemName = g_LogFileName;
    int fd = 0;
    char *pcBuffer = nullptr;
    auto err = openSharedMem(sharedMemName, fd, pcBuffer);
    if (err != 0) {
        return err;
    }
    parseRecord(pcBuffer, stride);

    closeSharedMem(sharedMemName, fd);
}