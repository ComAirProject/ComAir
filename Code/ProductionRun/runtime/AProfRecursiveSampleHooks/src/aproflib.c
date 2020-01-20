#include "aproflib.h"

// page table

struct log_address store_stack[1];

// share memory
int fd;
void *pcBuffer = NULL;
unsigned int store_size = sizeof(struct log_address);

int record_log = 0;

// sampling
static int old_value = -1;

// aprof api

void aprof_init() {

    // init share memory
    fd = shm_open(APROF_MEM_LOG, O_RDWR | O_CREAT | O_EXCL, 0777);

    if (fd < 0) {
        fd = shm_open(APROF_MEM_LOG, O_RDWR, 0777);

    } else
        ftruncate(fd, BUFFERSIZE);

    pcBuffer = (void *) mmap(0, BUFFERSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

}

void aprof_read(void *memory_addr, unsigned long length) {
    unsigned long start_addr = (unsigned long) memory_addr;
    if (record_log == 0) {
        if (store_stack[0].start_addr != start_addr || store_stack[0].length != length) {
            store_stack[0].flag = 'r';
            store_stack[0].start_addr = start_addr;
            store_stack[0].length = length;
            memcpy(pcBuffer, &(store_stack), store_size);
            pcBuffer += store_size;
        }

    } else {

        store_stack[0].flag = 'r';
        store_stack[0].start_addr = start_addr;
        store_stack[0].length = length;
        memcpy(pcBuffer, &(store_stack), store_size);
        pcBuffer += store_size;
        record_log = 0;
    }

}


void aprof_return(unsigned long numCost, int sample) {
    if (record_log == 0) {
        store_stack[0].flag = 'e';
        store_stack[0].start_addr = sample;
        // carefull !!!
        store_stack[0].length = numCost;
        memcpy(pcBuffer, &(store_stack), store_size);
        pcBuffer += store_size;
        record_log = 1;
    }
}

static double aprof_rand_val(int seed) {
    const long a = 16807;  // Multiplier
    const long m = 2147483647;  // Modulus
    const long q = 127773;  // m div a
    const long r = -2836;  // m mod a
    static long x;               // Random int value
    long x_div_q;         // x divided by q
    long x_mod_q;         // x modulo q
    long x_new;           // New x value

    // Set the seed if argument is non-zero and then return zero
    if (seed > 0) {
        x = seed;
        return (0.0);
    }

    // RNG using integer arithmetic
    x_div_q = x / q;
    x_mod_q = x % q;
    x_new = (a * x_mod_q) - (r * x_div_q);
    if (x_new > 0)
        x = x_new;
    else
        x = x_new + m;

    // Return a random value between 0.0 and 1.0
    return ((double) x / m);
}


int aprof_geo(int iRate) {
    double p = 1 / (double) iRate;
    double z;                     // Uniform random number (0 < z < 1)
    double geo_value;             // Computed geometric value to be returned

    do {
        // Pull a uniform random number (0 < z < 1)
        do {
            z = aprof_rand_val(0);
        } while ((z == 0) || (z == 1));

        // Compute geometric random variable using inversion method
        geo_value = (int) (log(z) / log(1.0 - p)) + 1;
    } while ((int) geo_value == old_value + 1);

    old_value = (int) geo_value;
    // log sampling call chain number
    return old_value;
}
