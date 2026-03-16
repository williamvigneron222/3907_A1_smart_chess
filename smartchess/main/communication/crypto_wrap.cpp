#include "crypto_wrap.h"
#include <cstring>
//static uint8_t g_nonce_seed[8];
//static bool g_nonce_seed_init = false;
#include "ASCON-AEAD128.h"

#define MAX_WORDS 64
/*static void init_nonce_seed()
{
    if (!g_nonce_seed_init) {
        esp_fill_random(g_nonce_seed, sizeof(g_nonce_seed));
        g_nonce_seed_init = true;
    }
}
    */

static uint64_t KEY[2] = {
    0x0011223344556677ULL,
    0x8899AABBCCDDEEFFULL
};

static uint64_t load64_le(const uint8_t *b)
{
    uint64_t x = 0;
    for (int i = 7; i >= 0; i--) {
        x = (x << 8) | b[i];
    }
    return x;
}

static void store64_le(uint8_t *b, uint64_t x)
{
    for (int i = 0; i < 8; i++) {
        b[i] = (uint8_t)(x & 0xFF);
        x >>= 8;
    }
}

static void make_nonce(uint16_t seq, uint8_t nonce_out[16])
{
    std::memset(nonce_out, 0, 16);
    nonce_out[0] = (uint8_t)(seq & 0xFF);
    nonce_out[1] = (uint8_t)(seq >> 8);
    nonce_out[15] = 0xA5;
}
/*replace above function with this 
static void make_nonce(uint16_t seq, uint8_t nonce_out[16])
{
    init_nonce_seed();

    std::memset(nonce_out, 0, 16);

    // first 8 bytes = random startup seed
    std::memcpy(&nonce_out[0], g_nonce_seed, 8);

    // next 2 bytes = sequence number
    nonce_out[8]  = static_cast<uint8_t>(seq & 0xFF);
    nonce_out[9]  = static_cast<uint8_t>((seq >> 8) & 0xFF);

    // remaining bytes can stay 0 for now
}

*/

bool ascon_encrypt_bytes(const uint8_t *pt, size_t pt_len, uint16_t seq,
                         uint8_t nonce_out[16], uint8_t tag_out[16],
                         uint8_t *ct_out, size_t ct_max, size_t *ct_len_out)
{
    if (!pt || !ct_out || !ct_len_out || pt_len == 0) return false;

    size_t padded = (pt_len + 15) & ~((size_t)15);
    if (padded > ct_max) return false;

    size_t words = padded / 8;
    if (words > MAX_WORDS) return false;

    uint64_t p_words[MAX_WORDS + 2];
    uint64_t c_words[MAX_WORDS + 2];
    std::memset(p_words, 0, sizeof(p_words));
    std::memset(c_words, 0, sizeof(c_words));

    for (size_t i = 0; i < pt_len; i++) {
        ((uint8_t*)p_words)[i] = pt[i];
    }

    make_nonce(seq, nonce_out);
    uint64_t NONCE[2] = {
        load64_le(&nonce_out[0]),
        load64_le(&nonce_out[8])
    };

    uint64_t TAG[2] = {0, 0};

    ascon_encrypt(KEY, NONCE, nullptr, 0, p_words, (unsigned)words, c_words, TAG);

    std::memcpy(ct_out, (uint8_t*)c_words, padded);
    *ct_len_out = padded;

    store64_le(&tag_out[0], TAG[0]);
    store64_le(&tag_out[8], TAG[1]);

    return true;
}

bool ascon_decrypt_bytes(const uint8_t *ct, size_t ct_len,
                         const uint8_t nonce[16], const uint8_t tag[16],
                         uint8_t *pt_out, size_t pt_max,
                         size_t *pt_len_out, size_t original_pt_len)
{
    if (!ct || !nonce || !tag || !pt_out || !pt_len_out) return false;
    if (ct_len == 0 || (ct_len % 16) != 0) return false;
    if (original_pt_len > pt_max) return false;

    size_t words = ct_len / 8;
    if (words > MAX_WORDS) return false;

    uint64_t c_words[MAX_WORDS + 2];
    uint64_t p_words[MAX_WORDS + 2];
    std::memset(c_words, 0, sizeof(c_words));
    std::memset(p_words, 0, sizeof(p_words));

    std::memcpy((uint8_t*)c_words, ct, ct_len);

    uint64_t NONCE[2] = {
        load64_le(&nonce[0]),
        load64_le(&nonce[8])
    };

    uint64_t TAG_IN[2] = {
        load64_le(&tag[0]),
        load64_le(&tag[8])
    };

    uint64_t TAG_OUT[2] = {0, 0};

    ascon_decrypt(KEY, NONCE, nullptr, 0, p_words, (unsigned)words, c_words, TAG_OUT);

    if (TAG_OUT[0] != TAG_IN[0] || TAG_OUT[1] != TAG_IN[1]) {
        return false;
    }

    std::memcpy(pt_out, (uint8_t*)p_words, original_pt_len);
    *pt_len_out = original_pt_len;

    return true;
}