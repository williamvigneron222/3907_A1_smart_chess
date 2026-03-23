#include "keyGen.h"
#include "Preferences.h"
#include "esp_random.h"
#include "ASCON-HASH256.h"
#include "esp_log.h"

static const char *TAG = "KEYGEN";

void generate(uint64_t b[2])
{
    // --- Source U: raw hardware RNG (ESP32 built-in TRNG) ---
    uint64_t u[2];
    u[0] = ((uint64_t)esp_random() << 32) | esp_random();
    u[1] = ((uint64_t)esp_random() << 32) | esp_random();

    // --- Source V: second RNG reading, then hashed with ASCON-HASH256 ---
    uint64_t v_raw[2];
    v_raw[0] = ((uint64_t)esp_random() << 32) | esp_random();
    v_raw[1] = ((uint64_t)esp_random() << 32) | esp_random();

    // Hash V using ASCON-HASH256 — output is 4 x uint64_t (256 bits)
    // We only use the first 2 words (128 bits)
    uint64_t v_hashed[4] = { 0 };
    hash(v_raw, 2, v_hashed);
    uint64_t v[2] = { v_hashed[0], v_hashed[1] };

    // XOR both sources together for final key
    b[0] = u[0] ^ v[0];
    b[1] = u[1] ^ v[1];

    ESP_LOGI(TAG, "Key generated: %016llX %016llX", b[0], b[1]);
}

void newKey()
{
    uint64_t b[2] = { 0 };
    generate(b);

    Preferences prefs;
    prefs.begin(KEYNAME, false);    // false = read/write mode
    prefs.putULong64("key0", b[0]);
    prefs.putULong64("key1", b[1]);
    prefs.end();

    ESP_LOGI(TAG, "New key stored in flash");
}

void getKey(uint64_t b[2])
{
    Preferences prefs;
    prefs.begin(KEYNAME, true);     // true = read only mode
    b[0] = prefs.getULong64("key0", 0);
    b[1] = prefs.getULong64("key1", 0);
    prefs.end();

    // Warn if key was never set — both words will be 0
    if (b[0] == 0 && b[1] == 0) {
        ESP_LOGW(TAG, "WARNING: key is all zeros — newKey() may not have been called yet");
    } else {
        ESP_LOGI(TAG, "Key loaded: %016llX %016llX", b[0], b[1]);
    }
}
/*

## How it works
```
generate()
    U = esp_random() × 4 calls          → 128 bits of hardware RNG
    V = esp_random() × 4 calls          → 128 bits of hardware RNG
        → hashed with ASCON-HASH256     → adds diffusion
    key = U XOR V                        → final 128-bit key

newKey()    → calls generate() then stores key0 + key1 to flash

getKey()    → loads key0 + key1 from flash into b[0] and b[1]
*/