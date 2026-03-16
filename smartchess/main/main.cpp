#include "Arduino.h"
#include "Preferences.h"
#include "esp_timer.h" // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html


// #include "adc.h"
extern "C"
{
#include "esp32-rmt-ir.h"
#include "ASCON-HASH256.h"
}


void setup()
{
    Serial.begin(115200);
    // irSetup();
}


void loop()
{
    uint64_t h[4] = { 0 };
    uint64_t m[2] = { 0, 1 }; // mlen 2
    hash(m, 2, h);
    //if IR button is held
        // begin timer 2 seconds
    // if IR is held > 2 seconds:
        // irLoop();
    // else if microphone button is held
        // adc loop

    // else listen to incoming signals

    //
}

