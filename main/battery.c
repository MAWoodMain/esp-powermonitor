//
// Created by MattWood on 23/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "battery.h"
#include "driver/pcnt.h"
/******************************* DEFINES ********************************/
#define BATTERY_COUNTER_UNIT PCNT_UNIT_0
#define BATTERY_ABNORMAL_PULSE_FREQUENCY_HZ 1000
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
#if 0
static void IRAM_ATTR battery_counter_intr_handler(void *arg);
#endif
static void IRAM_ATTR battery_cso_intr_handler(void *arg);
/******************************* CONSTANTS ******************************/
static const char *TAG = "BATT";
/******************************* VARIABLES ******************************/
float battery_lastReportedVoltage = 0.0f;
volatile uint32_t battery_lastLowTime = 0;
volatile uint32_t battery_lastHighTime = 0;
/*************************** PUBLIC FUNCTIONS ***************************/
bool battery_init(void)
{
    bool retVal = true;

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = BIT( PINMAP_BAT_CSO ) | BIT( PINMAP_BAT_PG );
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    if (ESP_OK != gpio_config( &io_conf ))
    {
        ESP_LOGE( TAG, "Failed to configure GPIO" );
        retVal = false;
    }

    gpio_set_intr_type(PINMAP_BAT_CSO, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PINMAP_BAT_CSO, battery_cso_intr_handler, NULL);
#if 0
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
            // Set PCNT input signal and control GPIOs
            .pulse_gpio_num = PINMAP_BAT_CSO,
            .ctrl_gpio_num = -1,
            .channel = PCNT_CHANNEL_0,
            .unit = BATTERY_COUNTER_UNIT,
            // What to do on the positive / negative edge of pulse input?
            .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
            .neg_mode = PCNT_COUNT_INC,   // Keep the counter value on the negative edge
            // What to do when control input is low or high?
            .lctrl_mode = PCNT_MODE_KEEP, // Reverse counting direction if low
            .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
            // Set the maximum and minimum limit values to watch
            .counter_h_lim = BATTERY_ABNORMAL_PULSE_FREQUENCY_HZ,
            .counter_l_lim = 0,
    };
    /* Initialize PCNT unit */
    pcnt_unit_config( &pcnt_config );

    /* Configure and enable the input filter */
    pcnt_set_filter_value( BATTERY_COUNTER_UNIT, 100 );
    pcnt_filter_enable( BATTERY_COUNTER_UNIT );

    //pcnt_set_event_value( BATTERY_COUNTER_UNIT, PCNT_EVT_THRES_0, BATTERY_ABNORMAL_PULSE_FREQUENCY_HZ );
    //pcnt_event_enable( BATTERY_COUNTER_UNIT, PCNT_EVT_THRES_0 );
    /* Enable events on zero, maximum and minimum limit values */
    //pcnt_event_enable( BATTERY_COUNTER_UNIT, PCNT_EVT_ZERO );
    pcnt_event_enable( BATTERY_COUNTER_UNIT, PCNT_EVT_H_LIM );

    /* Initialize PCNT's counter */
    pcnt_counter_pause( BATTERY_COUNTER_UNIT );
    pcnt_counter_clear( BATTERY_COUNTER_UNIT );

    gpio_pullup_en(PINMAP_BAT_CSO);
    /* Install interrupt service and add isr callback handler */
    pcnt_isr_service_install( 0 );
    pcnt_isr_handler_add( BATTERY_COUNTER_UNIT, battery_counter_intr_handler, NULL );

    /* Everything is set up, now go to counting */
    pcnt_counter_resume( BATTERY_COUNTER_UNIT );

#endif
    return retVal;
}

battery_state_e battery_getState(void)
{
    battery_state_e state = BATTERY_STATE_UNKNOWN;
    uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
    uint32_t lastLow = battery_lastLowTime;
    uint32_t lastHigh = battery_lastHighTime;

    /* if line activity in the last 1.1 seconds */
    if(((lastHigh + 1100) > now) || ((lastLow + 1100) > now))
    {
        /* if lines changes are within 1.1 seconds of each other assume its oscillating */
        if((MAX(lastLow,lastHigh) - MIN(lastLow,lastHigh)) < 1100)
        {
            //state = BATTERY_STATE_ABNORMAL;
        }
    }

    if(state != BATTERY_STATE_ABNORMAL)
    {
        if((battery_lastReportedVoltage > 2.0f) && (battery_lastReportedVoltage < 4.3f))
        {
            state = BATTERY_STATE_DISCHARGING;
        }
        else if(lastLow > lastHigh)
        {
            state = BATTERY_STATE_CHARGING;
        }
        else if(battery_lastReportedVoltage > 4.8)
        {
            state = BATTERY_STATE_NOT_FITTED;
        }
        else
        {
            //state = BATTERY_STATE_FULL;
            state = BATTERY_STATE_CHARGING;
        }
    }

    return state;
}

void battery_handleVoltageMeasurement(float voltage)
{
    battery_lastReportedVoltage = voltage;
}

float battery_getVoltage()
{
    return battery_lastReportedVoltage;
}

/*************************** PRIVATE FUNCTIONS **************************/

#if 0
static void IRAM_ATTR battery_counter_intr_handler(void *arg)
{
    uint32_t status = 0;
    if(ESP_OK == pcnt_get_event_status(BATTERY_COUNTER_UNIT, &status))
    {
        if(status & PCNT_EVT_H_LIM)
        {
            if((batteryTime + (BATTERY_ABNORMAL_PULSE_FREQUENCY_HZ*1.5)) > pdTICKS_TO_MS(xTaskGetTickCount()))
            {
                ESP_LOGI(TAG, "abnormal mode detected");
            }
            else
            {
                ESP_LOGI(TAG, "counts too slow");
            }
        }
    }
}
#endif
static void IRAM_ATTR battery_cso_intr_handler(void *arg)
{
    if(gpio_get_level(PINMAP_BAT_CSO))
    {
        /* CSO HIGH */
        battery_lastHighTime = pdTICKS_TO_MS(xTaskGetTickCount());
    }
    else
    {
        /* CSO LOW */
        battery_lastLowTime = pdTICKS_TO_MS(xTaskGetTickCount());
    }
}
