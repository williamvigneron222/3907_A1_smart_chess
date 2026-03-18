#include "Arduino.h"
// #include "esp32-rmt-ir.h"
#include "esp_timer.h"
#include "IRemote.h"

#include "infrared.h"

#include "keyGen.h" // getkey()

void irSetup()
{
    /// PIN SETUP
    // reciever pin
    irRxPin = 34;
    // transmit pin
	irTxPin = 4;

    irState = IDLE;
}

void irLoop()
{
    // esp_timer_get_time() /// int64_t
    //  esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period)



    // update timer
    // read rmtIR

    switch(currentState)
    {
        case IDLE:
            irState = INITIATE;
            break;
        case INITIATE:
            // every 2-3 seconds broadcast, otherwise detect
            // if we detect a CMD_INITIATE set state to INITIATE_ACK
            // if we detect a CMD_INITIATE_ACK set state to KEY_SEND
            
            break;
        case INITIATE_ACK:
            // every 2-3 seconds broadcast an ack
            // if we detect a key send, set state to key ack
            break;
        case KEY_SEND:
            // every 2-3 seconds broadcast, otherwise detect WiFi test packet
            // if we detect a test packet and can decrypt change to KEY_ACK
                // can't decrypt - CMD_END 1
            break;
        case KEY_ACK:
            // broadcast CMD_END until we get CMD_END back ?
            break;
        default:
            break;
    }
}




// #include "Arduino.h"
// #include "esp32_rmt_ir.h"

// #define IR_RX_PIN 15

// RMT_IR irReceiver(IR_RX_PIN);

// void setup() {
//     Serial.begin(115200);
//     delay(1000);

//     Serial.println("ESP32 IR Receiver Starting...");

//     if (!irReceiver.begin()) {
//         Serial.println("Failed to initialize IR receiver!");
//         while (true);
//     }

//     Serial.println("Waiting for IR signal...");
// }

// void loop() {
//     if (irReceiver.available()) {
//         IRData data = irReceiver.read();

//         Serial.println("IR Signal Received:");
//         Serial.print("Protocol: ");
//         Serial.println(data.protocol);

//         Serial.print("Address: 0x");
//         Serial.println(data.address, HEX);

//         Serial.print("Command: 0x");
//         Serial.println(data.command, HEX);

//         Serial.print("Raw Data: 0x");
//         Serial.println(data.value, HEX);

//         Serial.println("--------------------------");
//     }
// }