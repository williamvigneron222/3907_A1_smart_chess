/**
 * Implementation of NIST SP 800-232 "ASCON-AEAD128" using 64-bit unsigned integers
 *
 *  https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-232.pdf
*/

#include <stdio.h>

#include "ASCON-AEAD128.h"


void parse(uint64_t *x, unsigned size, unsigned r) // todo pass sizeoof(x)
{
    unsigned l = size / r; // c truncates integer division, so the value is essentially floored.
    for (unsigned i = 0; i < (l-1); i++)
    {
        
    }
    
}

void pad(uint64_t *x, unsigned r, uint64_t *out) // todo pass sizeof(x)
{
    // unsigned j = //(-1 * sizeof(x) - 1) % r
    // memcpy x to t, t[sizeof(x)] = 1, memcpy 0x00 j times
}

void sbox(uint64_t s[5])
{
    // from https://ascon.isec.tugraz.at/images/sbox_instructions.c
    uint64_t t[5] = { 0 };
    
    s[0] ^= s[4];    s[4] ^= s[3];    s[2] ^= s[1];
    t[0]  = s[0];    t[1]  = s[1];    t[2]  = s[2];    t[3]  = s[3];    t[4]  = s[4];
    t[0] =~ t[0];    t[1] =~ t[1];    t[2] =~ t[2];    t[3] =~ t[3];    t[4] =~ t[4];
    t[0] &= s[1];    t[1] &= s[2];    t[2] &= s[3];    t[3] &= s[4];    t[4] &= s[0];
    s[0] ^= t[1];    s[1] ^= t[2];    s[2] ^= t[3];    s[3] ^= t[4];    s[4] ^= t[0];
    s[1] ^= s[0];    s[0] ^= s[4];    s[3] ^= s[2];    s[2] =~ s[2];
}

void Ascon_p(uint64_t s[5], unsigned rnd)
{
    for (unsigned i = 0; i < rnd; i++)
    {
        // Constant-Addition
        s[2] ^= roundConstants[16 - rnd + i];
        
        // Substitution
        sbox(s);

        // Linear Diffusion
        s[0] = s[0] ^ ROTR(s[0], 19) ^ ROTR(s[0], 28);
        s[1] = s[1] ^ ROTR(s[1], 61) ^ ROTR(s[1], 39);
        s[2] = s[2] ^ ROTR(s[2],  1) ^ ROTR(s[2],  6);
        s[3] = s[3] ^ ROTR(s[3], 10) ^ ROTR(s[3], 17);
        s[4] = s[4] ^ ROTR(s[4],  7) ^ ROTR(s[4], 41);
    }

}

void encrypt(uint64_t key[2], uint64_t nonce[2], uint64_t *ad, unsigned adlen, uint64_t *p, 
    unsigned plen, uint64_t *c, uint64_t tag[2])
{
    uint64_t s[5] = { 0 };
    
    // Initial population of the state bits
    // #15
    s[0] = IV;
    s[1] = key[0];
    s[2] = key[1];
    s[3] = nonce[0];
    s[4] = nonce[1];

    // #16
    Ascon_p(s, 12);

    // #17
    s[3] ^= key[0];
    s[4] ^= key[1];

    // ASSOCIATED DATA START
    // "metadata". probably cbor or something

    if (adlen > 0)
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

    // #22
    s[4] ^= 1;
    // ASSOCIATED DATA END

    // PLAINTEXT START

    // 64-bit array elements, so there are plen/2 128-bit blocks
    // #23
    int blocks = plen / 2;

    // pad last block (whatever)
    // TODO: ignore padding when decrypting
    if (plen % 2 != 0) 
    {
        p[plen] = 0; // [plen++] ?
        plen++;
    }

    // #24
    for (unsigned i = 0; i < plen / 2; i++)
    {
        s[0] ^= p[2 * i];
        s[1] ^= p[2 * i + 1];

        c[0] = s[0];
        c[1] = s[1];

        Ascon_p(s, 8);
    }

    // TODO pad is used for last block, pad 1. #27

    c[plen-];
    




    // parse plaintext into P [n]
    // l = sizeof(p[n])

    // PLAINTEXT END

    // FINALIZATION START

    s[0] = 0;
    s[1] = 0;
    s[2] = key[0];
    s[3] = key[1];
    s[4] = 0;

    Ascon_p(s, 12);

    tag[0] = s[0] ^ key[0];
    tag[1] = s[1] ^ key[1];
    // FINALIZATION END
}

// void decrypt - DROP PLAINTEXT PADDING !!!!!


int main(void)
{
    uint64_t test[5] = { 0 };
    test[0] = IV;

    for (unsigned i = 0; i < 5; i++)
    {
        printf("0x%016I64x\n", test[i]);
    }

    Ascon_p(test, 12);

    for (unsigned i = 0; i < 5; i++)
    {
        printf("0x%016I64x\n", test[i]);
    }
}