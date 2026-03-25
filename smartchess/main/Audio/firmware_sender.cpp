#include "driver/i2s.h"
#include "driver/adc.h"
#include "ASCON-AEAD128.h"
#include "communication.h"

// ====== USER SETTINGS ======
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const adc1_channel_t ADC_CH = ADC1_CHANNEL_6;  // GPIO34
static const uint32_t SAMPLE_RATE = 32000;
static const size_t READ_SAMPLES = 256;

// ====== BUFFERS ======
static uint16_t raw[READ_SAMPLES];

// Convert 12-bit ADC sample -> signed 16-bit PCM
static inline int16_t adc12_to_pcm16(uint16_t adc_word) {
  uint16_t adc12 = adc_word & 0x0FFF;

  static int32_t dc = 2048;
  dc = (63 * dc + adc12) / 64;   // slow DC tracking

  int32_t centered = (int32_t)adc12 - dc;
  return (int16_t)(centered << 4);
}

// Dummy RX callback for now
void on_audio_received(const int16_t *samples, size_t num_samples)
{
  // Do nothing for now
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

void setup() {
  Serial.begin(921600);
  delay(200);

  setup_i2s_adc();
  communication_setup(on_audio_received);
}

void loop() {
  size_t bytes_read = 0;
  esp_err_t err = i2s_read(I2S_PORT, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
  if (err != ESP_OK) return;

  size_t samples_read = bytes_read / sizeof(uint16_t);

  static int16_t tx_buf[READ_SAMPLES];
  size_t tx_count = 0;

  for (size_t i = 0; i < samples_read; i++) {
    tx_buf[tx_count++] = adc12_to_pcm16(raw[i]);
  }

  if (tx_count > 0) {
    communication_send(tx_buf, tx_count);
  }
}