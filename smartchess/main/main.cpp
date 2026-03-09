#include "Arduino.h"
#include "Preferences.h"
#include "esp32-rmt-ir.h"


// #include "adc.h"
#include "infared.h"

void setup()
{
    // Serial.begin(115200); /// IFDEF DEBUG  ?
    irSetup();
}


void loop()
{
    //if IR button is held
        irLoop();
}

