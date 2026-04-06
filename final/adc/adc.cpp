#include "driver/i2s.h"
#include "driver/adc.h"

static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const i2s_port_t I2S_SPK = I2S_NUM_1;
static const adc1_channel_t ADC_CH = ADC1_CHANNEL_6;   // GPIO34

static const uint32_t SAMPLE_RATE = 32000;
static const size_t READ_SAMPLES = 80;

static const int SPK_BCLK = 26;
static const int SPK_LRC  = 25;
static const int SPK_DOUT = 22;

static uint16_t raw[READ_SAMPLES];


// ================= HELPERS =================
static inline int16_t adc12_to_pcm16(uint16_t adc_word) {
  uint16_t adc12 = adc_word & 0x0FFF;

  static int32_t dc = 2048;

  int32_t centered = (int32_t)adc12 - dc;
  int32_t pcm32 = centered << 5;

  if (pcm32 > 32767) pcm32 = 32767;
  if (pcm32 < -32768) pcm32 = -32768;

  return (int16_t)pcm32;
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
    Serial.println("ADC install failed");
    while (true) {}
  }

  i2s_set_adc_mode(ADC_UNIT_1, ADC_CH);
  i2s_adc_enable(I2S_PORT);
}

static void stop_i2s_adc() {
  i2s_adc_disable(I2S_PORT);
  i2s_driver_uninstall(I2S_PORT);
}

static void setup_i2s_speaker() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_cfg = {
    .bck_io_num = SPK_BCLK,
    .ws_io_num = SPK_LRC,
    .data_out_num = SPK_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  if (i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) {
    Serial.println("Speaker install failed");
    while (true) {}
  }

  if (i2s_set_pin(I2S_PORT, &pin_cfg) != ESP_OK) {
    Serial.println("Speaker pin failed");
    while (true) {}
  }

  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {

}

void loop() {

}