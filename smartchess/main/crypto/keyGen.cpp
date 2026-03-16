#include "keyGen.h"

extern "C" {
    #include "ASCON-HASH256.h"
}
#include "ASCON-HASH256.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static constexpr gpio_num_t BITSTREAM_GPIO = GPIO_NUM_19;

static constexpr int KEY_BYTES = 16;
static constexpr int POOL_BYTES = 32;

static constexpr int BASE_SAMPLE_DELAY_US = 40;
static constexpr int EXTRA_JITTER_US_MASK = 0x0F;

static bool gpio_ready = false;

static inline void sample_delay_us()
{
    int extra = esp_timer_get_time() & EXTRA_JITTER_US_MASK;
    ets_delay_us(BASE_SAMPLE_DELAY_US + extra);
}

static inline int read_raw_bit()
{
    return gpio_get_level(BITSTREAM_GPIO) & 1;
}

static int get_debiased_bit()
{
    while (true)
    {
        int b1 = read_raw_bit();
        sample_delay_us();

        int b2 = read_raw_bit();
        sample_delay_us();

        if (b1 == 0 && b2 == 1) return 0;
        if (b1 == 1 && b2 == 0) return 1;

        taskYIELD();
    }
}

void keygen_init()
{
    if (gpio_ready) return;

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BITSTREAM_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    gpio_ready = true;
}

void keygen_generate(uint8_t key_out[16])
{
    keygen_init();

    uint8_t pool[POOL_BYTES];
    memset(pool, 0, sizeof(pool));

    for (int i = 0; i < POOL_BYTES * 8; i++)
    {
        int bit = get_debiased_bit();
        int byte = i / 8;
        pool[byte] = (pool[byte] << 1) | bit;
    }

    uint64_t words[4] = {0};

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            words[i] = (words[i] << 8) | pool[i*8 + j];
        }
    }

    uint64_t hash_out[4];

    hash(words, 4, hash_out);

    memcpy(key_out, hash_out, KEY_BYTES);
}