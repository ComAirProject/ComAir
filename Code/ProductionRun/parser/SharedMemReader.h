#ifndef NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H
#define NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H

#ifndef TODISK
#define BUFFERSIZE ((1UL << 33))
#else
#define BUFFERSIZE ((1UL << 38))
#endif

int openSharedMem(const char *sharedMemName, int &fd, char *&pcBuffer);

int closeSharedMem(const char *sharedMemName, int fd, bool clearData = false);

#endif //NEWCOMAIR_DUMPMEM_SHAREDMEMREADER_H
