/**
 * Implementation of NIST SP 800-232 "ASCON-AEAD128" using 64-bit unsigned integers
 *
 *  https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-232.pdf
*/

#include <stdint.h> // uint64_t

// Right Rotate uint64_t x by i bits
#define ROTR(x, i) (x>>i)^(x<<64-i)


/////////////////////////
// Global Constants
/////////////////////////

const uint64_t IV = 0x00001000808c0001;

const uint64_t roundConstants[16] = {
    0x000000000000003c,
    0x000000000000002d,
    0x000000000000001e,
    0x000000000000000f,
    0x00000000000000f0,
    0x00000000000000e1,
    0x00000000000000d2,
    0x00000000000000c3,

    0x00000000000000b4,
    0x00000000000000a5,
    0x0000000000000096,
    0x0000000000000087,
    0x0000000000000078,
    0x0000000000000096,
    0x000000000000005a,
    0x000000000000004b,
};

/////////////////////////
// Function Declarations
/////////////////////////

/**
 * Parse the continuous bit stream x into sequence of l blocks,
 * where l=sizeof(x) / r
 * 
 * the output blocks each have a bit length r
 * 
 * NOTE: for ASCON-AEAD128, r is always 128.
 * 
 * @param x bitstring input
 * @param size sizeof(x)
 * @param r bit length of each x block
 * 
 * @param out output buffer
 */
void parse(uint64_t *x, unsigned size, unsigned r); // TODO param out

/**
 * Append 1 to the bitstring x then append r zeros
 * 
 * @param x input bitstring
 * @param r sizeof(x[0])
 */
void pad(uint64_t *x, unsigned r, uint64_t *out);

/**
 * SBOX operation
 */
void sbox(uint64_t s[5]);

/**
 * The ASCON permutation function
 * 
 * @param s bit state
 * @param rnd round
 */
void Ascon_p(uint64_t s[5], unsigned rnd);


void encrypt(uint64_t key[2], uint64_t nonce[2], uint64_t *ad, unsigned adlen, uint64_t *p, 
    unsigned plen, uint64_t *c, uint64_t tag[2]);

// void decrypt
