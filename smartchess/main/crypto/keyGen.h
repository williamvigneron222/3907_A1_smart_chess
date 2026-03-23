



#ifndef KEYGEN_H
#define KEYGEN_H

#include <stdint.h>

static const char *KEYNAME = "KEY";

/**
 * Generate a new 128-bit key using ESP32 hardware RNG
 * and store it in flash via Preferences.
 * Call this once during IR pairing sequence.
 */
void newKey();

/**
 * Load the stored 128-bit key from flash into b[0] and b[1].
 * Call this on every boot before any encrypt/decrypt.
 */
void getKey(uint64_t b[2]);

/**
 * Generate a 128-bit key into b[0] and b[1] using hardware RNG.
 * Uses two independent random sources XOR'd together for stronger randomness.
 */
void generate(uint64_t b[2]);

#endif /// KEYGEN_H

/*

#ifndef KEYGEN_H
#define KEYGEN_H

#include <stdint.h>


void getKey(uint64_t b[2]);

void newKey();

static const char *KEYNAME = "KEY";


#endif /// KEYGEN_H

*/