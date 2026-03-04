#include "Arduino.h"
#include "esp32-rmt-ir.h"



void irSetup()
{
    // reciever pin
    irRxPin = 34;
    // transmit pin
	irTxPin = 4;
}

void irLoop()
{
    
}