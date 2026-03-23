#pragma once
#include <cstdint>
#include <cstddef>

// Encrypts int16_t audio samples into payload bytes
bool ascon_encrypt_audio(const int16_t *samples, size_t num_samples,
                         uint16_t seq,
                         uint64_t nonce_out[2], uint64_t tag_out[2],
                         uint8_t *ct_out, size_t ct_max_bytes,
                         size_t *ct_bytes_out);

// Decrypts payload bytes back into int16_t audio samples
bool ascon_decrypt_audio(const uint8_t *ct, size_t ct_bytes,
                         const uint64_t nonce[2], const uint64_t tag[2],
                         int16_t *samples_out, size_t max_samples,
                         size_t *samples_out_count, size_t original_samples);