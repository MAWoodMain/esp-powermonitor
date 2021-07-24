#include <esp_types.h>
#include <sys/cdefs.h>
//
// Created by MattWood on 16/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "power_monitor.h"
#include "driver/timer.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "config.h"

/******************************* DEFINES ********************************/

#define POWER_MONITOR_SAMPLES_PER_BLOCK (CONFIG_PM_SAMPLING_RATE_HZ * (CONFIG_PM_SAMPLING_PROCESSING_PERIOD_MS / 1000))

#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate

/***************************** STRUCTURES *******************************/

typedef struct
{
    uint16_t buffer[POWER_MONITOR_SAMPLES_PER_BLOCK][CONFIG_PM_SAMPLING_CHANNEL_COUNT + 1];
    bool beingUsed;
} power_monitor_sampleBuffer_t;

typedef struct
{
    /* This is a pointer to the buffer storing the core readings */
    power_monitor_sampleBuffer_t *sampleBuffer;
    /* This is the measured mid point value in counts */
    uint16_t sampleZeroReferenceCounts;
    /* This is the time since boot in ms for time referencing */
    uint32_t sampleTimestamp;
} power_monitor_sampleNotification_t;

/************************** FUNCTION PROTOTYPES *************************/

static bool power_monitor_isr_callback(void *args);
_Noreturn void power_monitor_task( void *pvParameters );
static bool power_monitor_configurePins();
static bool power_monitor_configureTimer();

/******************************* CONSTANTS ******************************/

static const adc1_channel_t power_monitor_voltageChannel = PINMAP_PM_V_SEN_ADC;
static const adc1_channel_t power_monitor_currentChannels[CONFIG_PM_SAMPLING_CHANNEL_COUNT] =
        { PINMAP_PM_CH1_ADC,
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 1
          PINMAP_PM_CH2_ADC,
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 2
          PINMAP_PM_CH3_ADC,
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 3
          PINMAP_PM_CH4_ADC
#endif
#endif
#endif
        };
static const char *TAG = "PM";
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;

/******************************* VARIABLES ******************************/

static power_monitor_sampleBuffer_t power_monitor_sampleBuffers[CONFIG_PM_SAMPLING_NO_BUFFERS];

static esp_adc_cal_characteristics_t *adc_chars;

static volatile uint8_t power_monitor_bufferIndex = 0;
static volatile uint32_t power_monitor_readingIndex = 0;

static volatile uint16_t power_monitor_midPointCounts; /* This will be replaced with a measured value after each sample block */

static volatile uint16_t power_monitor_batteryReading = 0;
const size_t power_monitor_rawMessageBufferSizeBytes = 128;
const size_t power_monitor_measurementMessageBufferSizeBytes = 512;

static MessageBufferHandle_t power_monitor_rawMessageBufferHandle;
static MessageBufferHandle_t power_monitor_measurementMessageBufferHandle;
static TaskHandle_t power_monitor_taskHandle;

static power_monitor_measurement_t power_monitor_lastMeasurement = {
        .condition = PM_CONDITION_INVALID,
        .frequency = 0.0f,
        .rmsV = 0.0f,
        .batteryVoltage = 0.0f,
        .rmsI = {0.0f,0.0f,0.0f,0.0f},
        .rmsP = {0.0f,0.0f,0.0f,0.0f},
        .rmsVA = {0.0f,0.0f,0.0f,0.0f}
};

/*************************** PUBLIC FUNCTIONS ***************************/


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
        power_monitor_rawMessageBufferHandle = xMessageBufferCreate( power_monitor_rawMessageBufferSizeBytes );
        if(power_monitor_rawMessageBufferHandle == NULL )
        {
            ESP_LOGE(TAG, "Failed to create raw message buffer");
            retVal = false;
            break;
        }
        power_monitor_measurementMessageBufferHandle = xMessageBufferCreate( power_monitor_measurementMessageBufferSizeBytes );
        if(power_monitor_measurementMessageBufferHandle == NULL )
        {
            ESP_LOGE(TAG, "Failed to create measurement message buffer");
            retVal = false;
            break;
        }

         xTaskCreate(power_monitor_task, "PM", 4096, NULL, 1, &power_monitor_taskHandle);
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
        for(idx = 0; idx < CONFIG_PM_SAMPLING_NO_BUFFERS; idx++)
        {
            power_monitor_sampleBuffers[idx].beingUsed = false;
            memset(power_monitor_sampleBuffers[idx].buffer, 0, sizeof(uint16_t)*(POWER_MONITOR_SAMPLES_PER_BLOCK)*(CONFIG_PM_SAMPLING_CHANNEL_COUNT + 1));
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

MessageBufferHandle_t power_monitor_getOutputMessageHandle(void)
{
    return power_monitor_measurementMessageBufferHandle;
}

power_monitor_measurement_t power_monitor_getLastMeasurement(void)
{
    return power_monitor_lastMeasurement;
}

/*************************** PRIVATE FUNCTIONS **************************/

static bool IRAM_ATTR power_monitor_isr_callback(void* args)
{
#if CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC
    gpio_set_level( PINMAP_ISR_TIMING, 1 );
#endif /* CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC */
    uint8_t sampleIdx;
    uint8_t channelIdx;
    power_monitor_sampleBuffers[power_monitor_bufferIndex].beingUsed = true;

    /* Sample voltage channel */
    for (sampleIdx = 0; sampleIdx < CONFIG_PM_SAMPLING_NO_SUBSAMPLES; sampleIdx++)
    {
        power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer[power_monitor_readingIndex][0] += adc1_get_raw(
                power_monitor_voltageChannel );
    }
    /* Divide for mean */
    power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer[power_monitor_readingIndex][0] /= CONFIG_PM_SAMPLING_NO_SUBSAMPLES;

    for (channelIdx = 0; channelIdx < CONFIG_PM_SAMPLING_CHANNEL_COUNT; channelIdx++)
    {
        /* Sample each current channel */
        for (sampleIdx = 0; sampleIdx < CONFIG_PM_SAMPLING_NO_SUBSAMPLES; sampleIdx++)
        {
            power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer[power_monitor_readingIndex][1 +
                                                                                                      channelIdx] += adc1_get_raw(
                    power_monitor_currentChannels[channelIdx] );
        }
        /* Divide for mean */
        power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer[power_monitor_readingIndex][1 +
                                                                                                  channelIdx] /= CONFIG_PM_SAMPLING_NO_SUBSAMPLES;
    }

    power_monitor_batteryReading = adc1_get_raw( PINMAP_BAT_SEN_ADC );

    power_monitor_readingIndex++;
    power_monitor_readingIndex %= POWER_MONITOR_SAMPLES_PER_BLOCK;
    if (power_monitor_readingIndex == 0)
    {
        power_monitor_sampleNotification_t notification;

        /* TODO: measure mid point reference between sample blocks */
        notification.sampleZeroReferenceCounts = adc1_get_raw(ADC1_CHANNEL_5);

        power_monitor_sampleBuffers[power_monitor_bufferIndex].beingUsed = false;
        notification.sampleBuffer = &power_monitor_sampleBuffers[power_monitor_bufferIndex];
        notification.sampleTimestamp = pdTICKS_TO_MS(xTaskGetTickCount());
        power_monitor_bufferIndex++;
        power_monitor_bufferIndex %= CONFIG_PM_SAMPLING_NO_BUFFERS;
        memset( power_monitor_sampleBuffers[power_monitor_bufferIndex].buffer, 0,
                sizeof( uint16_t ) * ( POWER_MONITOR_SAMPLES_PER_BLOCK ) *
                ( CONFIG_PM_SAMPLING_CHANNEL_COUNT + 1 ) );

        BaseType_t xHigherPriorityTaskWoken = pdFALSE; /* Initialised to pdFALSE. */

        /* Attempt to send the string to the message buffer. */
        xMessageBufferSendFromISR( power_monitor_rawMessageBufferHandle,
                                   (void*) &notification,
                                   sizeof(notification),
                                   &xHigherPriorityTaskWoken );

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
#if CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC
    gpio_set_level( PINMAP_ISR_TIMING, 0 );
#endif /* CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC */

    return true;
}


_Noreturn void power_monitor_task( void *pvParameters )
{
    power_monitor_sampleNotification_t notification;
    int32_t v, lastV, i, maxV, minV, maxI[CONFIG_PM_SAMPLING_CHANNEL_COUNT], minI[CONFIG_PM_SAMPLING_CHANNEL_COUNT], sumP[CONFIG_PM_SAMPLING_CHANNEL_COUNT];
    uint32_t sqV, sumV;
    uint32_t sqI, sumI[CONFIG_PM_SAMPLING_CHANNEL_COUNT];
    uint32_t sample, channel, vOffset, iOffset, lastPositiveCrossingIdx, positiveCrossings, sumCrossingPeriod;

    for( ;; )
    {
        if( 0 < xMessageBufferReceive( power_monitor_rawMessageBufferHandle,
                                       ( void * ) &notification,
                                       sizeof( notification ),
                                       portMAX_DELAY )
                )
        {
#if CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC
            gpio_set_level(PINMAP_TASK_TIMING, 1);
#endif /* CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC */
            power_monitor_measurement_t measurement;


            measurement.sampleTimestamp = notification.sampleTimestamp;
            measurement.condition = PM_CONDITION_OK;

            sumV = 0;
            minV = 0;
            maxV = 0;
            lastV = 0;
            lastPositiveCrossingIdx = 0;
            positiveCrossings = 0;
            sumCrossingPeriod = 0;
            for(channel = 0; channel < CONFIG_PM_SAMPLING_CHANNEL_COUNT; channel++)
            {
                minI[channel] = 0;
                maxI[channel] = 0;
                sumI[channel] = 0;
            }


            for(sample = 0; sample < POWER_MONITOR_SAMPLES_PER_BLOCK; sample++)
            {
                sumV += notification.sampleBuffer->buffer[sample][0];
            }

            vOffset = esp_adc_cal_raw_to_voltage( sumV / POWER_MONITOR_SAMPLES_PER_BLOCK, adc_chars);
            iOffset = esp_adc_cal_raw_to_voltage( notification.sampleZeroReferenceCounts, adc_chars);
            sumV = 0;

            for(sample = 0; sample < POWER_MONITOR_SAMPLES_PER_BLOCK; sample++)
            {
                /* Convert to mV */
                v = (int32_t) esp_adc_cal_raw_to_voltage(notification.sampleBuffer->buffer[sample][0], adc_chars );
                /* Remove DC vOffset */
                v -= (int32_t) vOffset;

                if(sample != 0)
                {
                    if((lastV < 0) && (v >= 0))
                    {
                        if(lastPositiveCrossingIdx != 0)
                        {
                            sumCrossingPeriod += sample-lastPositiveCrossingIdx;
                            positiveCrossings++;
                        }
                        lastPositiveCrossingIdx = sample;
                    }
                }
                lastV = v;
                if(v < minV) minV = v;
                if(v > maxV) maxV = v;
                sqV = v * v;
                sumV += sqV;

                for(channel = 0; channel < CONFIG_PM_SAMPLING_CHANNEL_COUNT; channel++)
                {
                    /* Remove zero vOffset and convert to mV */
                    i = (int32_t) esp_adc_cal_raw_to_voltage(notification.sampleBuffer->buffer[sample][1 + channel], adc_chars );
                    /* Remove DC vOffset */
                    i -= (int32_t) iOffset;
                    if(i < minI[channel]) minI[channel] = i;
                    if(i > maxI[channel]) maxI[channel] = i;
                    sqI = i * i;
                    sumI[channel] += sqI;
                    sumP[channel] = v*i;
                }

            }

            measurement.batteryVoltage = 2.0f * (float)(esp_adc_cal_raw_to_voltage(power_monitor_batteryReading, adc_chars)/1000.0);

            measurement.rmsV = sqrtf((float)sumV/POWER_MONITOR_SAMPLES_PER_BLOCK) * config_getFloatField(CONFIG_FLOAT_FIELD_V_CAL);
            measurement.frequency = CONFIG_PM_SAMPLING_RATE_HZ/((float)sumCrossingPeriod/(float)positiveCrossings);
#if CONFIG_PM_SAMPLING_LINE_FREQUENCY_50HZ
            const uint8_t intendedFrequency = 50;
#elif CONFIG_PM_SAMPLING_LINE_FREQUENCY_60HZ
            const uint8_t intendedFrequency = 60;
#else
#error "No line frequency specified"
#endif
            if((measurement.frequency < (intendedFrequency * 0.9)) || (measurement.frequency > (intendedFrequency * 1.1)))
            {

                measurement.condition = PM_CONDITION_BAD_FREQUENCY;
            }
            if(measurement.rmsV < 50.0f)
            {
                measurement.condition = PM_CONDITION_NO_VOLTAGE;
            }

            for(channel = 0; channel < CONFIG_PM_SAMPLING_CHANNEL_COUNT; channel++)
            {
                measurement.rmsI[channel] = sqrtf((float)sumI[channel]/POWER_MONITOR_SAMPLES_PER_BLOCK);
                measurement.rmsVA[channel] = measurement.rmsI[channel] * measurement.rmsV;
                measurement.rmsP[channel] = config_getFloatField(CONFIG_FLOAT_FIELD_V_CAL) * config_getFloatField(CONFIG_FLOAT_FIELD_I1_CAL+channel) * sumP[channel];
            }
#if CONFIG_PM_PRINT_ENABLE
            ESP_LOGI( TAG, "Measurement ->\n\r\tV:\t\t%.3f V (%.2fV <-> %.2fV)\n\r\tFreq:\t%.2f Hz\n\r\tI[0]:\t%.3f mV (%d mV <-> %d mV)"
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 1
                           "\n\r\tI[1]:\t%.3f mV (%d mV <-> %d mV)"
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 2
                           "\n\r\tI[2]:\t%.3f mV (%d mV <-> %d mV)"
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 3
                           "\n\r\tI[3]:\t%.3f mV (%d mV <-> %d mV)"
#endif
#endif
#endif
                      ,measurement.rmsV, minV*PM_V_SCALE_FACTOR, maxV*PM_V_SCALE_FACTOR, measurement.frequency,
                      measurement.rmsI[0], minI[0], maxI[0]
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 1
                      ,measurement.rmsI[1], minI[1], maxI[1]
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 2
                      ,measurement.rmsI[2], minI[2], maxI[2]
#if CONFIG_PM_SAMPLING_CHANNEL_COUNT > 3
                      ,measurement.rmsI[3], minI[3], maxI[3]
#endif
#endif
#endif
                      );
#endif
            power_monitor_lastMeasurement = measurement;
            xMessageBufferSend( power_monitor_measurementMessageBufferHandle,
                                       (void*) &measurement,
                                       sizeof(measurement),
                                pdMS_TO_TICKS(100)
            );
        }
#if CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC
        gpio_set_level(PINMAP_TASK_TIMING, 0);
#endif /* CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC */
    }

    vTaskDelete( NULL );
}

bool power_monitor_configurePins()
{
    bool retVal = true;
    uint8_t idx;
#if CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC
    /* Setup GPIO for timing measurements */
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT(PINMAP_ISR_TIMING) | BIT(PINMAP_TASK_TIMING) | BIT(PINMAP_TRANSMIT_TIMING);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    if(ESP_OK != gpio_config(&io_conf))
    {
        ESP_LOGE(TAG, "Failed to configure timing GPIO");
        retVal = false;
    }
    /* Set to initially low */
    gpio_set_level(PINMAP_ISR_TIMING, 0);
    gpio_set_level(PINMAP_TASK_TIMING, 0);
    gpio_set_level(PINMAP_TRANSMIT_TIMING, 0);
#endif /* CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC */


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

    for(idx = 0; idx < CONFIG_PM_SAMPLING_CHANNEL_COUNT; idx++)
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
    if(ESP_OK != timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, ( TIMER_SCALE / CONFIG_PM_SAMPLING_RATE_HZ)))
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
