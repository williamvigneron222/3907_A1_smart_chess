#pragma once
#include <cstdint>

#define PAYLOAD_MAX 160

typedef struct __attribute__((packed)) {
    uint16_t seq;
    uint16_t pt_len;
    uint16_t ct_len;
    uint8_t nonce[16];
    uint8_t tag[16];
    uint8_t payload[PAYLOAD_MAX];
} enc_packet_t;