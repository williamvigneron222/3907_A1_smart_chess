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
    //if IR button is held
         // begin timer 2 seconds
     // if IR is held > 2 seconds:
         // irLoop();
     // else if microphone button is held
         // adc loop
 
     // else listen to incoming signals
 
     //
}

