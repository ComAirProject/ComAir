#ifndef APROFHOOKS_LIBRARY_H
#define APROFHOOKS_LIBRARY_H

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>


/* ---- page table ---- */

#define PAGE_SIZE     4096
#define L0_TABLE_SIZE 16
#define L1_TABLE_SIZE 512
#define L3_TABLE_SIZE 1024

#define L0_MASK  0xF0000000
#define L1_MASK  0xFF80000
#define L2_MASK  0x7FC00
#define L3_MASK  0x3FF

#define NEG_L3_MASK 0xFFFFFC00

#define STACK_SIZE 2000

/*---- end ----*/

/*---- share memory ---- */
#define BUFFERSIZE (unsigned long) 1 << 33
#define APROF_MEM_LOG "/aprof_log.log"

/*---- end ----*/

/*---- run time lib api ----*/

struct stack_elem {
    int funcId; // function id
    unsigned long long ts; // time stamp
    unsigned long long rms;
    unsigned long long cost;
};

#ifdef __cplusplus
extern "C" {

void aprof_init_page_table();

unsigned long aprof_query_page_table(unsigned long address);

void aprof_insert_page_table(unsigned long address, unsigned long count);

void aprof_destroy_memory();

char *aprof_init_share_mem();

void aprof_init();

void aprof_write(void *memory_addr, unsigned long length);

void aprof_read(void *memory, unsigned long length);

void aprof_increment_cost();

void aprof_increment_rms(unsigned long length);

void aprof_call_before(int funcId);

void aprof_collect();

void aprof_call_after();

void aprof_return(unsigned long numCost);

/*---- sampling generator ----*/

//===========================================================================
//=  Function to generate geometrically distributed random variables        =
//=    - Input:  Probability of success p                                   =
//=    - Output: Returns with geometrically distributed random variable     =
//===========================================================================

int aprof_geo(int iRate);            // Returns a geometric random variable

//=========================================================================
//= Multiplicative LCG for generating uniform(0.0, 1.0) random numbers    =
//=   - x_n = 7^5*x_(n-1)mod(2^31 - 1)                                    =
//=   - With x seeded to 1 the 10000th x value should be 1043618065       =
//=   - From R. Jain, "The Art of Computer Systems Performance Analysis," =
//=     John Wiley & Sons, 1991. (Page 443, Figure 26.2)                  =
//=========================================================================
static double aprof_rand_val(int seed);    // Jain's RNG

/*---- end ----*/

};
#endif

#endif
