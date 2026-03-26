// #include "Arduino.h"
// #include "IRemote.hpp"
#include <IRremote.hpp>
#define IR_RECEIVE_PIN 35
#define IR_SEND_PIN 4

void setup()
{
  Serial.begin(115200);
  // IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN);
}

void loop() {


  
  // uint16_t rawIRTimings[77] = {8330,1720, 880,5720, 2380,970, 4030,520, 530,620, 480,620, 530,620, 530,570, 580,520, 580,1620, 580,570, 530,920, 30,70, 1230,520, 580,1620, 580,1670, 580,1620, 580,1620, 530,620, 530,1670, 580,570, 580,520, 580,870, 1330,1620, 630,520, 580,520, 580,570, 530,570, 480,1720, 580,1620, 580,570, 580,520, 530,1020, 1230,1070, 30,120, 1030,1620, 580,1620, 580,7120, 8330};  // Protocol=UNKNOWN Hash=0x57D99F85 39 bits (incl. gap and start) received

  // a test static key. deos not include packet header &c
  uint16_t data[] = {
      0x1E3E,
      0xFB36,
      0x779B,
      0x1867,
      0x4274,
      0x35C3,
      0x82DC,
      0x5E53
  };

  
  IrSender.sendRaw(data, sizeof(data) / sizeof(data[0]), 38, 0, 0);


  // if (IrReceiver.decode()) {
  // if (IrReceiver.decodedIRData.decodedRawData != 0)
  //   {
  //     Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX); // Print "old" raw data
  //     IrReceiver.printIRResultShort(&Serial); // Print complete received data in one line
  //     IrReceiver.printIRSendUsage(&Serial);   // Print the statement required to send this data
  //   }

  //     IrReceiver.resume(); // Enable receiving of the next value

  // }
  
}