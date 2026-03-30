#include "Arduino.h"
#include "IRremote.hpp"
#include "Adafruit_GFX.h"
#include "Adafruit_ST7735.h"

#include "driver/i2s.h"
#include "driver/adc.h"

#include "keyGen.h"
#include "clock.h"

// ============================================================
// PIN ASSIGNMENTS
// ============================================================

// ---------- LCD (software SPI to avoid conflicts) ----------
static const int TFT_CS   = 5;
static const int TFT_DC   = 32;
static const int TFT_RST  = 23;
static const int TFT_SCLK = 14;
static const int TFT_MOSI = 13;
// LED pin on display -> tie directly to 3.3V

// ---------- Menu buttons (active LOW) ----------
static const int BTN_CYCLE  = 21;   // menu next / back
static const int BTN_SELECT = 27;   // select / hold-to-talk / generate

// ---------- IR ----------
static const int IR_SEND_PIN    = 4;
static const int IR_RECEIVE_PIN = 35; // input only is fine for receiver

// ---------- Audio / existing pins ----------
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const adc1_channel_t ADC_CH = ADC1_CHANNEL_6;   // GPIO34

static const uint32_t SAMPLE_RATE = 16000;
static const size_t   AUDIO_SAMPLES = 256;
static const size_t   AUDIO_QUEUE_LEN = 6;

static const int SPK_BCLK = 26;
static const int SPK_LRC  = 25;
static const int SPK_DOUT = 22;

// ============================================================
// LCD
// ============================================================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ============================================================
// AUDIO BLOCK
// ============================================================
struct AudioBlock {
  int16_t samples[AUDIO_SAMPLES];
  size_t count;
};

// ============================================================
// UI + SYSTEM STATES
// ============================================================
enum UiPage {
  PAGE_MENU,
  PAGE_ACTIVE,
  PAGE_PAIRING
};

enum AudioMode {
  MODE_IDLE,
  MODE_RX,
  MODE_TX,
  MODE_PAIR
};

enum PairState {
  PAIR_WAITING,       // on pairing page, waiting for start or user action
  PAIR_GENERATING,    // generating local key
  PAIR_SENDING,       // sending start + chunks
  PAIR_WAIT_ACK,      // initiator waiting for ack
  PAIR_RECEIVING,     // receiver collecting chunks
  PAIR_SUCCESS,
  PAIR_FAILED
};

static UiPage currentPage = PAGE_MENU;
static volatile AudioMode currentMode = MODE_IDLE;
static PairState pairState = PAIR_WAITING;

// ============================================================
// MENU
// ============================================================
static const char *menuItems[] = {
  "Active",
  "Pairing"
};
static const int menuCount = 2;
static int selectedMenuIndex = 0;

// ============================================================
// AUDIO GLOBALS
// ============================================================
static QueueHandle_t audioQueue = nullptr;

static TaskHandle_t adcTaskHandle      = nullptr;
static TaskHandle_t txTaskHandle       = nullptr;
static TaskHandle_t rxTaskHandle       = nullptr;
static TaskHandle_t speakerTaskHandle  = nullptr;

static uint16_t raw[AUDIO_SAMPLES];

// ============================================================
// BUTTON EDGE TRACKING
// ============================================================
static int lastCycleState  = HIGH;
static int lastSelectState = HIGH;

// Prevent immediate TX when entering Active page while still
// holding select from the menu.
static bool activeSelectReleased = false;

// ============================================================
// KEY / PAIRING DATA
// ============================================================

// 128-bit key
static uint8_t localKey[16]    = {0};
static uint8_t receivedKey[16] = {0};
static bool receivedChunk[8]   = {false};
static bool keyValid           = false;
static bool pairInitiator      = false;

static int pairRetryCount = 0;
static unsigned long lastPairSendMs = 0;

static const int PAIR_MAX_RETRIES = 10;
static const unsigned long PAIR_RESEND_MS = 1200;

// NEC markers
static const uint16_t IR_START_ADDR = 0xABCD;
static const uint8_t  IR_START_CMD  = 0xF0;

static const uint16_t IR_ACK_ADDR   = 0xDCBA;
static const uint8_t  IR_ACK_CMD    = 0xF1;

// ============================================================
// HELPERS
// ============================================================
static void print_bytes(const char *label, const uint8_t *data, int len) {
  Serial.print(label);
  for (int i = 0; i < len; i++) {
    if (data[i] < 16) Serial.print("0");
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

static bool cyclePressedEdge() {
  int current = digitalRead(BTN_CYCLE);
  bool pressed = (lastCycleState == HIGH && current == LOW);
  lastCycleState = current;
  return pressed;
}

static bool selectPressedEdge() {
  int current = digitalRead(BTN_SELECT);
  bool pressed = (lastSelectState == HIGH && current == LOW);
  lastSelectState = current;
  return pressed;
}

static bool selectHeldRaw() {
  return digitalRead(BTN_SELECT) == LOW;
}

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

static void clearReceivedKeyState() {
  memset(receivedKey, 0, sizeof(receivedKey));
  for (int i = 0; i < 8; i++) {
    receivedChunk[i] = false;
  }
}

static bool allKeyChunksReceived() {
  for (int i = 0; i < 8; i++) {
    if (!receivedChunk[i]) return false;
  }
  return true;
}

// ============================================================
// LCD DRAWING
// ============================================================
static void drawMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(8, 8);
  tft.print("Main Menu");

  for (int i = 0; i < menuCount; i++) {
    int y = 32 + i * 20;

    if (i == selectedMenuIndex) {
      tft.fillRect(6, y - 2, 116, 12, ST77XX_WHITE);
      tft.setTextColor(ST77XX_BLACK);
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }

    tft.setCursor(10, y);
    tft.print(menuItems[i]);
  }
}

static void drawActivePage(bool transmitting) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(1);

  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 10);
  tft.print("Active Mode");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 30);
  tft.print("Hold SELECT = Talk");

  tft.setCursor(10, 45);
  tft.print("CYCLE = Back");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 70);
  tft.print("Status:");

  if (transmitting) {
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 90);
    tft.print("Transmitting");
  } else {
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(10, 90);
    tft.print("Listening");
  }
}

static void drawPairingPage(const char *status, bool success) {
  if (success) {
    tft.fillScreen(ST77XX_GREEN);
    tft.setTextColor(ST77XX_BLACK);
  } else {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_GREEN);
  }

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("Pairing Mode");

  if (!success) {
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 30);
    tft.print("SELECT = Gen/Send");

    tft.setCursor(10, 45);
    tft.print("CYCLE = Back");

    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(10, 70);
    tft.print("Status:");
    tft.setTextColor(ST77XX_WHITE);
  } else {
    tft.setCursor(10, 30);
    tft.print("Pairing successful");
  }

  tft.setCursor(10, 90);
  tft.print(status);
}

// ============================================================
// AUDIO PLACEHOLDERS
// Replace these with your real encrypted radio transport.
// ============================================================
static void send_audio_block(const AudioBlock &blk) {
  (void)blk;
  // TODO: encrypt and send the audio block
}

static bool receive_audio_block(AudioBlock &blk) {
  (void)blk;
  // TODO: receive and decrypt incoming block
  vTaskDelay(pdMS_TO_TICKS(16));
  return true;
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
// AUDIO TASKS
// ============================================================
static void adc_capture_task(void *pvParameters) {
  AudioBlock blk;

  while (currentMode == MODE_TX) {
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_PORT, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
    if (err != ESP_OK) continue;

    size_t samples_read = bytes_read / sizeof(uint16_t);
    if (samples_read > AUDIO_SAMPLES) samples_read = AUDIO_SAMPLES;

    blk.count = samples_read;
    for (size_t i = 0; i < samples_read; i++) {
      blk.samples[i] = adc12_to_pcm16(raw[i]);
    }

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
      i2s_write(I2S_PORT, stereo_buf, blk.count * 2 * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    } else {
      memset(stereo_buf, 0, sizeof(stereo_buf));
      size_t bytes_written = 0;
      i2s_write(I2S_PORT, stereo_buf, sizeof(stereo_buf), &bytes_written, portMAX_DELAY);
    }
  }

  speakerTaskHandle = nullptr;
  vTaskDelete(NULL);
}

// ============================================================
// AUDIO MODE CONTROL
// ============================================================
static void clear_audio_queue() {
  if (!audioQueue) return;
  AudioBlock dump;
  while (xQueueReceive(audioQueue, &dump, 0) == pdTRUE) {}
}

static void stop_audio_mode() {
  AudioMode prev = currentMode;
  if (prev == MODE_IDLE) return;

  currentMode = MODE_IDLE;
  vTaskDelay(pdMS_TO_TICKS(30));

  if (prev == MODE_TX) {
    stop_i2s_adc();
  } else if (prev == MODE_RX) {
    stop_i2s_speaker();
  }

  clear_audio_queue();
}

static void start_tx_mode() {
  if (currentMode == MODE_TX) return;

  stop_audio_mode();
  setup_i2s_adc();
  clear_audio_queue();
  currentMode = MODE_TX;

  xTaskCreatePinnedToCore(adc_capture_task, "adc_capture_task", 4096, NULL, 2, &adcTaskHandle, 0);
  xTaskCreatePinnedToCore(tx_send_task,     "tx_send_task",     4096, NULL, 2, &txTaskHandle, 1);
}

static void start_rx_mode() {
  if (currentMode == MODE_RX) return;

  stop_audio_mode();
  setup_i2s_speaker();
  clear_audio_queue();
  currentMode = MODE_RX;

  xTaskCreatePinnedToCore(rx_receive_task,   "rx_receive_task",   4096, NULL, 2, &rxTaskHandle, 0);
  xTaskCreatePinnedToCore(speaker_play_task, "speaker_play_task", 4096, NULL, 2, &speakerTaskHandle, 1);
}

// ============================================================
// HARDWARE KEY GENERATION
// ============================================================
static void generate_hardware_key() {
  drawPairingPage("Generating key...", false);

  clock_init_default();
  keygen_init();
  keygen_generate(localKey);

  keyValid = true;
  print_bytes("Generated key: ", localKey, 16);
}

// ============================================================
// IR PAIRING HELPERS (NEC)
// 8 chunks of 2 bytes each = 16-byte key
// ============================================================
static void sendPairStartPacket() {
  IrSender.sendNEC(IR_START_ADDR, IR_START_CMD, 0);
}

static void sendPairAckPacket() {
  IrSender.sendNEC(IR_ACK_ADDR, IR_ACK_CMD, 0);
}

static void sendKeyChunk(uint8_t chunk) {
  uint8_t byteIndex = chunk * 2;
  uint16_t addr = ((uint16_t)localKey[byteIndex] << 8) | localKey[byteIndex + 1];
  IrSender.sendNEC(addr, chunk, 0);
}

static void sendEntireKeyBurst() {
  sendPairStartPacket();
  delay(80);

  for (uint8_t i = 0; i < 8; i++) {
    sendKeyChunk(i);
    delay(80);
  }
}

static void startPairingAsInitiator() {
  stop_audio_mode();
  currentMode = MODE_PAIR;

  clearReceivedKeyState();
  pairInitiator = true;
  pairRetryCount = 0;
  lastPairSendMs = 0;
  pairState = PAIR_GENERATING;

  generate_hardware_key();
  drawPairingPage("Key generated", false);

  delay(250);

  pairState = PAIR_SENDING;
  drawPairingPage("Sending key...", false);
}

static void enterPairingPage() {
  stop_audio_mode();
  currentMode = MODE_PAIR;

  pairState = PAIR_WAITING;
  pairInitiator = false;
  pairRetryCount = 0;
  keyValid = false;
  clearReceivedKeyState();

  drawPairingPage("Waiting / ready", false);
}

static void finishPairSuccessAsReceiver() {
  memcpy(localKey, receivedKey, sizeof(localKey));
  keyValid = true;
  pairState = PAIR_SUCCESS;
  drawPairingPage("Pairing successful", true);
  print_bytes("Paired key: ", localKey, 16);
}

static void finishPairSuccessAsInitiator() {
  pairState = PAIR_SUCCESS;
  drawPairingPage("Pairing successful", true);
  print_bytes("Paired key: ", localKey, 16);
}

static void handleIncomingIrPacket() {
  if (!IrReceiver.decode()) return;

  if (IrReceiver.decodedIRData.protocol == NEC) {
    uint16_t address = IrReceiver.decodedIRData.address;
    uint8_t command  = IrReceiver.decodedIRData.command;

    // Receiver side: saw start packet
    if (address == IR_START_ADDR && command == IR_START_CMD && !pairInitiator) {
      clearReceivedKeyState();
      pairState = PAIR_RECEIVING;
      drawPairingPage("Receiving key...", false);
    }
    // Initiator side: got ack back
    else if (address == IR_ACK_ADDR && command == IR_ACK_CMD && pairInitiator && pairState == PAIR_WAIT_ACK) {
      finishPairSuccessAsInitiator();
    }
    // Receiver side: store chunks
    else if (!pairInitiator && pairState == PAIR_RECEIVING && command < 8) {
      uint8_t chunk = command;
      uint8_t byteIndex = chunk * 2;

      receivedKey[byteIndex]     = (uint8_t)((address >> 8) & 0xFF);
      receivedKey[byteIndex + 1] = (uint8_t)(address & 0xFF);
      receivedChunk[chunk] = true;

      if (allKeyChunksReceived()) {
        drawPairingPage("Key received, ack...", false);

        // Send ACK a few times for reliability
        for (int i = 0; i < 3; i++) {
          sendPairAckPacket();
          delay(80);
        }

        finishPairSuccessAsReceiver();
      }
    }
  }

  IrReceiver.resume();
}

static void servicePairingState() {
  handleIncomingIrPacket();

  if (pairInitiator && pairState == PAIR_SENDING) {
    sendEntireKeyBurst();
    pairState = PAIR_WAIT_ACK;
    lastPairSendMs = millis();
    pairRetryCount = 1;
    drawPairingPage("Waiting for ack...", false);
  }

  if (pairInitiator && pairState == PAIR_WAIT_ACK) {
    if (millis() - lastPairSendMs >= PAIR_RESEND_MS) {
      if (pairRetryCount >= PAIR_MAX_RETRIES) {
        pairState = PAIR_FAILED;
        drawPairingPage("Pairing failed", false);
      } else {
        drawPairingPage("Resending key...", false);
        sendEntireKeyBurst();
        lastPairSendMs = millis();
        pairRetryCount++;
        drawPairingPage("Waiting for ack...", false);
      }
    }
  }
}

// ============================================================
// UI STATE HANDLERS
// ============================================================
static void handleMenuState(bool cycleEdge, bool selectEdge) {
  if (currentMode != MODE_IDLE) {
    stop_audio_mode();
  }

  if (cycleEdge) {
    selectedMenuIndex++;
    if (selectedMenuIndex >= menuCount) selectedMenuIndex = 0;
    drawMenu();
  }

  if (selectEdge) {
    if (selectedMenuIndex == 0) {
      currentPage = PAGE_ACTIVE;
      activeSelectReleased = false;
      start_rx_mode();
      drawActivePage(false);
    } else if (selectedMenuIndex == 1) {
      currentPage = PAGE_PAIRING;
      enterPairingPage();
    }
  }
}

static void handleActiveState(bool cycleEdge, bool rawSelectLow) {
  if (cycleEdge) {
    stop_audio_mode();
    currentPage = PAGE_MENU;
    drawMenu();
    return;
  }

  if (!activeSelectReleased) {
    if (!rawSelectLow) {
      activeSelectReleased = true;
    }
    return;
  }

  if (rawSelectLow) {
    if (currentMode != MODE_TX) {
      start_tx_mode();
      drawActivePage(true);
    }
  } else {
    if (currentMode != MODE_RX) {
      start_rx_mode();
      drawActivePage(false);
    }
  }
}

static void handlePairingState(bool cycleEdge, bool selectEdge) {
  if (cycleEdge) {
    stop_audio_mode();
    currentPage = PAGE_MENU;
    drawMenu();
    return;
  }

  if (selectEdge) {
    // Only initiate if not already actively pairing
    if (pairState == PAIR_WAITING || pairState == PAIR_FAILED || pairState == PAIR_SUCCESS) {
      startPairingAsInitiator();
    }
  }

  servicePairingState();
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BTN_CYCLE, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  audioQueue = xQueueCreate(AUDIO_QUEUE_LEN, sizeof(AudioBlock));
  if (!audioQueue) {
    Serial.println("Failed to create audio queue");
    while (true) {}
  }

  // IR
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);

  // LCD
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(1);

  drawMenu();
}

void loop() {
  bool cycleEdge  = cyclePressedEdge();
  bool selectEdge = selectPressedEdge();
  bool rawSelect  = selectHeldRaw();

  switch (currentPage) {
    case PAGE_MENU:
      handleMenuState(cycleEdge, selectEdge);
      break;

    case PAGE_ACTIVE:
      handleActiveState(cycleEdge, rawSelect);
      break;

    case PAGE_PAIRING:
      handlePairingState(cycleEdge, selectEdge);
      break;
  }

  delay(20);
}