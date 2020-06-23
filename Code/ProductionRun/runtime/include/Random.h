#ifndef PRODUCTIONRUN_RANDOM_H
#define PRODUCTIONRUN_RANDOM_H

/*---- sampling generator ----*/

//===========================================================================
//=  Function to generate geometrically distributed random variables        =
//=    - Input:  Probability of success p                                   =
//=    - Output: Returns with geometrically distributed random variable     =
//===========================================================================

int geo(int iRate);            // Returns a geometric random variable

//=========================================================================
//= Multiplicative LCG for generating uniform(0.0, 1.0) random numbers    =
//=   - x_n = 7^5*x_(n-1)mod(2^31 - 1)                                    =
//=   - With x seeded to 1 the 10000th x value should be 1043618065       =
//=   - From R. Jain, "The Art of Computer Systems Performance Analysis," =
//=     John Wiley & Sons, 1991. (Page 443, Figure 26.2)                  =
//=========================================================================
static double rand_val(int seed);    // Jain's RNG

/*---- end ----*/

#endif //PRODUCTIONRUN_RANDOM_H
