#include <Arduino.h>
#include <IRremote.hpp>

#define IR_SEND_PIN 4

void setup() {
    Serial.begin(115200);
    delay(1000);

    IrSender.begin(IR_SEND_PIN);

    Serial.println("ESP32 IR transmitter ready");
}

void loop() {
    Serial.println("Sending NEC packet...");

    // Send a simple NEC packet:
    // address = 0x10
    // command = 0x34
    // repeats = 0
    IrSender.sendNEC(0x10, 0x34, 0);

    delay(1000);
}