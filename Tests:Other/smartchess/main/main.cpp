#include "Arduino.h"
#include "Preferences.h"
#include "esp_system.h"
//#include "esp32-rmt-ir.h"
#include "esp_timer.h" // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html
#include "keyGen.h"
#include "clock.h"


// #include "adc.h"
//#include "infrared.h"

//128-bit key (16 bytes)
static uint8_t key[16] = {0};

//128-bit nonce (16 bytes), generates once every boot
static uint8_t nonce[16] = {0};

// External physical button on GPIO23
static const int BUTTON_PIN = 23;

// Tracks previous button state for edge detection
static int last_button_state = HIGH;

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
    Serial.begin(115200); /// IFDEF DEBUG  ?
    // irSetup();
    delay(1000);

    // Clock output for HRNG hardware
    // Drives the DFF
    clock_init_default();

    // Setting up the GPIO19 as the random bitstream input
    keygen_init();

    // Generating the key
    keygen_generate(key);

    // Generate a random starting nonce once at boot using ESP32's built-in RNG
    for (int i = 0; i < 16; i++)
    {
        nonce[i] = (uint8_t)(esp_random() & 0xFF);
    }

    Serial.println("System initialized.");
    print_bytes("Generated key:  ", key, 16);
    print_bytes("Starting nonce: ", nonce, 16);

    // Configure external button as input
    // Not pressed = HIGH, pressed = LOW
    pinMode(BUTTON_PIN, INPUT_PULLUP);
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

    // Read current BOOT button state
    int current_button_state = digitalRead(BUTTON_PIN);

    // Detect a new button press (HIGH -> LOW transition)
    if (last_button_state == HIGH && current_button_state == LOW)
    {
        Serial.println("Button pressed.");
        print_bytes("Current nonce: ", nonce, 16);

        increment_nonce();

        print_bytes("Updated nonce: ", nonce, 16);

        // Small debounce delay so one press does not trigger multiple times
        delay(200);
    }

    // Save current state for next loop iteration
    last_button_state = current_button_state;

    delay(20);
    
}

//the code generates the key once and the starting nonce once


