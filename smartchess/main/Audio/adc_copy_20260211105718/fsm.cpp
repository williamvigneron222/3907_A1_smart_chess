#include "driver/i2s.h"
#include "driver/adc.h"

// ============================================================
// SETTINGS
// ============================================================
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const adc1_channel_t ADC_CH = ADC1_CHANNEL_6;   // GPIO34

static const uint32_t SAMPLE_RATE = 16000;
static const size_t   AUDIO_SAMPLES = 256;             // 16 ms at 16 kHz
static const size_t   AUDIO_QUEUE_LEN = 6;

static const int SPK_BCLK = 26;
static const int SPK_LRC  = 25;
static const int SPK_DOUT = 22;

static const int PTT_BUTTON = 33;   // active LOW

// ============================================================
// AUDIO BLOCK
// ============================================================
struct AudioBlock {
  int16_t samples[AUDIO_SAMPLES];
  size_t count;
};

// ============================================================
// GLOBALS
// ============================================================
enum AudioMode {
  MODE_RX,
  MODE_TX,
  MODE_PAIR
};

static volatile AudioMode currentMode = MODE_RX;

static QueueHandle_t audioQueue = nullptr;

static TaskHandle_t adcTaskHandle      = nullptr;
static TaskHandle_t txTaskHandle       = nullptr;
static TaskHandle_t rxTaskHandle       = nullptr;
static TaskHandle_t speakerTaskHandle  = nullptr;

// raw ADC DMA input
static uint16_t raw[AUDIO_SAMPLES];

// ============================================================
// HELPERS
// ============================================================
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

// ============================================================
// I2S ADC
// ============================================================
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

// ============================================================
// I2S SPEAKER
// ============================================================
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

static void stop_i2s_speaker() {
  i2s_zero_dma_buffer(I2S_PORT);
  i2s_driver_uninstall(I2S_PORT);
}

// ============================================================
// PLACEHOLDER RADIO / CRYPTO HOOKS
// Replace these with your real functions
// ============================================================
static void send_audio_block(const AudioBlock &blk) {
    //encrypt and send the audio block
}

static bool receive_audio_block(AudioBlock &blk) {
  //decrypt incoming block

  vTaskDelay(pdMS_TO_TICKS(16)); // roughly one block period
  return true;
}

// ============================================================
// TASKS: TX MODE
// ============================================================
static void adc_capture_task(void *pvParameters) {
  AudioBlock blk;

  while (currentMode == MODE_TX) {
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_PORT, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
    if (err != ESP_OK) {
      continue;
    }

    size_t samples_read = bytes_read / sizeof(uint16_t);
    if (samples_read > AUDIO_SAMPLES) samples_read = AUDIO_SAMPLES;

    blk.count = samples_read;
    for (size_t i = 0; i < samples_read; i++) {
      blk.samples[i] = adc12_to_pcm16(raw[i]);
    }

    // If queue is full, drop oldest to keep latency bounded
    if (uxQueueSpacesAvailable(audioQueue) == 0) {
      AudioBlock dump;
      xQueueReceive(audioQueue, &dump, 0);
    }

    xQueueSend(audioQueue, &blk, portMAX_DELAY);
  }

  adcTaskHandle = nullptr;
  vTaskDelete(NULL);
}

static void tx_send_task(void *pvParameters) {
  AudioBlock blk;

  while (currentMode == MODE_TX) {
    if (xQueueReceive(audioQueue, &blk, pdMS_TO_TICKS(50)) == pdTRUE) {
      send_audio_block(blk);
    }
  }

  txTaskHandle = nullptr;
  vTaskDelete(NULL);
}

// ============================================================
// TASKS: RX MODE
// ============================================================
static void rx_receive_task(void *pvParameters) {
  AudioBlock blk;

  while (currentMode == MODE_RX) {
    if (receive_audio_block(blk)) {
      if (uxQueueSpacesAvailable(audioQueue) == 0) {
        AudioBlock dump;
        xQueueReceive(audioQueue, &dump, 0);
      }

      xQueueSend(audioQueue, &blk, portMAX_DELAY);
    }
  }

  rxTaskHandle = nullptr;
  vTaskDelete(NULL);
}

static void speaker_play_task(void *pvParameters) {
  AudioBlock blk;
  static int16_t stereo_buf[AUDIO_SAMPLES * 2];

  while (currentMode == MODE_RX) {
    if (xQueueReceive(audioQueue, &blk, pdMS_TO_TICKS(50)) == pdTRUE) {
      for (size_t i = 0; i < blk.count; i++) {
        stereo_buf[2 * i]     = blk.samples[i];
        stereo_buf[2 * i + 1] = blk.samples[i];
      }

      size_t bytes_written = 0;
      i2s_write(
        I2S_PORT,
        stereo_buf,
        blk.count * 2 * sizeof(int16_t),
        &bytes_written,
        portMAX_DELAY
      );
    } else {
      // underflow: play silence
      memset(stereo_buf, 0, sizeof(stereo_buf));
      size_t bytes_written = 0;
      i2s_write(I2S_PORT, stereo_buf, sizeof(stereo_buf), &bytes_written, portMAX_DELAY);
    }
  }

  speakerTaskHandle = nullptr;
  vTaskDelete(NULL);
}

// ============================================================
// MODE CONTROL
// ============================================================
static void clear_audio_queue() {
  if (!audioQueue) return;

  AudioBlock dump;
  while (xQueueReceive(audioQueue, &dump, 0) == pdTRUE) {}
}

static void stop_tx_mode() {
  currentMode = MODE_RX; // break TX task loops

  vTaskDelay(pdMS_TO_TICKS(30));

  stop_i2s_adc();
  clear_audio_queue();
}

static void stop_rx_mode() {
  currentMode = MODE_TX; // break RX task loops

  vTaskDelay(pdMS_TO_TICKS(30));

  stop_i2s_speaker();
  clear_audio_queue();
}

static void start_tx_mode() {
  Serial.println("Switching to TX mode");

  setup_i2s_adc();
  clear_audio_queue();
  currentMode = MODE_TX;

  xTaskCreatePinnedToCore(adc_capture_task, "adc_capture_task", 4096, NULL, 2, &adcTaskHandle, 0);
  xTaskCreatePinnedToCore(tx_send_task,     "tx_send_task",     4096, NULL, 2, &txTaskHandle, 1);
}

static void start_rx_mode() {
  Serial.println("Switching to RX mode");

  setup_i2s_speaker();
  clear_audio_queue();
  currentMode = MODE_RX;

  xTaskCreatePinnedToCore(rx_receive_task,  "rx_receive_task",  4096, NULL, 2, &rxTaskHandle, 0);
  xTaskCreatePinnedToCore(speaker_play_task,"speaker_play_task",4096, NULL, 2, &speakerTaskHandle, 1);
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PTT_BUTTON, INPUT_PULLUP);

  audioQueue = xQueueCreate(AUDIO_QUEUE_LEN, sizeof(AudioBlock));
  if (!audioQueue) {
    Serial.println("Failed to create audio queue");
    while (true) {}
  }

  // default = RX mode
  start_rx_mode();
}

void loop() {
  static AudioMode desiredMode = MODE_RX;

  bool ptt_pressed = (digitalRead(PTT_BUTTON) == LOW);
  desiredMode = ptt_pressed ? MODE_TX : MODE_RX;

  static AudioMode lastMode = MODE_RX;

  if (desiredMode != lastMode) {
    if (lastMode == MODE_RX && desiredMode == MODE_TX) {
      stop_rx_mode();
      delay(50);
      start_tx_mode();
    } else if (lastMode == MODE_TX && desiredMode == MODE_RX) {
      stop_tx_mode();
      delay(50);
      start_rx_mode();
    }

    lastMode = desiredMode;
  }

  delay(20);
}