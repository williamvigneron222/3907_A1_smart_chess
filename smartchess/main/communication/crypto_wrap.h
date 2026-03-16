#pragma once
#include <cstdint>
#include <cstddef>

bool ascon_encrypt_bytes(const uint8_t *pt, size_t pt_len, uint16_t seq,
                         uint8_t nonce_out[16], uint8_t tag_out[16],
                         uint8_t *ct_out, size_t ct_max, size_t *ct_len_out);

bool ascon_decrypt_bytes(const uint8_t *ct, size_t ct_len,
                         const uint8_t nonce[16], const uint8_t tag[16],
                         uint8_t *pt_out, size_t pt_max,
                         size_t *pt_len_out, size_t original_pt_len);