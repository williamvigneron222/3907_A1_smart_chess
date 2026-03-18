#include "Arduino.h"
#include "Preferences.h"
#include "esp_timer.h"

//#include "infared.h"
#include "communication.h"

extern "C" void app_main(void)
{
    

    //irSetup();
    communication_setup();
}