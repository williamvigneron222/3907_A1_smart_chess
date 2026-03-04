#ifndef ASCON_HASH256_H
#define ASCON_HASH256_H

#include "ASCON-AEAD128.h"


static const uint64_t HASH_IV = 0x0000080100cc0002;

void hash(uint64_t *m, unsigned mlen, uint64_t h[4]);


#endif /// ASCON_HASH256_H