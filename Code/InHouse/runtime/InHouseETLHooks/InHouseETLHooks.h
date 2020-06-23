#ifndef INHOUSE_INHOUSEETLHOOKS_H
#define INHOUSE_INHOUSEETLHOOKS_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- page table ---- */
// Address is 48 bits for 64 bit systems
#define PAGE_SIZE 4096
#define L0_TABLE_SIZE PAGE_SIZE
#define L1_TABLE_SIZE PAGE_SIZE
#define L2_TABLE_SIZE PAGE_SIZE
#define L3_TABLE_SIZE PAGE_SIZE

#define L0_MASK 0xFFF000000000
#define L1_MASK 0x000FFF000000
#define L2_MASK 0x000000FFF000
#define L3_MASK 0x000000000FFF

#define L0_OFFSET 36
#define L1_OFFSET 24
#define L2_OFFSET 12

#define NEG_L3_MASK 0xFFFFFFFFF000

#define STACK_SIZE 2000

    typedef unsigned CountTy;

    CountTy aprof_query_insert_page_table(unsigned long start_addr, CountTy count);

/*---- end ----*/

/*---- share memory ---- */
#define BUFFERSIZE (unsigned long)1UL << 38
#define APROF_MEM_LOG "/media/boqin/New Volume/aprof_log.log"

    /*---- end ----*/

    /*---- run time lib api ----*/

    struct stack_elem
    {
        unsigned funcId; // function id
        unsigned ts;     // time stamp
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

#ifdef __cplusplus
}
#endif

#endif //INHOUSE_INHOUSEETLHOOKS_H
