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
#ifndef TODISK
static const char *g_LogFileName = "newcomair_123456789";
#else
static const char *g_LogFileName = "/mnt/d/newcomair_123456789";
#endif

// the file descriptor of the shared memory, need to be closed at the end
static int fd = -1;

// the buffer size of the shared memory
#ifndef TODISK
#define BUFFERSIZE ((1UL << 33))
#else
#define BUFFERSIZE ((1UL << 38))
#endif

// the ptr to buffer
char *pcBuffer;

/**
 * Open a shared memory to store results, provide a ptr->buffer to operate on.
 */
char *InitMemHooks()
{
#ifndef TODISK
    fd = shm_open(g_LogFileName, O_RDWR | O_CREAT, 0777);
#else
    fd = open(g_LogFileName, O_RDWR | O_CREAT, 0777);
#endif
    if (fd == -1)
    {
        fprintf(stderr, "open failed: %s\n", strerror(errno));
        exit(-1);
    }
    if (ftruncate(fd, BUFFERSIZE) == -1)
    {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        exit(-1);
    }
    pcBuffer = (char *)mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
    if (munmap(pcBuffer, BUFFERSIZE) == -1)
    {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        exit(-1);
    }
    if (ftruncate(fd, iBufferIndex) == -1)
    {
        fprintf(stderr, "ftruncate failed: %s\n", strerror(errno));
        exit(-1);
    }
    close(fd);
}