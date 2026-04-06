#ifndef INFRARED_H
#define INFRARED_H

#include <stdint.h>

/*
 ********************
 ***COMMAND CODES ***
 ********************
 */
/** 
* (1) denotes the device with a lower MAC address
* (2) denotes the device with the higher MAC address
*
* Command codes:
* 0001 | Initiate (append my MAC)
* 0010 | Acknowledge a 0001 (append other MAC)
* 0100 | (lower MAC sends its generated key)
* 1000 | End
* 
*/
#define CMD_INITIATE       0b0001   /// header | cmd code | my MAC address
#define CMD_INITIATE_ACK   0b0010   /// header | cmd code | my MAC address | other MAC address
#define CMD_KEY_SEND       0b0100   /// header | cmd code | key // TODO crc check here?
#define CMD_END            0b1000   /// header | cmd code | 0 (end) 1 (fail)

#define PACKET_HEADER      0x1A     /// identifier 

// typedef enum {
//     IDLE,                  /// not Looking for another device
//     INITIATE,              /// looking for another devce. send CMD_INITIATE every 2-3 seconds // TODO time
//     OTHER_DEVICE_RECEIVE,  /// recieved a CMD_INITIATE from another device. send the key back every 5 seconds
//                            /// until we recieve a wifi packet ACK. Then return to idle
// } state;


typedef enum {
    IDLE,
    INITIATE,
    INITIATE_ACK,
    KEY_SEND,
    KEY_ACK,
} state;

static state irState = IDLE;

void irSetup();

void irLoop();

#endif /// INFRARED_H