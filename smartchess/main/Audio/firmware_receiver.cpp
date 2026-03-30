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

static const size_t MAX_RX_SAMPLES = 80;

// Stereo buffer for MAX98357A output
static int16_t stereo_buf[MAX_RX_SAMPLES * 2];

// Audio queue — safe buffering from rx_task context
static int16_t       audio_queue[MAX_RX_SAMPLES];
static size_t        audio_queue_count = 0;
static volatile bool audio_ready       = false;

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

// Called from rx_task — just buffer, no i2s_write here
void on_audio_received(const int16_t *samples, size_t count)
{
  if (!samples || count == 0) return;
  if (count > MAX_RX_SAMPLES) count = MAX_RX_SAMPLES;

  // Use samples directly — already converted PCM from sender
  // ← fixed: removed double adc12_to_pcm16 conversion
  memcpy(audio_queue, samples, count * sizeof(int16_t));
  audio_queue_count = count;
  audio_ready       = true;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setup_i2s_speaker();
  communication_setup(on_audio_received);

  Serial.println("Receiver ready");
}

void loop() {
  communication_loop();  // just vTaskDelay(1) now

  if (audio_ready) {
    audio_ready = false;

    for (size_t i = 0; i < audio_queue_count; i++) {
      stereo_buf[2 * i]     = audio_queue[i];   // ← fixed: no conversion
      stereo_buf[2 * i + 1] = audio_queue[i];
    }

    size_t bytes_written = 0;
    i2s_write(
      I2S_SPK,
      stereo_buf,
      audio_queue_count * 2 * sizeof(int16_t),
      &bytes_written,
      portMAX_DELAY
    );
  }
}
