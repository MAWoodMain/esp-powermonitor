#include <esp_types.h>
#include <sys/cdefs.h>
//
// Created by MattWood on 16/07/2021.
//

#include "power_monitor.h"
#include "driver/timer.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define POWER_MONITOR_SAMPLING_FREQUENCY_HZ 1000
#define POWER_MONITOR_PROCESSING_FREQUENCY_HZ 1
#define POWER_MONITOR_NO_CURRENT_CHANNELS 4
#define POWER_MONITOR_NO_SUBSAMPLES 3
#define POWER_MONITOR_NO_BUFFERS 2

#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate


typedef struct
{
    uint16_t buffer[POWER_MONITOR_SAMPLING_FREQUENCY_HZ / POWER_MONITOR_PROCESSING_FREQUENCY_HZ][POWER_MONITOR_NO_CURRENT_CHANNELS + 1];
    bool beingUsed;
} power_monitor_sampleBuffer_t;

typedef struct
{
    power_monitor_sampleBuffer_t *sampleBuffer;
    uint32_t sampleTimestamp;
} power_monitor_sampleNotification_t;

static power_monitor_sampleBuffer_t power_monitor_sampleBuffers[POWER_MONITOR_NO_BUFFERS];

static esp_adc_cal_characteristics_t *adc_chars;
static const adc1_channel_t power_monitor_voltageChannel = PINMAP_PM_CH4_ADC;
static const adc1_channel_t power_monitor_currentChannels[POWER_MONITOR_NO_CURRENT_CHANNELS] = { PINMAP_PM_CH4_ADC,
                                                                                                 PINMAP_PM_CH4_ADC,
                                                                                                 PINMAP_PM_CH4_ADC,
                                                                                                 PINMAP_PM_CH4_ADC };

static const char *TAG = "PM";
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;
static volatile uint8_t power_monitor_bufferIndex = 0;
static volatile uint32_t power_monitor_readingIndex = 0;

const size_t power_monitor_MessageBufferSizeBytes = 128;

static MessageBufferHandle_t power_monitor_messageBufferHandle;
static TaskHandle_t power_monitor_taskHandle;


static bool power_monitor_isr_callback(void *args);
_Noreturn void power_monitor_task( void *pvParameters );
static bool power_monitor_configurePins();
static bool power_monitor_configureTimer();

bool power_monitor_init(void)
{
    bool retVal = true;
    uint8_t idx;
    do
    {
        /* Configures the IO and ADC inputs for sampling */
        if(false == power_monitor_configurePins())
        {
            ESP_LOGE(TAG, "Failed to configure hardware");
            retVal = false;
            break;
        }

        /* Clear the message buffer for ISR to task processing calls */
        power_monitor_messageBufferHandle = xMessageBufferCreate( power_monitor_MessageBufferSizeBytes );
        if( power_monitor_messageBufferHandle == NULL )
        {
            ESP_LOGE(TAG, "Failed to create message buffer");
            retVal = false;
            break;
        }

         xTaskCreate(power_monitor_task, "PM", 2048, NULL, 1, &power_monitor_taskHandle);
        if( power_monitor_taskHandle == NULL )
        {
            ESP_LOGE(TAG, "Failed to create the processing task");
            retVal = false;
            break;
        }

        /* Sets up the timer and attaches the ISR (does not start the timer yet) */
        if(false == power_monitor_configureTimer())
        {
            ESP_LOGE(TAG, "Failed to configure the timer");
            retVal = false;
            break;
        }

        /* Clear buffers */
        for(idx = 0; idx < POWER_MONITOR_NO_BUFFERS; idx++)
        {
            power_monitor_sampleBuffers[idx].beingUsed = false;
            memset(power_monitor_sampleBuffers[idx].buffer, 0, sizeof(uint16_t)*(POWER_MONITOR_SAMPLING_FREQUENCY_HZ / POWER_MONITOR_PROCESSING_FREQUENCY_HZ)*(POWER_MONITOR_NO_CURRENT_CHANNELS + 1));
        }

        /* Starts the timer beginning sampling */
        if(ESP_OK != timer_start(TIMER_GROUP_0, TIMER_0))
        {
            ESP_LOGE(TAG, "Failed to start timer");
            retVal = false;
            break;
        }

    } while(false);

    return retVal;
}

static bool IRAM_ATTR power_monitor_isr_callback(void* args)
{
    gpio_set_level( PINMAP_ISR_TIMING, 1 );
    uint8_t sampleIdx;
    uint8_t channelIdx;

    power_monitor_sampleBuffers[power_monitor_bufferIndex].beingUsed = true;

    /* Sample voltage channel */
    for (sampleIdx = 0; sampleIdx < POWER_MONITOR_NO_SUBSAMPLES; sampleIdx++)
    {
        power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer[power_monitor_readingIndex][0] += adc1_get_raw(
                power_monitor_voltageChannel );
    }
    /* Divide for mean */
    power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer[power_monitor_readingIndex][0] /= POWER_MONITOR_NO_SUBSAMPLES;

    for (channelIdx = 0; channelIdx < POWER_MONITOR_NO_CURRENT_CHANNELS; channelIdx++)
    {
        /* Sample each current channel */
        for (sampleIdx = 0; sampleIdx < POWER_MONITOR_NO_SUBSAMPLES; sampleIdx++)
        {
            power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer[power_monitor_readingIndex][1 +
                                                                                                      channelIdx] += adc1_get_raw(
                    power_monitor_currentChannels[channelIdx] );
        }
        /* Divide for mean */
        power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer[power_monitor_readingIndex][1 +
                                                                                                  channelIdx] /= POWER_MONITOR_NO_SUBSAMPLES;
    }

    power_monitor_readingIndex++;
    power_monitor_readingIndex %= POWER_MONITOR_SAMPLING_FREQUENCY_HZ / POWER_MONITOR_PROCESSING_FREQUENCY_HZ;
    if (power_monitor_readingIndex == 0)
    {
        power_monitor_sampleNotification_t notification;
        power_monitor_sampleBuffers[power_monitor_bufferIndex].beingUsed = false;
        notification.sampleBuffer = &power_monitor_sampleBuffers[power_monitor_bufferIndex];
        notification.sampleTimestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        power_monitor_bufferIndex++;
        power_monitor_bufferIndex %= POWER_MONITOR_NO_BUFFERS;
        memset( power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer, 0,
                sizeof( uint16_t ) * ( POWER_MONITOR_SAMPLING_FREQUENCY_HZ / POWER_MONITOR_PROCESSING_FREQUENCY_HZ ) *
                ( POWER_MONITOR_NO_CURRENT_CHANNELS + 1 ) );

        BaseType_t xHigherPriorityTaskWoken = pdFALSE; /* Initialised to pdFALSE. */

        /* Attempt to send the string to the message buffer. */
        xMessageBufferSendFromISR( power_monitor_messageBufferHandle,
                                   (void*) &notification,
                                   sizeof(notification),
                                   &xHigherPriorityTaskWoken );

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
    gpio_set_level( PINMAP_ISR_TIMING, 0 );

    return true;
}


_Noreturn void power_monitor_task( void *pvParameters )
{
    power_monitor_sampleNotification_t notification;
    for( ;; )
    {
        if( 0 < xMessageBufferReceive( power_monitor_messageBufferHandle,
                                       ( void * ) &notification,
                                       sizeof( notification ),
                                       portMAX_DELAY )
                )
        {
            /* Got a message */
            ESP_LOGI( TAG, "GOT A MESSAGE! timestamp: %d, sample: %d, %d, %d", notification.sampleTimestamp,
                      notification.sampleBuffer->buffer[128][0], notification.sampleBuffer->buffer[256][0], notification.sampleBuffer->buffer[512][0] );

        }
    }

    vTaskDelete( NULL );
}

bool power_monitor_configurePins()
{
    bool retVal = true;
    uint8_t idx;
    /* Setup GPIO for timing measurements */
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(PINMAP_ISR_TIMING);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    if(ESP_OK != gpio_config(&io_conf))
    {
        ESP_LOGE(TAG, "Failed to configure timing GPIO");
        retVal = false;
    }
    /* Set to initially low */
    gpio_set_level(PINMAP_ISR_TIMING, 0);



    if(ESP_OK != adc1_config_width(width))
    {
        ESP_LOGE(TAG, "Failed to configure ADC width");
        retVal = false;
    }

    if(ESP_OK != adc1_config_channel_atten(power_monitor_voltageChannel, atten))
    {
        ESP_LOGE(TAG, "Failed to configure ADC attenuation (channel V)");
        retVal = false;
    }

    for(idx = 0; idx < POWER_MONITOR_NO_CURRENT_CHANNELS; idx++)
    {
        if(ESP_OK != adc1_config_channel_atten(power_monitor_currentChannels[idx], atten))
        {
            ESP_LOGE(TAG, "Failed to configure ADC attenuation (channel I%d)", idx);
            retVal = false;
        }
    }

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);

    return retVal;
}

bool power_monitor_configureTimer()
{
    bool retVal = true;
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
            .divider = TIMER_DIVIDER,
            .counter_dir = TIMER_COUNT_UP,
            .counter_en = TIMER_PAUSE,
            .alarm_en = TIMER_ALARM_EN,
            .auto_reload = true,
    }; // default clock source is APB
    if(ESP_OK != timer_init(TIMER_GROUP_0, TIMER_0, &config))
    {
        ESP_LOGE(TAG, "Failed to initialise the timer");
        retVal = false;
    }

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    if(ESP_OK != timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0))
    {
        ESP_LOGE(TAG, "Failed to set the timer counter value");
        retVal = false;
    }

    /* Configure the alarm value and the interrupt on alarm. */
    if(ESP_OK != timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, ( TIMER_SCALE / POWER_MONITOR_SAMPLING_FREQUENCY_HZ)))
    {
        ESP_LOGE(TAG, "Failed to set the timer alarm value");
        retVal = false;
    }
    if(ESP_OK != timer_enable_intr(TIMER_GROUP_0, TIMER_0))
    {
        ESP_LOGE(TAG, "Failed to set enable the ISR");
        retVal = false;
    }

    if(ESP_OK != timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, power_monitor_isr_callback, NULL, 0))
    {
        ESP_LOGE(TAG, "Failed to add the ISR callback");
        retVal = false;
    }
    return retVal;
}
