#include "Preferences.h"

#include "keyGen.h"
#include "ASCON-HASH256.h"

/**
 * generate a 128-bit key for use in ASCON-128AEAD algorithm
 * 
 * @param b output buffer
 */
void generate(uint64_t b[2])
{
    uint64_t u[2] = { 0 };
    uint64_t v[2] = { 0 };

    // initialize u & v here:
    // U will call rng, intake 128 bits
    // V will call rng intake 128 bits then hash itself

    // XOR u and v
    b[0] = u[0] ^ v[0];
    b[1] = u[1] ^ v[1];
}


void getKey(uint64_t b[2])
{
    // need preferences library
    // preferences retrieve key using keyname
}

void newKey()
{
    // need preferences library

    uint64_t b[2] = { 0 };
    // generate(b);
    // preferences store uint64 b[0]
    // preferences store uint64 b[1];
}