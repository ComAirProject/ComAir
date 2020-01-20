#ifndef PRODUCTIONRUN_SHMEM_H
#define PRODUCTIONRUN_SHMEM_H

/**
 * Open a shared memory to store results, provide a ptr->buffer to operate on.
 * @return ptr to shared mem buffer.
 */
char* InitMemHooks();

/**
 * Truncate the shared memory buffer to the actual data size, then close.
 * @param iBufferIndex curr index of shared mem buffer.
 */
void FinalizeMemHooks(unsigned long iBufferIndex);

#endif //PRODUCTIONRUN_SHMEM_H
