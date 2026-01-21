/**
 * Implementation of NIST SP 800-232 "ASCON-AEAD128" using 64-bit unsigned integers
 * 
 *  https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-232.pdf
*/

#include <stdio.h> // TODO is this used ?


#include "ASCON-AEAD128.h"


// // TODO .h file / unused ?
// uint64_t key[2] = { 0 };
// uint64_t nonce[2] = { 0 };
// const uint64_t IV =  0x00001000808c0001;
// uint64_t s[5] = { 0 };
// // x[] for internal state storage



void parse(uint64_t *x, unsigned size, unsigned r) // todo pass sizeoof(x)
{
    unsigned l = size / r; // c truncates integer division, so the value is essentially floored.
    for (unsigned i = 0; i < (l-1); i++)
    {
        
    }
    
}


/**
 * appends 1 to x, followed by j  `0`s, where j=(-|x|-1) mod r
 * 
 * @param x bitstring input
 * @param r 
 */
void pad(uint64_t *x, unsigned r, uint64_t *out) // todo pass sizeof(x)
{
    // unsigned j = //(-1 * sizeof(x) - 1) % r
    // memcpy x to t, t[sizeof(x)] = 1, memcpy 0x00 j times
}

void sbox(uint64_t s[5])
{
    // temporary buffer
    uint64_t y[5] = { 0 };

    y[0] = s[4] & s[1] ^ s[3] ^ s[2] & s[1] ^ s[2] ^ s[1] & s[0] ^ s[1] ^ s[0];
    y[1] = s[4] ^ s[3] & s[2] ^ s[3] & s[1] ^ s[3] ^ s[2] & s[1] ^ s[2] ^ s[1] ^ s[0];
    y[2] = s[4] & s[3] ^ s[4] ^ s[2] ^ s[1] ^ 1;
    y[3] = s[4] & s[0] ^ s[4] ^ s[3] & s[0] ^ s[3] ^ s[2] ^ s[1] ^ s[0];
    y[4] = s[4] & s[1] ^ s[4] ^ s[3] ^ s[1] & s[0] ^ s[1];

    // copy over values
    for (unsigned i = 0; i < 5; i++) s[i] = y[i];

}

void Ascon_p(uint64_t s[5], unsigned rnd)
{
    for (unsigned i = 0; i < rnd; i++)
    {
        // constant-addition
        // get c-value for this round
        uint64_t c = roundConstants[16 - rnd + i];

        s[2] ^= c;
        
        // substitution
        sbox(s);

        // linear diffusion
        s[0] = s[0] ^ ROTR(s[0], 19) ^ ROTR(s[0], 28);
        s[1] = s[1] ^ ROTR(s[1], 61) ^ ROTR(s[1], 39);
        s[2] = s[2] ^ ROTR(s[2],  1) ^ ROTR(s[2],  6);
        s[3] = s[3] ^ ROTR(s[3], 10) ^ ROTR(s[3], 17);
        s[4] = s[4] ^ ROTR(s[4],  7) ^ ROTR(s[4], 41);
    }

}

void encrypt(uint64_t key[2], uint64_t nonce[2], uint64_t *associatedData, uint64_t *plaintext)
{
    uint64_t s[5] = { 0 };
    
    // Initial population of the state bits
    s[0] = IV;
    s[1] = key[0];
    s[2] = key[1];
    s[3] = nonce[0];
    s[4] = nonce[1];

    Ascon_p(s, 12);

    s[3] ^= key[0];
    s[4] ^= key[1];

    // ASSOCIATED DATA START
    
    if (sizeof(associatedData) > 0)
    {
        // process associatedData (into "A")
        // pad associatedData[-1] with 128
        // for (unsigned i = 0; i < array len associatedData i++)
        {
            /// s[0] = first half of A
            /// s[1] = second half of A
            Ascon_p(s, 8);
        }
    }

    s[4] ^= 1;

    // ASSOCIATED DATA END
}


// void ascon_p(uint64_t s[5], unsigned i, unsigned rnd)
// {
//     // assert rnd <= 16
//     for (unsigned i = 0; i < rnd; i++)
//     {
//         // constant-addition layer
//         const uint64_t c = roundConstants[16 - rnd + i];
//         s[2] ^= c;
//     }
// }