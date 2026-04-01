#include "ASCON-AEAD128.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>




int main(void)
{
    uint64_t key[2] = { 0x1E3EFB36779B1867, 0x427435C382DC5E53 };
    
    uint64_t nonce[2] = { 0, 0 };

    // uint64_t plaintext[] = { 3, 9, 0, 7, 3907, 0x1a, 0x39071a };


    uint64_t plaintext[] = { 0x3333333333333333, 0x9999999999999999, 0x000000000000000, 0x7777777777777777, 0x3907390739073907, 0x1a1a1a1a1a1a1a1a};
    unsigned plen = sizeof(plaintext)/sizeof(plaintext[0]);

    uint64_t ciphertext[7] = { 0 };

    uint64_t tag[2] = { 0 };

    ascon_encrypt(key, nonce, NULL, 0, plaintext, plen, ciphertext, tag);

    printf("\n\n\n===== Pre-Encryption Plaintext: =====\r\n");
    for (unsigned i = 0; i < plen; i++)
    {
        printf("%d: %016I64x\r\n", i, plaintext[i]);
    }

    printf("\n\n\n----- Ciphertext: -----\r\n");
    for (unsigned i = 0; i < plen; i++)
    {
        printf("%d: %016I64x\r\n", i, ciphertext[i]);
    }

    // printf("tag:\r\n");
    // for(unsigned i = 0; i < 2; i++)
    // {
    //     printf("%d: %016I64x\r\n", i, tag[i]);
    // }

    ascon_decrypt(key, nonce, NULL, 0, plaintext, plen, ciphertext, tag);

    printf("\n\n\n===== Decrypted Plaintext: =====\r\n");
    for (unsigned i = 0; i < plen; i++)
    {
        printf("%d: %016I64x\r\n", i, plaintext[i]);
    }

    //  printf("tag:\r\n");
    // for(unsigned i = 0; i < 2; i++)
    // {
    //     printf("%d: %016I64x\r\n", i, tag[i]);
    // }
}