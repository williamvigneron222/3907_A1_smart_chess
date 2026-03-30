#include "driver/i2s.h"
#include "driver/adc.h"

// ================= SETTINGS =================
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const adc1_channel_t ADC_CH = ADC1_CHANNEL_6;   // GPIO34

static const uint32_t SAMPLE_RATE = 32000;
static const size_t RECORD_SECONDS = 1;
static const size_t READ_SAMPLES = 256;

static const int SPK_BCLK = 26;
static const int SPK_LRC  = 25;
static const int SPK_DOUT = 22;

// ================= BUFFERS =================
static const size_t RECORD_SAMPLES = SAMPLE_RATE * RECORD_SECONDS;

static uint16_t raw[READ_SAMPLES];
static int16_t* record_buf = nullptr;
static int16_t stereo_buf[READ_SAMPLES * 2];

// ================= HELPERS =================
static inline int16_t adc12_to_pcm16(uint16_t adc_word) {
  uint16_t adc12 = adc_word & 0x0FFF;

  static int32_t dc = 2048;
  dc = (255 * dc + adc12) / 256;

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
  Serial.begin(921600);
  delay(500);

  record_buf = (int16_t*)malloc(RECORD_SAMPLES * sizeof(int16_t));
  if (!record_buf) {
    Serial.println("Failed to allocate record_buf");
    while (true) {}
  }

  Serial.println("Starting ADC...");
  setup_i2s_adc();

  size_t record_index = 0;

  Serial.println("Recording...");

  while (record_index < RECORD_SAMPLES) {
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_PORT, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
    if (err != ESP_OK) {
      Serial.println("ADC read failed");
      return;
    }

    size_t samples_read = bytes_read / sizeof(uint16_t);

    for (size_t i = 0; i < samples_read && record_index < RECORD_SAMPLES; i++) {
      record_buf[record_index++] = adc12_to_pcm16(raw[i]);
    }
  }

  Serial.printf("Recorded samples: %u\n", (unsigned)record_index);
  Serial.printf("Expected time: %.3f s\n", (float)record_index / SAMPLE_RATE);

  Serial.println("First 20 samples:");
  for (int i = 0; i < 20 && i < (int)record_index; i++) {
    Serial.println(record_buf[i]);
  }

  Serial.println("Stopping ADC...");
  stop_i2s_adc();
  delay(200);

  Serial.println("Starting speaker...");
  setup_i2s_speaker();
  delay(200);

  Serial.println("Looping playback...");
}

void loop() {
  size_t play_index = 0;

  while (play_index < RECORD_SAMPLES) {
    size_t chunk = READ_SAMPLES;
    if (play_index + chunk > RECORD_SAMPLES) {
      chunk = RECORD_SAMPLES - play_index;
    }

    for (size_t i = 0; i < chunk; i++) {
      int16_t s = record_buf[play_index + i];
      stereo_buf[2 * i]     = s;
      stereo_buf[2 * i + 1] = s;
    }

    size_t bytes_written = 0;
    esp_err_t err = i2s_write(
      I2S_PORT,
      stereo_buf,
      chunk * 2 * sizeof(int16_t),
      &bytes_written,
      portMAX_DELAY
    );

    if (err != ESP_OK) {
      Serial.printf("Playback failed: %d\n", err);
      delay(500);
      return;
    }

    play_index += chunk;
  }
}