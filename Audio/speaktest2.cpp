#include "driver/i2s.h"
#include "driver/adc.h"
#include "short.h"

static const int BCLK = 26;
static const int LRC  = 25;
static const int DOUT = 22;

void setup() {

  Serial.begin(115200);

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_pin_config_t pins = {
    .bck_io_num = BCLK,
    .ws_io_num = LRC,
    .data_out_num = DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);

  i2s_zero_dma_buffer(I2S_NUM_0);

  Serial.println("Playing audio...");

  size_t bytes_written;

  i2s_write(
    I2S_NUM_0,
    short_wav + 44,
    short_wav_len - 44,
    &bytes_written,
    portMAX_DELAY
  );
}

void loop() {}