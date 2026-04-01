#include "ASCON-HASH256.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


int main(void)
{
    uint64_t h[4] = { 0 };
    uint64_t m[] = { 0x3333333333333333, 0x9999999999999999, 0x000000000000000, 0x7777777777777777, 0x3907390739073907, 0x1a1a1a1a1a1a1a1a};



    printf("\n\n\n===== INPUT DATA =====\r\n");
    for(unsigned i = 0; i < 6; i++)
    {
        printf("%d: %016I64x\r\n", i, m[i]);
    }

    // hash here
    hash(m, 6, h);

    printf("\n\n\n----- HASH OUTPUT -----\r\n");
    for(unsigned i = 0; i < 4; i++)
    {
        printf("%d: %016I64x\r\n", i, h[i]);
    }

    printf("\n\n");


}