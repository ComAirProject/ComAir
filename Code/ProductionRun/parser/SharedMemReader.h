#ifndef NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H
#define NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H

// #define BUFFERSIZE ((1UL << 38))
// #define BUFFERSIZE ((1UL << 37))
#define BUFFERSIZE ((1UL << 34))

int openSharedMem(const char *sharedMemName, int &fd, char *&pcBuffer);

int closeSharedMem(const char *sharedMemName, int fd, bool clearData = false);

#endif //NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H
