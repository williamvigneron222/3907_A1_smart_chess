#include "Arduino.h"
#include "esp32-rmt-ir.h"
#include "esp_timer.h"

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