#include "Arduino.h"
#include "esp32-rmt-ir.h"

#include "infared.h"

void irSetup()
{
    // reciever pin
    irRxPin = 34;
    // transmit pin
	irTxPin = 4;

    currentState = IDLE;
}

void irLoop()
{
    /**
     * Header identifier:
     * 1A1A
     * 
     * 
     * (1) (2) both transmit 1A1A 0001
     */
    switch(currentState)
    {
        case IDLE:
            currentState = INITIATE;
            break;
        case INITIATE:

        default:
            break;
    }
}