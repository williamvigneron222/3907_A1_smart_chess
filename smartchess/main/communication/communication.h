#pragma once
#include <cstdint>
#include <cstddef>

typedef void (*audio_rx_cb_t)(const int16_t *samples, size_t num_samples);

void communication_setup(audio_rx_cb_t rx_callback);
void communication_send(const int16_t *samples, size_t num_samples);
void communication_loop();