/**
 * Implementation of NIST SP 800-232 "ASCON-AEAD128" using 64-bit unsigned integers
 *
 *  https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-232.pdf
*/

#include <stdio.h>

#include "ASCON.h"


void parse(uint64_t *x, unsigned size, unsigned r) // todo pass sizeof(x)
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
    // INITIALIZATION START
    uint64_t s[5] = { 0 };

    s[0] = AEAD_IV;
    s[1] = key[0];
    s[2] = key[1];
    s[3] = nonce[0];
    s[4] = nonce[1];

    Ascon_p(s, 12);

    s[3] ^= key[0];
    s[4] ^= key[1];

    // INITIALIZATION END

    // AD START
    // AD END

    // PLAINTEXT START

    // 64-bit array elements, so there are plen/2 128-bit blocks
    // TODO this can be bitwise operation - round up to an even number
    if (plen % 2 != 0) 
    {
        p[plen] = 0;
        plen++;
    }

    for (unsigned i = 0; i < plen / 2; i++)
    {
        s[0] ^= p[2 * i];
        s[1] ^= p[2 * i + 1];

        c[2 * i] = s[0];
        c[2 * i + 1] = s[1];

        Ascon_p(s, 8);
    }
    // PLAINTEXT END

    // FINALIZATION START
    s[2] ^= key[0];
    s[3] ^= key[1];

    Ascon_p(s, 12);

    tag[0] = s[3] ^ key[0];
    tag[1] = s[4] ^ key[1];
    // FINALIZATION END
}

void decrypt(uint64_t key[2], uint64_t nonce[2], uint64_t *ad, unsigned adlen, uint64_t *p, 
    unsigned plen, uint64_t *c, uint64_t tag[2])
{
    // INITIALIZATION START
    uint64_t s[5] = { 0 };
    
    s[0] = AEAD_IV;
    s[1] = key[0];
    s[2] = key[1];
    s[3] = nonce[0];
    s[4] = nonce[1];

    Ascon_p(s, 12);

    s[3] ^= key[0];
    s[4] ^= key[1];
    // INITIALIZATION END

    // ASSOCIATED DATA START
    // ASSOCIATED DATA END

    // CIPHERTEXT START
    for (unsigned i = 0; i < plen / 2; i++)
    {
        p[2 * i] = s[0] ^ c[2 * i];
        p[2 * i + 1] = s[1] ^ c[2 * i + 1];

        s[0] = c[2 * i];
        s[1] = c[2 * i + 1];

        Ascon_p(s, 8);
    }
    // CIPHERTEXT END

    // FINALIZATION START
    s[2] ^= key[0];
    s[3] ^= key[1];

    Ascon_p(s, 12);

    tag[0] = s[3] ^ key[0];
    tag[1] = s[4] ^ key[1];
    // FINALIZATION END
}



// TODO
// This is a temporary test to verify enrcyption/decryption works.
int main(void)
{
    uint64_t key[2] = { 0, 0 };
    uint64_t nonce[2] = { 0, 0 };
    uint64_t plaintext[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };
    unsigned plen = 17;

    uint64_t ciphertext[17] = { 0 };

    uint64_t tag[2] = { 0 };

    encrypt(key, nonce, NULL, 0, plaintext, plen, ciphertext, tag);

    printf("Plaintext:\r\n");
    for (unsigned i = 0; i < plen; i++)
    {
        printf("%d: %016I64x\r\n", i, plaintext[i]);
    }

    printf("Ciphertext:\r\n");
    for (unsigned i = 0; i < plen; i++)
    {
        printf("%d: %016I64x\r\n", i, ciphertext[i]);
    }

    printf("tag:\r\n");
    for(unsigned i = 0; i < 2; i++)
    {
        printf("%d: %016I64x\r\n", i, tag[i]);
    }

    decrypt(key, nonce, NULL, 0, plaintext, plen, ciphertext, tag);

    printf("Plaintext:\r\n");
    for (unsigned i = 0; i < plen; i++)
    {
        printf("%d: %016I64x\r\n", i, plaintext[i]);
    }

     printf("tag:\r\n");
    for(unsigned i = 0; i < 2; i++)
    {
        printf("%d: %016I64x\r\n", i, tag[i]);
    }
}