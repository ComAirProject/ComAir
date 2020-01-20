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
#define BUFFERSIZE (unsigned long) 1UL << 34
#define APROF_MEM_LOG "/aprof_loop_array_list_log.log"

/*---- end ----*/

/*---- run time lib api ----*/

struct log_address {
    unsigned long  numCost;
    int  length;
};

void aprof_init();

void aprof_call_in();

void aprof_dump(void *memory_addr, int length);

void aprof_return(unsigned long numCost);

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

#endif
