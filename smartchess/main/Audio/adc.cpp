#include "driver/i2s.h"
#include "driver/adc.h"
#include "ASCON-AEAD128.h"

static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const adc1_channel_t ADC_CH = ADC1_CHANNEL_6;  // ADC1_CH6 = GPIO34 (common)

static const uint32_t SAMPLE_RATE = 48000;

static const size_t BLOCK_BYTES = 512; // 512 bytes per block (multiple of 16)

static const size_t READ_SAMPLES = 256; // 256 * 2 bytes = 512 bytes read from I2S

static const size_t BLOCK_WORDS = BLOCK_BYTES / 8;

static uint16_t raw[READ_SAMPLES];
static uint64_t  block[BLOCK_WORDS];
static size_t   block_index = 0;

static const i2s_port_t I2S_SPK = I2S_NUM_1;

static const int SPK_BLCK = 26;
static const int SPK_LRC = 25;
static const int SPK_OUT = 22;

// Convert 12-bit ADC sample (0..4095) -> signed-ish 16-bit PCM (-32768..32767-ish)
static inline int16_t adc12_to_pcm16(uint16_t adc_word) {
  uint16_t adc12 = (adc_word) & 0x0FFF;
  static int32_t dc = 1970;      
  int32_t centered = (int32_t)adc12 - dc;

  return (int16_t)(centered << 4);
}

static inline uint64_t pack_to_64(int16_t a, int16_t b, int16_t c, int16_t d) {
  return ((uint64_t)(uint16_t)a) | ((uint64_t)(uint16_t)b << 16) | ((uint64_t)(uint16_t)c << 32) | ((uint64_t)(uint16_t)d << 48);
}

static inline int16_t median3(int16_t a, int16_t b, int16_t c) {
  if (a > b) { int16_t t = a; a = b; b = t; }
  if (b > c) { int16_t t = b; b = c; c = t; }
  if (a > b) { int16_t t = a; a = b; b = t; }
  return b;
}

static void setup_i2s_adc() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC_CH, ADC_ATTEN_DB_11);

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
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

  i2s_set_adc_mode(ADC_UNIT_1, ADC_CH);
  i2s_adc_enable(I2S_PORT);
}

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
  size_t bytes_read = 0;
  esp_err_t err = i2s_read(I2S_PORT, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
  if (err != ESP_OK) return;

  size_t samples_read = bytes_read / sizeof(uint16_t);

  int16_t pack[4];
  int pack_count = 0;

  for (size_t i = 1; i < samples_read - 1; i++) {
    int16_t s0 = adc12_to_pcm16(raw[i-1]);
    int16_t s1 = adc12_to_pcm16(raw[i]);
    int16_t s2 = adc12_to_pcm16(raw[i+1]);

    int16_t filtered = median3(s0, s1, s2);

    pack[pack_count++] = filtered;
    if (pack_count == 4) {
      block[block_index++] = pack_to_64(pack[0], pack[1], pack[2], pack[3]);
      pack_count = 0; 
    }
  }
}

