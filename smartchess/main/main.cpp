#include "Arduino.h"
#include "Preferences.h"
#include "esp_timer.h"

//#include "infared.h"
#include "communication.h"

void setup()
{
    //irSetup();
    communication_setup();
}

void loop()
{
    //irLoop();
    communication_loop();
    delay(10);
}