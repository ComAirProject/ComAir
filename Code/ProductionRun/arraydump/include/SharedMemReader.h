#ifndef NEWCOMAIR_ARRAYDUMP_SHAREDMEMREADER_H
#define NEWCOMAIR_ARRAYDUMP_SHAREDMEMREADER_H

#define BUFFERSIZE (1UL << 33)

int openSharedMem(const char *sharedMemName, int &fd, char *&pcBuffer);

int closeSharedMem(const char *sharedMemName, int fd, bool clearData = false);


#endif //NEWCOMAIR_ARRAYDUMP_SHAREDMEMREADER_H
