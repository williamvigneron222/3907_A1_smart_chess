#ifndef KEYGEN_H
#define KEYGEN_H

#include <stdint.h>


void getKey(uint64_t b[2]);

void newKey();

static const char *KEYNAME = "KEY";


#endif /// KEYGEN_H