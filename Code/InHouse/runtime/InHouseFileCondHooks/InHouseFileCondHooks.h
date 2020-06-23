#ifndef INHOUSE_INHOUSEFILECONDHOOKS_H
#define INHOUSE_INHOUSEFILECONDHOOKS_H

/* ---- page table ---- */

#define PAGE_SIZE     4096
#define L0_TABLE_SIZE 1048576
#define L1_TABLE_SIZE 512
#define L3_TABLE_SIZE 1024

#define L0_MASK  0xFFFFFFFFF0000000
#define L1_MASK  0xFF80000
#define L2_MASK  0x7FC00
#define L3_MASK  0x3FF

#define NEG_L3_MASK 0xFFFFFFFFFFFFFC00

#define STACK_SIZE 2000

typedef unsigned CountTy;

CountTy aprof_query_insert_page_table(unsigned long start_addr, CountTy count);

/*---- end ----*/

/*---- share memory ---- */
#define BUFFERSIZE (unsigned long) 1UL << 38
#define APROF_MEM_LOG "/media/boqin/New Volume/aprof_log.log"

/*---- end ----*/

/*---- run time lib api ----*/

struct stack_elem {
    unsigned funcId; // function id
    unsigned ts; // time stamp
    long rms;
    unsigned long cost;
};

void aprof_init();

void aprof_write(unsigned long start_addr, unsigned long length);

void aprof_read(unsigned long start_addr, unsigned long length);

void aprof_increment_rms(unsigned long length);

void aprof_call_before(unsigned funcId);

void aprof_return(unsigned long numCost);

void aprof_final();


#endif //INHOUSE_INHOUSEFILECONDHOOKS_H
