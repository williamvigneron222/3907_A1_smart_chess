#include <stdio.h>

#include "ASCON.h"

void hash(uint64_t *m, unsigned mlen, uint64_t h[4])
{
    uint64_t s[5] = { 0 };


    s[0] = HASH_IV;

    for (unsigned i = 0; i < mlen - 1; i++) 
    {
        s[0] ^= m[i];
        Ascon_p(s, 12);
    }
    s[0] ^= m[mlen - 1];


    // Squeezing
    Ascon_p(s, 12);

    for (unsigned i = 0; i < 3; i++)
    {
        h[i] = s[0];
        Ascon_p(s, 12);
    }

    h[3] = s[0];
}