#pragma once
#include <cstdint>
#include <cstddef>

// Firmware registers this callback — called when audio is decrypted on receive
// samples: pointer to int16_t audio buffer
// num_samples: number of int16_t samples
typedef void (*audio_rx_cb_t)(const int16_t *samples, size_t num_samples);

// Call once in setup() — pass your playback callback
void communication_setup(audio_rx_cb_t rx_callback);

// Call in loop() when PTT is held — pass audio block from I2S mic
// num_samples max is PAYLOAD_MAX_SAMPLES (80)
void communication_send(const int16_t *samples, size_t num_samples);

// Call in loop() — nothing to poll but keeps structure clean
void communication_loop();