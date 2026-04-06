#pragma once
#include <cstdint>

#define PAYLOAD_MAX_BYTES   160
#define PAYLOAD_MAX_SAMPLES (PAYLOAD_MAX_BYTES / sizeof(int16_t))  // 80 samples

typedef struct __attribute__((packed)) {
    uint16_t seq;
    uint16_t pt_samples;
    uint16_t ct_bytes;
    uint64_t nonce[2];
    uint64_t tag[2];
    uint8_t  payload[PAYLOAD_MAX_BYTES];
} enc_packet_t;