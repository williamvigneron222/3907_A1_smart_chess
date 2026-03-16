#include "Arduino.h"
#include "Preferences.h"
#include "esp_timer.h" // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html


// #include "adc.h"
#include "infared.h"

void setup()
{
    // Serial.begin(115200); /// IFDEF DEBUG  ?
    irSetup();
}


void loop()
{
    uint64_t h[4] = { 0 };
    uint64_t m[2] = { 0, 1 }; // mlen 2
    hash(m, 2, h);
    //if IR button is held
        irLoop();
}

