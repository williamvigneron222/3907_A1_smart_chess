#include "Arduino.h"
#include "Preferences.h"
#include "esp_timer.h" // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html
#include "keyGen.h"
#include "clock.h"


// #include "adc.h"
//#include "infrared.h"

//128-bit key (16 bytes)
static uint8_t key[16] = {0};

//128-bit nonce (16 bytes), starts at every reboot for now
static uint8_t nonce[16] = {0};

//helper function to print bytes in hex form (easy to debug)
static void print_bytes(const char *label, const uint8_t *data, int len)
{
    Serial.print(label);

    for (int i = 0; i < len; i++)
    {
        // Add a leading 0 if the byte is less than 0x10
        if (data[i] < 16)
        {
            Serial.print("0");
        }

        Serial.print(data[i], HEX);
    }

    Serial.println();
}

//Helper function to count the 128-bit nonce up by 1
static void increment_nonce()
{
    for (int i = 15; i >= 0; i--)
    {
        nonce[i]++;

        // If this byte did not overflow back to 0, stop here
        if (nonce[i] != 0)
        {
            break;
        }
    }
}


void setup()
{
    Serial.begin(115200);
    // irSetup();
    delay(1000);

    // Clock output for HRNG hardware
    // Drives the DFF
    clock_init_default();

    // Setting up the GPIO19 as the random bitstream input
    keygen_init();

    // Generating the key
    keygen_generate(key);

    // Setting the nonce to all zeros, resets every boot
    for (int i = 0; i < 16; i++)
    {
        nonce[i] = 0;
    }

    Serial.println("System initialized.");
    print_bytes("Generated key:  ", key, 16);
    print_bytes("Starting nonce: ", nonce, 16);
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
    delay(100);
}

//the code generates the key once and the starting nonce once


