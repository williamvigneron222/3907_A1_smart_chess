#include "driver/i2s.h"
#include "driver/adc.h"
#include "ASCON-AEAD128.h"

// ====== USER SETTINGS ======
static const i2s_port_t I2S_PORT = I2S_NUM_0;

// IMPORTANT: use ADC1 (ADC2 conflicts with Wi-Fi)
static const adc1_channel_t ADC_CH = ADC1_CHANNEL_6;  // ADC1_CH6 = GPIO34 (common)

// Sample rate for audio capture
static const uint32_t SAMPLE_RATE = 32000;

// Encryption input block size in BYTES
static const size_t BLOCK_BYTES = 512; // e.g., 512 bytes per block (multiple of 16)

// How many ADC samples to read per i2s_read() call (tuneable)
static const size_t READ_SAMPLES = 256; // 256 * 2 bytes = 512 bytes read from I2S

static const size_t BLOCK_WORDS = BLOCK_BYTES / 8;


static const i2s_port_t I2S_SPK = I2S_NUM_1;

static const int SPK_BLCK = 26;
static const int SPK_LRC = 25;
static const int SPK_OUT = 22;

static void setup_i2s_speaker() {
    i2s_config_t spk_cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_cfg = {
        .bck_io_num = SPK_BLCK,
        .ws_io_num = SPK_LRC,
        .data_out_num = SPK_OUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    if (i2s_driver_install(I2S_SPK, &spk_cfg, 0, NULL) != ESP_OK) {
        Serial.println("ERROR: i2s_driver_install speaker failed");
        while (true) {}
    }

    if (i2s_set_pin(I2S_SPK, &pin_cfg) != ESP_OK) {
        Serial.println("ERROR: i2s_set_pin speaker failed");
        while (true) {}
    }

    i2s_zero_dma_buffer(I2S_SPK);
}

void setup() {
  Serial.begin(921600);
  delay(200);

  setup_i2s_adc();
  setup_i2s_speaker();
}

void loop() {


  for (size_t i = 0; i < count; i++) {
        stereo_buf[2 * i]     = samples[i];
        stereo_buf[2 * i + 1] = samples[i];
      }

      size_t bytes_written = 0;
      i2s_write(
        I2S_PORT,
        stereo_buf,
        blk.count * 2 * sizeof(int16_t),
        &bytes_written,
        portMAX_DELAY
      );
}
