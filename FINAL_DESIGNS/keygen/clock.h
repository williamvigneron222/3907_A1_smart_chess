#pragma once
#include <stdint.h>

// freq_hz: clock frequency in Hz (ex: 1000000 for 1 MHz)
// gpio_num: the GPIO pin number to output the clock on
void clock_init(uint32_t freq_hz, int gpio_num);

// Convenience: start a default clock (you can change defaults in clock.cpp)
void clock_init_default();