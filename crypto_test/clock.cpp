#include "clock.h"

#include "driver/ledc.h"
#include "esp_err.h"

static constexpr int DEFAULT_CLOCK_GPIO = 18;
static constexpr uint32_t DEFAULT_CLOCK_FREQ_HZ = 10000; // 10 kHz to DFF clock

void clock_init(uint32_t freq_hz, int gpio_num)
{
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    timer_cfg.timer_num = LEDC_TIMER_0;
    timer_cfg.duty_resolution = LEDC_TIMER_10_BIT;
    timer_cfg.freq_hz = freq_hz;
    timer_cfg.clk_cfg = LEDC_AUTO_CLK;

    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {};
    ch_cfg.speed_mode = LEDC_HIGH_SPEED_MODE;
    ch_cfg.channel = LEDC_CHANNEL_0;
    ch_cfg.timer_sel = LEDC_TIMER_0;
    ch_cfg.intr_type = LEDC_INTR_DISABLE;
    ch_cfg.gpio_num = gpio_num;
    ch_cfg.duty = (1 << 10) / 2;   // 50% duty
    ch_cfg.hpoint = 0;

    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));
}

void clock_init_default()
{
    clock_init(DEFAULT_CLOCK_FREQ_HZ, DEFAULT_CLOCK_GPIO);
}