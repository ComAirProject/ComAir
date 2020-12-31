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

int openSharedMem(const char *sharedMemName, int &fd, char *&pcBuffer)
{
#ifndef TODISK
    fd = shm_open(sharedMemName, O_RDWR, 0777);
#else
    fd = open(sharedMemName, O_RDWR, 0777);
#endif
    if (fd == -1)
    {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        return errno;
    }
    if (ftruncate(fd, BUFFERSIZE) == -1)
    {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        return errno;
    }
    pcBuffer = (char *)mmap(nullptr, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pcBuffer == nullptr)
    {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return errno;
    }

    return 0;
}

int closeSharedMem(const char *sharedMemName, int fd, bool clearData)
{
    if (clearData)
    {
        if (ftruncate(fd, 0) == -1)
        {
            fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
            return errno;
        }
#ifndef TODISK
        if (shm_unlink(sharedMemName) == -1)
        {
            fprintf(stderr, "shm_unlink failed: %s\n", strerror(errno));
            return errno;
        }
#endif
    }
    if (close(fd) == -1)
    {
        fprintf(stderr, "close failed: %s\n", strerror(errno));
        return errno;
    }
    return 0;
}

int main(int argc, char *argv[])
{
#ifndef TODISK
    static char g_LogFileName[] = "newcomair_123456789";
#else
    static char g_LogFileName[] = "/mnt/d/newcomair_123456789";
#endif

    char *sharedMemName = g_LogFileName;
    int fd = 0;
    char *pcBuffer = nullptr;
    auto err = openSharedMem(sharedMemName, fd, pcBuffer);
    if (err != 0)
    {
        return err;
    }
    //parseRecord(pcBuffer);
    parseRecordNoSample(pcBuffer);
    //parseRecordDebug(pcBuffer);

    closeSharedMem(sharedMemName, fd);
}