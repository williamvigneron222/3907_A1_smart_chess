#ifndef KEYGEN_H
#define KEYGEN_H

#include <stdint.h>

void keygen_init();
void keygen_generate(uint8_t key_out[16]);

#endif // KEYGEN_H