//
// Shared memory
//

#include "Shmem.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// the log file name
// static const char *g_LogFileName = "newcomair_123456789";
static const char *g_LogFileName = "/media/boqin/New Volume/newcomair_123456789";

// the file descriptor of the shared memory, need to be closed at the end
static int fd = -1;

// the buffer size of the shared memory
#define BUFFERSIZE ((1UL << 38))
// #define BUFFERSIZE ((1UL << 37))
// #define BUFFERSIZE ((1UL << 34))

/**
 * Open a shared memory to store results, provide a ptr->buffer to operate on.
 */
char *InitMemHooks()
{
    // fd = shm_open(g_LogFileName, O_RDWR | O_CREAT, 0777);
    fd = open(g_LogFileName, O_RDWR | O_CREAT, 0777);
    if (fd == -1)
    {
        fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        exit(-1);
    }
    if (ftruncate(fd, BUFFERSIZE) == -1)
    {
        fprintf(stderr, "fstruncate failed: %s\n", strerror(errno));
        exit(-1);
    }
    char *pcBuffer = (char *)mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pcBuffer == NULL)
    {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        exit(-1);
    }
    return pcBuffer;
}

/**
 * Truncate the shared memory buffer to the actual data size, then close.
 */
void FinalizeMemHooks(unsigned long iBufferIndex)
{
    if (ftruncate(fd, iBufferIndex) == -1)
    {
        fprintf(stderr, "ftruncate: %s\n", strerror(errno));
        exit(-1);
    }
    close(fd);
}
