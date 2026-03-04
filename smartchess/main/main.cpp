// #include "Arduino.h"
// #include "Preferences.h"
// #include "esp32-rmt-ir.h"


// #include "adc.h"

// void setup()
// {
//     // Serial.begin(115200); /// IFDEF DEBUG  ?
//     // pinMode(GPIO, INPUT);
// }


// void loop()
// {
//     // int bit = digitalRead(GPIO);
//     // Serial.println();

// }

#include <Arduino.h>
#include <esp32_rmt_ir.h>

#define IR_RX_PIN 15

RMT_IR irReceiver(IR_RX_PIN);

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("ESP32 IR Receiver Starting...");

    if (!irReceiver.begin()) {
        Serial.println("Failed to initialize IR receiver!");
        while (true);
    }

    Serial.println("Waiting for IR signal...");
}

void loop() {
    if (irReceiver.available()) {
        IRData data = irReceiver.read();

        Serial.println("IR Signal Received:");
        Serial.print("Protocol: ");
        Serial.println(data.protocol);

        Serial.print("Address: 0x");
        Serial.println(data.address, HEX);

        Serial.print("Command: 0x");
        Serial.println(data.command, HEX);

        Serial.print("Raw Data: 0x");
        Serial.println(data.value, HEX);

        Serial.println("--------------------------");
    }
}