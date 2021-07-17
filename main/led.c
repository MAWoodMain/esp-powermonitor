//
// Created by MattWood on 15/07/2021.
//

#include "led.h"
#include "driver/ledc.h"


#define LED_CHANNEL_MAX 3

typedef enum {
    led_CHANNEL_RED,
    led_CHANNEL_GREEN,
    led_CHANNEL_BLUE
} led_channel_e;

bool led_setBrightness(led_channel_e channel, uint16_t brightness, uint16_t fadeTime, bool waitUntilComplete);

const ledc_channel_config_t led_channels[LED_CHANNEL_MAX] = {
        [led_CHANNEL_RED] = {
                .channel    = LEDC_CHANNEL_0,
                .duty       = 0,
                .gpio_num   = PINMAP_LED_RED,
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .hpoint     = 0,
                .timer_sel  = LEDC_TIMER_0,
                .flags.output_invert = PINMAP_LED_INVERTED
        },
        [led_CHANNEL_GREEN] = {
                .channel    = LEDC_CHANNEL_1,
                .duty       = 0,
                .gpio_num   = PINMAP_LED_GREEN,
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .hpoint     = 0,
                .timer_sel  = LEDC_TIMER_0,
                .flags.output_invert = PINMAP_LED_INVERTED
        },
        [led_CHANNEL_BLUE] = {
                .channel    = LEDC_CHANNEL_2,
                .duty       = 0,
                .gpio_num   = PINMAP_LED_BLUE,
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .hpoint     = 0,
                .timer_sel  = LEDC_TIMER_0,
                .flags.output_invert = PINMAP_LED_INVERTED
        }
};

bool led_init(void)
{
    bool retVal = false;
    ledc_timer_config_t timer_config = {
            .duty_resolution    = 8,
            .freq_hz            = 1000,
            .speed_mode         = LEDC_LOW_SPEED_MODE,
            .timer_num          = LEDC_TIMER_0,
            .clk_cfg            = LEDC_AUTO_CLK
    };

    ledc_timer_config( &timer_config );

    for(uint8_t channel = 0; channel < LED_CHANNEL_MAX; channel++)
    {
        ledc_channel_config(&led_channels[channel]);
    }

    retVal = ledc_fade_func_install(0) == ESP_OK;
    return retVal;
}

void led_pretty_light_pattern(void)
{
    led_setBrightness(led_CHANNEL_RED, 255, 1000, false);
    vTaskDelay( pdMS_TO_TICKS(1500));
    led_setBrightness(led_CHANNEL_RED, 0, 1000, false);
    vTaskDelay( pdMS_TO_TICKS(1500));
    led_setBrightness(led_CHANNEL_GREEN, 255, 1000, false);
    vTaskDelay( pdMS_TO_TICKS(1500));
    led_setBrightness(led_CHANNEL_GREEN, 0, 1000, false);
    vTaskDelay( pdMS_TO_TICKS(1500));
    led_setBrightness(led_CHANNEL_BLUE, 255, 1000, false);
    vTaskDelay( pdMS_TO_TICKS(1500));
    led_setBrightness(led_CHANNEL_BLUE, 0, 1000, false);
    vTaskDelay( pdMS_TO_TICKS(1500));
}
/*************************** PRIVATE FUNCTIONS **************************/

bool led_setBrightness(led_channel_e channel, uint16_t brightness, uint16_t fadeTime, bool waitUntilComplete)
{
    bool retVal = false;
    brightness = MIN(brightness, LED_MAX_BRIGHTNESS);
    if(fadeTime == 0)
    {
        ledc_set_duty(led_channels[channel].speed_mode, led_channels[channel].channel, brightness);
        ledc_update_duty(led_channels[channel].speed_mode, led_channels[channel].channel);
        retVal = true;
    }
    else
    {
        ledc_set_fade_with_time(led_channels[channel].speed_mode, led_channels[channel].channel, brightness, fadeTime);
        ledc_fade_start(led_channels[channel].speed_mode, led_channels[channel].channel, waitUntilComplete?LEDC_FADE_WAIT_DONE:LEDC_FADE_NO_WAIT);
        retVal = true;
    }
    return retVal;
}

bool led_setLed(uint16_t red, uint16_t green, uint16_t blue, uint16_t fadeTime)
{
    bool retVal = false;
    retVal = led_setBrightness(led_CHANNEL_RED, red, fadeTime, false) &&
             led_setBrightness(led_CHANNEL_GREEN, green, fadeTime, false) &&
             led_setBrightness(led_CHANNEL_BLUE, blue, fadeTime, false);
    return retVal;
}
