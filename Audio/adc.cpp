#include <arduino.h>
#include "driver/i2s.h"
#include "driver/adc.h"

// ====== USER SETTINGS ======
static const i2s_port_t I2S_PORT = I2S_NUM_0;

// IMPORTANT: use ADC1 (ADC2 conflicts with Wi-Fi)
static const adc1_channel_t ADC_CH = ADC1_CHANNEL_6;  // ADC1_CH6 = GPIO34 (common)

// Sample rate for audio capture
static const uint32_t SAMPLE_RATE = 16000;

// Encryption input block size in BYTES
static const size_t BLOCK_BYTES = 512; // e.g., 512 bytes per block (multiple of 16)

// How many ADC samples to read per i2s_read() call (tuneable)
static const size_t READ_SAMPLES = 256; // 256 * 2 bytes = 512 bytes read from I2S

// ====== BUFFERS ======
static uint16_t raw[READ_SAMPLES];      // ADC data comes as 16-bit words (12-bit valid)
static uint8_t  block[BLOCK_BYTES];     // what you pass to encryption
static size_t   block_index = 0;        // current fill position in block[]

// ====== YOUR ENCRYPTION HOOK ======
// Replace this with your existing Ascon pipeline call.
// Must accept BLOCK_BYTES bytes (or whatever you choose).


// Convert 12-bit ADC sample (0..4095) -> signed-ish 16-bit PCM (-32768..32767-ish)
static inline int16_t adc12_to_pcm16(uint16_t adc_word) {
  uint16_t adc12 = adc_word & 0x0FFF;     // ensures first 4 bits are 0
  int32_t centered = (int32_t)adc12 - 2048; // center around 0
  return (int16_t)(centered << 4);        // scale 12-bit -> 16-bit range
}

static void setup_i2s_adc() {
  // 1) Configure ADC
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC_CH, ADC_ATTEN_DB_11);

  // 2) Configure I2S to read from built-in ADC
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("ERROR: i2s_driver_install failed");
    while (true) {}
  }

  // 3) Tell I2S which ADC channel to use
  i2s_set_adc_mode(ADC_UNIT_1, ADC_CH);

  // 4) Start ADC sampling into I2S DMA
  i2s_adc_enable(I2S_PORT);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Starting ADC->I2S audio capture...");

  setup_i2s_adc();

  Serial.println("Capture running. Blocks will be produced continuously.");
}

void loop() {
  // Read a chunk of ADC samples from DMA
  size_t bytes_read = 0;
  esp_err_t err = i2s_read(I2S_PORT, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
  if (err != ESP_OK) return;

  size_t samples_read = bytes_read / sizeof(uint16_t);

  // Pack into encryption-sized blocks
  for (size_t i = 0; i < samples_read; i++) {
    int16_t pcm = adc12_to_pcm16(raw[i]);

    // Pack int16 into 2 bytes (little-endian)
    block[block_index++] = (uint8_t)(pcm & 0xFF);
    block[block_index++] = (uint8_t)((pcm >> 8) & 0xFF);

    // If block filled, hand it to encryption
    if (block_index >= BLOCK_BYTES) {
      encrypt_block(block, BLOCK_BYTES);
      block_index = 0;
    }
  }
}

