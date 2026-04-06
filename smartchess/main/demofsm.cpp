#include "Arduino.h"
#include "IRremote.hpp"
#include "Adafruit_GFX.h"
#include "Adafruit_ST7735.h"

#include "keyGen.h"
#include "clock.h"

// ============================================================
// PIN ASSIGNMENTS
// ============================================================

// ---------- LCD (software SPI) ----------
static const int TFT_CS   = 5;
static const int TFT_DC   = 32;
static const int TFT_RST  = 23;
static const int TFT_SCLK = 14;
static const int TFT_MOSI = 13;

// ---------- Menu buttons (active LOW) ----------
static const int BTN_CYCLE  = 21;
static const int BTN_SELECT = 27;

// ---------- HRNG ----------
static const int HRNG_CLOCK_PIN = 18;   // used by your clock.cpp
static const int HRNG_BIT_PIN   = 19;   // used by your keyGen.cpp

// ---------- IR ----------
static const int IR_SEND_PIN    = 4;
static const int IR_RECEIVE_PIN = 15;   // safer than GPIO35 for demo

// ============================================================
// LCD
// ============================================================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// ============================================================
// UI STATES
// ============================================================
enum UiPage {
  PAGE_MENU,
  PAGE_PAIRING
};

enum PairState {
  PAIR_IDLE,
  PAIR_WAITING,
  PAIR_GENERATING,
  PAIR_SENDING,
  PAIR_WAIT_ACK,
  PAIR_RECEIVING,
  PAIR_SUCCESS,
  PAIR_FAILED
};

static UiPage currentPage = PAGE_MENU;
static PairState pairState = PAIR_IDLE;

// ============================================================
// MENU
// ============================================================
static const char *menuItems[] = {
  "Pairing"
};
static const int menuCount = 1;
static int selectedMenuIndex = 0;

// ============================================================
// BUTTON EDGE TRACKING
// ============================================================
static int lastCycleState  = HIGH;
static int lastSelectState = HIGH;

// ============================================================
// KEY / PAIRING DATA
// ============================================================
static uint8_t localKey[16]    = {0};
static uint8_t receivedKey[16] = {0};
static bool receivedChunk[8]   = {false};

static bool keyValid      = false;
static bool pairInitiator = false;

static int pairRetryCount = 0;
static unsigned long lastPairSendMs = 0;
static unsigned long lastLcdUpdateMs = 0;

static const int PAIR_MAX_RETRIES = 10;
static const unsigned long PAIR_RESEND_MS = 1500;

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

  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 90);
  tft.print("SELECT = Enter");
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
    tft.setCursor(10, 28);
    tft.print("SELECT = Gen/Send");

    tft.setCursor(10, 43);
    tft.print("CYCLE = Back");

    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(10, 65);
    tft.print("Status:");

    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 85);
    tft.print(status);
  } else {
    tft.setCursor(10, 35);
    tft.print("Pairing successful");

    tft.setCursor(10, 55);
    tft.print(status);

    tft.setCursor(10, 85);
    tft.print("CYCLE = Back");
  }
}

static void drawShortKeyLine(const uint8_t *key) {
  tft.fillRect(0, 104, 128, 24, ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(4, 108);

  // Show first 4 bytes only so it fits
  for (int i = 0; i < 4; i++) {
    if (key[i] < 16) tft.print("0");
    tft.print(key[i], HEX);
  }
  tft.print("...");
}

// ============================================================
// HARDWARE KEY GENERATION
// ============================================================
static void generate_hardware_key() {
  drawPairingPage("Generating key...", false);
  Serial.println("Initializing HRNG...");
  Serial.print("Clock pin expected by hardware: GPIO ");
  Serial.println(HRNG_CLOCK_PIN);
  Serial.print("Bit pin expected by hardware: GPIO ");
  Serial.println(HRNG_BIT_PIN);

  // Assumes your existing clock.cpp / keyGen.cpp already use GPIO18/GPIO19
  clock_init_default();
  keygen_init();
  keygen_generate(localKey);

  keyValid = true;
  print_bytes("Generated key: ", localKey, 16);
  drawShortKeyLine(localKey);
}

// ============================================================
// IR HELPERS
// ============================================================
static void sendPairStartPacket() {
  IrSender.sendNEC(IR_START_ADDR, IR_START_CMD, 0);
  Serial.println("Sent START");
}

static void sendPairAckPacket() {
  IrSender.sendNEC(IR_ACK_ADDR, IR_ACK_CMD, 0);
  Serial.println("Sent ACK");
}

static void sendKeyChunk(uint8_t chunk) {
  uint8_t byteIndex = chunk * 2;
  uint16_t addr = ((uint16_t)localKey[byteIndex] << 8) | localKey[byteIndex + 1];

  IrSender.sendNEC(addr, chunk, 0);

  Serial.print("Sent chunk ");
  Serial.print(chunk);
  Serial.print(" -> ");
  if (localKey[byteIndex] < 16) Serial.print("0");
  Serial.print(localKey[byteIndex], HEX);
  if (localKey[byteIndex + 1] < 16) Serial.print("0");
  Serial.println(localKey[byteIndex + 1], HEX);
}

static void sendEntireKeyBurst() {
  // START a few times so the other board definitely locks in
  for (int i = 0; i < 3; i++) {
    sendPairStartPacket();
    delay(150);
  }

  delay(400);

  // Send all 8 chunks
  for (uint8_t i = 0; i < 8; i++) {
    sendKeyChunk(i);
    delay(150);
  }
}

// ============================================================
// PAIRING FLOW
// ============================================================
static void enterPairingPage() {
  currentPage = PAGE_PAIRING;
  pairState = PAIR_WAITING;
  pairInitiator = false;
  pairRetryCount = 0;
  lastPairSendMs = 0;
  keyValid = false;
  clearReceivedKeyState();

  drawPairingPage("Waiting / ready", false);
  Serial.println("Entered pairing page. Waiting for START or SELECT.");
}

static void finishPairSuccessAsReceiver() {
  memcpy(localKey, receivedKey, sizeof(localKey));
  keyValid = true;
  pairState = PAIR_SUCCESS;

  Serial.println("Receiver pairing success.");
  print_bytes("Paired key: ", localKey, 16);

  drawPairingPage("RX paired", true);
  drawShortKeyLine(localKey);
}

static void finishPairSuccessAsInitiator() {
  pairState = PAIR_SUCCESS;

  Serial.println("Initiator pairing success.");
  print_bytes("Paired key: ", localKey, 16);

  drawPairingPage("TX paired", true);
  drawShortKeyLine(localKey);
}

static void startPairingAsInitiator() {
  pairInitiator = true;
  pairRetryCount = 0;
  lastPairSendMs = 0;
  pairState = PAIR_GENERATING;

  Serial.println("Starting pairing as initiator...");

  generate_hardware_key();
  drawPairingPage("Key generated", false);
  delay(250);

  pairState = PAIR_SENDING;
  drawPairingPage("Sending key...", false);
  drawShortKeyLine(localKey);
}

static void handleIncomingIrPacket() {
  if (!IrReceiver.decode()) return;

  if (IrReceiver.decodedIRData.protocol == NEC) {
    uint16_t address = IrReceiver.decodedIRData.address;
    uint8_t command  = IrReceiver.decodedIRData.command;

    if (address == IR_START_ADDR && command == IR_START_CMD && !pairInitiator) {
      clearReceivedKeyState();
      pairState = PAIR_RECEIVING;
      Serial.println("START received");
    }
    else if (address == IR_ACK_ADDR && command == IR_ACK_CMD &&
             pairInitiator && pairState == PAIR_WAIT_ACK) {
      Serial.println("ACK received");
      finishPairSuccessAsInitiator();
    }
    else if (!pairInitiator && pairState == PAIR_RECEIVING && command < 8) {
      uint8_t chunk = command;
      uint8_t byteIndex = chunk * 2;

      receivedKey[byteIndex] = (uint8_t)((address >> 8) & 0xFF);
      receivedKey[byteIndex + 1] = (uint8_t)(address & 0xFF);
      receivedChunk[chunk] = true;

      Serial.print("Chunk ");
      Serial.println(chunk);

      if (allKeyChunksReceived()) {
        Serial.println("All chunks received");
        print_bytes("Received key: ", receivedKey, 16);

        for (int i = 0; i < 3; i++) {
          sendPairAckPacket();
          delay(150);
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

    drawPairingPage("Waiting for ACK...", false);
    drawShortKeyLine(localKey);
    Serial.println("Key sent. Waiting for ACK...");
  }

  if (pairInitiator && pairState == PAIR_WAIT_ACK) {
    if (millis() - lastPairSendMs >= PAIR_RESEND_MS) {
      if (pairRetryCount >= PAIR_MAX_RETRIES) {
        pairState = PAIR_FAILED;
        drawPairingPage("Pairing failed", false);
        Serial.println("Pairing failed: max retries reached.");
      } else {
        pairRetryCount++;
        drawPairingPage("Resending key...", false);
        drawShortKeyLine(localKey);

        Serial.print("Resending burst, attempt ");
        Serial.println(pairRetryCount);

        sendEntireKeyBurst();
        lastPairSendMs = millis();

        drawPairingPage("Waiting for ACK...", false);
        drawShortKeyLine(localKey);
      }
    }
  }

  // Refresh passive waiting screen every so often so it stays obvious
  if (!pairInitiator && pairState == PAIR_WAITING) {
    if (millis() - lastLcdUpdateMs > 2000) {
      drawPairingPage("Waiting / ready", false);
      lastLcdUpdateMs = millis();
    }
  }
}

// ============================================================
// UI HANDLERS
// ============================================================
static void handleMenuState(bool cycleEdge, bool selectEdge) {
  (void)cycleEdge;

  if (selectEdge) {
    enterPairingPage();
  }
}

static void handlePairingState(bool cycleEdge, bool selectEdge) {
  if (cycleEdge) {
    currentPage = PAGE_MENU;
    pairState = PAIR_IDLE;
    pairInitiator = false;
    drawMenu();
    Serial.println("Returned to menu.");
    return;
  }

  if (selectEdge) {
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
  delay(700);
  Serial.println();
  Serial.println("ESP32 IR Pairing Demo");

  pinMode(BTN_CYCLE, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  // IR
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);

  // LCD
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(1);
  drawMenu();

  Serial.print("IR send pin: GPIO ");
  Serial.println(IR_SEND_PIN);
  Serial.print("IR receive pin: GPIO ");
  Serial.println(IR_RECEIVE_PIN);
  Serial.println("Press SELECT to enter pairing.");
}

void loop() {
  bool cycleEdge  = cyclePressedEdge();
  bool selectEdge = selectPressedEdge();

  switch (currentPage) {
    case PAGE_MENU:
      handleMenuState(cycleEdge, selectEdge);
      break;

    case PAGE_PAIRING:
      handlePairingState(cycleEdge, selectEdge);
      break;
  }

  delay(20);
}