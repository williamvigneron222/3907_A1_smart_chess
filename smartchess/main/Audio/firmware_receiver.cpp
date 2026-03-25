#include "Arduino.h"
#include "driver/i2s.h"
#include "communication.h"

// ====== PIN SETTINGS ======
static const int BTN_PIN = 33;

// ====== I2S SPEAKER ======
static const i2s_port_t I2S_SPK_PORT = I2S_NUM_1;
static const int        SPK_BCLK     = 26;
static const int        SPK_LRC      = 25;
static const int        SPK_DOUT     = 22;
static const uint32_t   SAMPLE_RATE  = 8000;
static const size_t     SAMPLES      = 80;

// ====== BUFFER ======
static int16_t spk_stereo[SAMPLES * 2];  // stereo expansion

// ====== SPEAKER SETUP ======
void setup_i2s_speaker()
{
    i2s_config_t spk_cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_cfg = {
        .bck_io_num   = SPK_BCLK,
        .ws_io_num    = SPK_LRC,
        .data_out_num = SPK_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    if (i2s_driver_install(I2S_SPK_PORT, &spk_cfg, 0, NULL) != ESP_OK) {
        Serial.println("ERROR: speaker i2s_driver_install failed");
        while (true) {}
    }

    if (i2s_set_pin(I2S_SPK_PORT, &pin_cfg) != ESP_OK) {
        Serial.println("ERROR: speaker i2s_set_pin failed");
        while (true) {}
    }

    i2s_zero_dma_buffer(I2S_SPK_PORT);
    Serial.println("Speaker I2S ready");
}

// ====== AUDIO RX CALLBACK — called by communication layer after decrypt ======
// This gets registered in communication_setup()
void on_audio_received(const int16_t *samples, size_t num_samples)
{
    // Only play audio when PTT is NOT held (not transmitting)
    if (digitalRead(BTN_PIN) == LOW) return;

    for (size_t i = 0; i < num_samples; i++) {
        spk_stereo[2 * i]     = samples[i];
        spk_stereo[2 * i + 1] = samples[i];
    }

    size_t bytes_written = 0;
    i2s_write(I2S_SPK_PORT,
              spk_stereo,
              num_samples * 2 * sizeof(int16_t),
              &bytes_written,
              portMAX_DELAY);
}