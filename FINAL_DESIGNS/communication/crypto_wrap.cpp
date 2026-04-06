#include "crypto_wrap.h"
#include "ASCON-AEAD128.h"
#include <cstring>

#define MAX_WORDS 64

// Hardcoded key for integration testing
// Replace with getKey() once IR pairing is integrated
static const uint64_t KEY[2] = {
    0x0011223344556677ULL,
    0x8899AABBCCDDEEFFULL
};

static void make_nonce(uint16_t seq, uint64_t nonce_out[2])
{
    nonce_out[0] = static_cast<uint64_t>(seq);
    nonce_out[1] = 0xA500000000000000ULL;
}

bool ascon_encrypt_audio(const int16_t *samples, size_t num_samples,
                         uint16_t seq,
                         uint64_t nonce_out[2], uint64_t tag_out[2],
                         uint8_t *ct_out, size_t ct_max_bytes,
                         size_t *ct_bytes_out)
{
    if (!samples || !ct_out || !ct_bytes_out || num_samples == 0) return false;

    size_t input_bytes  = num_samples * sizeof(int16_t);
    size_t padded_bytes = (input_bytes + 15) & ~((size_t)15);
    if (padded_bytes > ct_max_bytes) return false;

    size_t words = padded_bytes / 8;
    if (words > MAX_WORDS) return false;

    uint64_t p_words[MAX_WORDS + 2] = {};
    uint64_t c_words[MAX_WORDS + 2] = {};
    std::memcpy(p_words, samples, input_bytes);

    make_nonce(seq, nonce_out);
    tag_out[0] = 0;
    tag_out[1] = 0;

    ascon_encrypt(const_cast<uint64_t*>(KEY), nonce_out, nullptr, 0,
                  p_words, (unsigned)words, c_words, tag_out);

    std::memcpy(ct_out, c_words, padded_bytes);
    *ct_bytes_out = padded_bytes;
    return true;
}

bool ascon_decrypt_audio(const uint8_t *ct, size_t ct_bytes,
                         const uint64_t nonce[2], const uint64_t tag[2],
                         int16_t *samples_out, size_t max_samples,
                         size_t *samples_out_count, size_t original_samples)
{
    if (!ct || !nonce || !tag || !samples_out || !samples_out_count) return false;
    if (ct_bytes == 0 || (ct_bytes % 16) != 0)                       return false;
    if (original_samples > max_samples)                               return false;

    size_t words = ct_bytes / 8;
    if (words > MAX_WORDS) return false;

    uint64_t c_words[MAX_WORDS + 2] = {};
    uint64_t p_words[MAX_WORDS + 2] = {};
    std::memcpy(c_words, ct, ct_bytes);

    uint64_t tag_computed[2] = {0, 0};
    ascon_decrypt(const_cast<uint64_t*>(KEY),
                  const_cast<uint64_t*>(nonce),
                  nullptr, 0,
                  p_words, (unsigned)words, c_words, tag_computed);

    if (tag_computed[0] != tag[0] || tag_computed[1] != tag[1]) return false;

    std::memcpy(samples_out, p_words, original_samples * sizeof(int16_t));
    *samples_out_count = original_samples;
    return true;
}