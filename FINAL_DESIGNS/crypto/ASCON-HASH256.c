#include <stdio.h>

#include "ASCON-HASH256.h"

void hash(uint64_t *m, unsigned mlen, uint64_t h[4])
{
    uint64_t s[5] = { 0 };


    s[0] = HASH_IV;

    for (unsigned i = 0; i < mlen - 1; i++) 
    {
        s[0] ^= m[i];
        Ascon_p(s, (unsigned)12);
    }
    s[0] ^= m[mlen - 1];

    // Squeezing
    Ascon_p(s, (unsigned)12);

    for (unsigned i = 0; i < 3; i++)
    {
        h[i] = s[0];
        Ascon_p(s, (unsigned)12);
    }

    h[3] = s[0];
}

// int main(void)
// {
//     uint64_t h[4] = { 0 };
//     uint64_t m[2] = { 0, 1 }; // mlen 2

//     for(unsigned i = 0; i < 2; i++)
//     {
//         printf("%d: %016I64x\r\n", i, h[i]);
//     }

//     // hash here
//     hash(m, 2, h);

//     printf("---\r\n");
//     for(unsigned i = 0; i < 2; i++)
//     {
//         printf("%d: %016I64x\r\n", i, h[i]);
//     }
    
// }