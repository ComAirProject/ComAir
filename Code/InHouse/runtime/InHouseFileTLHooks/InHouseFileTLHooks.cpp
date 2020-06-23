#include "InHouseFileTLHooks.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>

// page table
// L0  28-31
// L1  19-27
// L2  10-18
// L3   0-9

void **pL0 = NULL;
void **pL1 = NULL;
void **pL2 = NULL;

unsigned long *pL3 = NULL;

unsigned long prev = 0;
unsigned long *prev_pL3 = NULL;

// page table
unsigned long count = 0;

struct stack_elem shadow_stack[STACK_SIZE];
int stack_top = -1;

// share memory
int fd = -1;
thread_local char *pcBuffer = nullptr;
unsigned int struct_size = sizeof(struct stack_elem);

thread_local bool start_record = false;

// aprof api

void aprof_init() {

    // init share memory
    fd = open(APROF_FILE_LOG, O_RDWR | O_CREAT | O_EXCL, 0777);

    if (fd < 0) {
        fd = open(APROF_FILE_LOG, O_RDWR, 0777);
    }

    ftruncate(fd, BUFFERSIZE);

    pcBuffer = (char *) mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);

    // init page table
    pL0 = (void **) malloc(sizeof(void *) * L0_TABLE_SIZE);
    memset(pL0, 0, sizeof(void *) * L0_TABLE_SIZE);

    start_record = true;
}


unsigned long aprof_query_insert_page_table(unsigned long addr, unsigned long count) {

    unsigned long pre_value = 0;
    if (prev_pL3 && (addr & NEG_L3_MASK) == prev) {
        pre_value = prev_pL3[addr & L3_MASK];
        prev_pL3[addr & L3_MASK] = count;
        return pre_value;
    }

    unsigned long tmp = (addr & L0_MASK) >> 28;

    if (pL0[tmp] == NULL) {
        pL0[tmp] = (void **) malloc(sizeof(void *) * L1_TABLE_SIZE);
        memset(pL0[tmp], 0, sizeof(void *) * L1_TABLE_SIZE);
    }

    pL1 = (void **) pL0[tmp];

    tmp = (addr & L1_MASK) >> 19;

    if (pL1[tmp] == NULL) {

        pL1[tmp] = (void **) malloc(sizeof(void *) * L1_TABLE_SIZE);
        memset(pL1[tmp], 0, sizeof(void *) * L1_TABLE_SIZE);
    }

    pL2 = (void **) pL1[tmp];

    tmp = (addr & L2_MASK) >> 10;

    if (pL2[tmp] == NULL) {
        pL2[tmp] = (unsigned long *) malloc(sizeof(unsigned long) * L3_TABLE_SIZE);
        memset(pL2[tmp], 0, sizeof(unsigned long) * L3_TABLE_SIZE);
    }

    pL3 = (unsigned long *) pL2[tmp];

    prev = addr & NEG_L3_MASK;
    prev_pL3 = pL3;
    pre_value = prev_pL3[addr & L3_MASK];
    pL3[addr & L3_MASK] = count;
    return pre_value;
}


void aprof_write(unsigned long start_addr, unsigned long length) {

    if (start_record) {
        unsigned long end_addr = start_addr + length;

        for (; start_addr < end_addr; start_addr++) {
            aprof_query_insert_page_table(start_addr, count);
        }
    }
}


void aprof_read(unsigned long start_addr, unsigned long length) {

    if (start_record) {
        unsigned long end_addr = start_addr + length;
        int j;

        for (; start_addr < end_addr; start_addr++) {

            // We assume that w has been wrote before reading.
            // ts[w] > 0 and ts[w] < S[top]
            unsigned long ts_w = aprof_query_insert_page_table(start_addr, count);
            if (ts_w < shadow_stack[stack_top].ts) {

                shadow_stack[stack_top].rms++;

                if (ts_w != 0) {
                    for (j = stack_top - 1; j >= 0; j--) {

                        if (shadow_stack[j].ts <= ts_w) {
                            shadow_stack[j].rms--;
                            break;
                        }
                    }
                }
            }
        }
    }
}


void aprof_increment_rms(unsigned long length) {

    if (start_record) {
        shadow_stack[stack_top].rms += length;
    }
}


void aprof_call_before(unsigned funcId) {

    if (start_record) {
        count++;
        stack_top++;
        shadow_stack[stack_top].funcId = funcId;
        shadow_stack[stack_top].ts = count;
        shadow_stack[stack_top].rms = 0;
        // newEle->cost update in aprof_return
        shadow_stack[stack_top].cost = 0;
    }
}


void aprof_return(unsigned long numCost) {

    if (start_record) {
        shadow_stack[stack_top].cost += numCost;
        // length of call chains
        // shadow_stack[stack_top].ts = count - shadow_stack[stack_top].ts;
        memcpy(pcBuffer, &(shadow_stack[stack_top]), struct_size);
        pcBuffer += struct_size;
        shadow_stack[stack_top - 1].rms += shadow_stack[stack_top].rms;
        shadow_stack[stack_top - 1].cost += shadow_stack[stack_top].cost;
        stack_top--;
    }
}

void aprof_final() {

    if (start_record) {
        start_record = false;
        // close  share memory
        close(fd);

        // free malloc memory
        int i, j, k;

        for (i = 0; i < L0_TABLE_SIZE; i++) {
            if (pL0[i] == NULL) {
                continue;
            }

            pL1 = (void **) pL0[i];

            for (j = 0; j < L1_TABLE_SIZE; j++) {
                if (pL1[j] == NULL) {
                    continue;
                }

                pL2 = (void **) pL1[j];

                for (k = 0; k < L1_TABLE_SIZE; k++) {
                    if (pL2[k] != NULL) {
                        free(pL2[k]);
                    }
                }

                free(pL2);
            }

            free(pL1);
        }

        free(pL0);
    }
}

