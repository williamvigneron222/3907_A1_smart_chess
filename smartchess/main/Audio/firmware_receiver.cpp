#include "driver/i2s.h"
#include "driver/adc.h"
#include "ASCON-AEAD128.h"
#include "communication.h"
#include "Arduino.h"

// ====== USER SETTINGS ======
static const uint32_t SAMPLE_RATE = 32000;

static const i2s_port_t I2S_SPK = I2S_NUM_1;

static const int SPK_BCLK = 26;
static const int SPK_LRC  = 25;
static const int SPK_DOUT = 22;

// Match your packet size / callback chunk size
static const size_t MAX_RX_SAMPLES = 80;

// Stereo buffer for MAX98357A output
static int16_t stereo_buf[MAX_RX_SAMPLES * 2];

static inline int16_t adc12_to_pcm16(uint16_t adc_word) {
  uint16_t adc12 = adc_word & 0x0FFF;

  static int32_t dc = 2048;
  dc = (63 * dc + adc12) / 64;   // slow DC tracking

  int32_t centered = (int32_t)adc12 - dc;
  return (int16_t)(centered << 5);
}

static void setup_i2s_speaker() {
  i2s_config_t spk_cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 80,
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


// This gets called automatically when a valid packet is received + decrypted
void on_audio_received(const int16_t *samples, size_t count)
{
  if (!samples || count == 0) return;

  if (count > MAX_RX_SAMPLES) {
    count = MAX_RX_SAMPLES;
  }

  // Duplicate mono samples into L and R for the MAX98357A
  for (size_t i = 0; i < count; i++) {
    int16_t val = adc12_to_pcm16((uint16_t)samples[i]);
    stereo_buf[2 * i]     = val;
    stereo_buf[2 * i + 1] = val;
  }

  for (size_t i = 0; i < 15; i++)
  {
    Serial.println(stereo_buf[i]);
  }

  size_t bytes_written = 0;
  i2s_write(
    I2S_SPK,
    stereo_buf,
    count * 2 * sizeof(int16_t),
    &bytes_written,
    portMAX_DELAY
  );
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setup_i2s_speaker();

  // Register callback with communication layer
  communication_setup(on_audio_received);

  Serial.println("Receiver ready");
}

void loop() {
  // ESP-NOW RX happens through callback
  communication_loop();
  vTaskDelay(1);
  
}