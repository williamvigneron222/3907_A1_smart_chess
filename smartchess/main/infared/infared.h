#ifndef INFARED_H
#define INFARED_H

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
* 1000 | (First a wifi packet will be sent to (1))
* 
*/
#define CMD_INITIATE       0b0001   /// header | cmd code | my MAC address
#define CMD_ACK_INITIATE   0b0010   /// header | cmd code | my MAC address | other MAC address
#define CMD_KEY_SEND       0b0100   /// header | cmd code | key // TODO crc check here?
#define CMD_KEY_ACK        0b1000   /// header | cmd code | [nothing]

#define PACKET_HEADER      0x1A1A  /// identifier 

typedef enum {
    IDLE,                  /// not Looking for another device
    INITIATE,              /// looking for another devce. send CMD_INITIATE every 2-3 seconds // TODO time
    OTHER_DEVICE_RECEIVE,  /// recieved a CMD_INITIATE from another device. send the key back every 5 seconds
                           /// until we recieve a wifi packet ACK. Then return to idle

} state;

static state currentState = IDLE;

void irSetup();

void irLoop();

#endif /// INFARED_H