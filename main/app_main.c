//
// Created by MattWood on 17/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/

#include <nvs_flash.h>
#include <esp_task_wdt.h>
#include "std_includes.h"
#include "led.h"
#include "power_monitor.h"
#include "esp_system.h"
#include "web_server.h"
#include "wifi.h"
#include "mqtt.h"
#include "config.h"
#include "battery.h"
#include "format.h"
#include "home_assistant.h"

/******************************* DEFINES ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
bool app_main_mqttImpl(char* topic, char* payload, bool retain);
/******************************* CONSTANTS ******************************/
static const char* TAG = "MAIN";
static home_assistant_deviceConfig_t device_config =
        {
                .componentType = HA_COMPONENT_SENSOR,
                .deviceName = "ESP Power Monitor",
                .model = "esp-pm-v1",
                .manufacturer = "Matt Wood",
                .useCombinedPayload = true,
                .mqttImplementation = app_main_mqttImpl
        };

static home_assistant_deviceChannelConfig_t voltage_channel_config =
        {
                .name = "Line Voltage",
                .unitOfMeasurement = HA_UOM_VOLT,
                .class = HA_CLASS_VOLTAGE,
                .valueType = HA_TYPE_DOUBLE,
                .initialValue.d = 0.0,
                .decimalPlaces = 1,
                .stateClass = HA_STATE_CLASS_MEASUREMENT
        };

static home_assistant_deviceChannelConfig_t frequency_channel_config =
        {
                .name = "Line Frequency",
                .unitOfMeasurement = HA_UOM_HZ,
                .class = HA_CLASS_HA_CLASS_NONE,
                .valueType = HA_TYPE_DOUBLE,
                .initialValue.d = 0.0,
                .decimalPlaces = 2,
                .stateClass = HA_STATE_CLASS_MEASUREMENT
        };

static home_assistant_deviceChannelConfig_t current_1_channel_config =
        {
                .name = "Current 1",
                .unitOfMeasurement = HA_UOM_AMP,
                .class = HA_CLASS_CURRENT,
                .valueType = HA_TYPE_DOUBLE,
                .initialValue.d = 0.0,
                .decimalPlaces = 3,
                .stateClass = HA_STATE_CLASS_MEASUREMENT
        };

static home_assistant_deviceChannelConfig_t power_1_channel_config =
        {
                .name = "Power 1",
                .unitOfMeasurement = HA_UOM_WATT,
                .class = HA_CLASS_POWER,
                .valueType = HA_TYPE_DOUBLE,
                .initialValue.d = 0.0,
                .decimalPlaces = 3,
                .stateClass = HA_STATE_CLASS_MEASUREMENT
        };

static home_assistant_deviceChannelConfig_t apparent_power_1_channel_config =
        {
                .name = "Apparent Power 1",
                .unitOfMeasurement = HA_UOM_VA,
                .class = HA_CLASS_POWER,
                .valueType = HA_TYPE_DOUBLE,
                .initialValue.d = 0.0,
                .decimalPlaces = 3,
                .stateClass = HA_STATE_CLASS_MEASUREMENT
        };

static home_assistant_deviceChannelConfig_t power_consumption_1_channel_config =
        {
                .name = "Power Consumption 1",
                .unitOfMeasurement = HA_UOM_KWH,
                .class = HA_CLASS_ENERGY,
                .valueType = HA_TYPE_DOUBLE,
                .initialValue.d = 0.0,
                .decimalPlaces = 3,
                .stateClass = HA_STATE_CLASS_TOTAL_INCREASING
        };
/******************************* VARIABLES ******************************/
home_assistant_deviceChannel_t* voltage_channel;
home_assistant_deviceChannel_t* frequency_channel;
home_assistant_deviceChannel_t* current_channel;
home_assistant_deviceChannel_t* power_channel;
home_assistant_deviceChannel_t* apparentPower_channel;
home_assistant_deviceChannel_t* powerConsumption_channel;


/*************************** PUBLIC FUNCTIONS ***************************/

_Noreturn void app_main(void)
{
    power_monitor_measurement_t measurement;
    uint8_t payload[512];

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if(ESP_OK != esp_read_mac( device_config.macAddress, ESP_MAC_WIFI_STA))
    {
        ESP_LOGE(TAG, "Failed to read factory MAC Address");
    }

    vTaskDelay( pdMS_TO_TICKS(2000));
    led_init();
    config_init();
    power_monitor_init();
    battery_init();
    wifi_init();
    wifi_connect();

    home_assistant_init( device_config );

    voltage_channel = home_assistant_createChannel( voltage_channel_config );
    frequency_channel = home_assistant_createChannel( frequency_channel_config );
    current_channel = home_assistant_createChannel( current_1_channel_config );
    power_channel = home_assistant_createChannel( power_1_channel_config );
    apparentPower_channel = home_assistant_createChannel( apparent_power_1_channel_config );
    powerConsumption_channel = home_assistant_createChannel( power_consumption_1_channel_config );


    if(wifi_waitForConnection(portMAX_DELAY))
    {
        web_server_init();
        mqtt_init();
        mqtt_connect();
        if(mqtt_waitForConnection(portMAX_DELAY))
        {
            home_assistant_sendDiscoveryMessage();
            led_pretty_light_pattern();

            for (;;) {
                if( 0 < xMessageBufferReceive( power_monitor_getOutputMessageHandle(),
                                               ( void * ) &measurement,
                                               sizeof( measurement ),
                                               portMAX_DELAY )
                        )
                {
#if CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC
                    gpio_set_level(PINMAP_TRANSMIT_TIMING, 1);
#endif /* CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC */

                    battery_handleVoltageMeasurement(measurement.batteryVoltage);
                    if(measurement.condition == PM_CONDITION_OK)
                    {
                        powerConsumption_channel->latestValue.d += ((measurement.rmsP[0] / 3600.0)/1000);
                        home_assistant_submitDoubleValue(voltage_channel, measurement.rmsV);
                        home_assistant_submitDoubleValue(frequency_channel, measurement.frequency);
                        home_assistant_submitDoubleValue(current_channel, measurement.rmsI[0]);
                        home_assistant_submitDoubleValue(power_channel, measurement.rmsP[0]);
                        home_assistant_submitDoubleValue(apparentPower_channel, measurement.rmsVA[0]);
                        home_assistant_publishValues();
                        memset(payload, 0, sizeof(payload));
                        sprintf( (char*) payload,
                                 "{\"rmsV\":%.03f,\"frequency\":%.02f, \"rmsI\":[%.03f,%.03f,%.03f,%.03f],\"rmsVA\":[%.03f,%.03f,%.03f,%.03f],\"rmsP\":[%.03f,%.03f,%.03f,%.03f],\"vssVoltage\":%.02f,\"batteryStatus\":\"%s\"}",
                                 measurement.rmsV, measurement.frequency,
                                 measurement.rmsI[0], measurement.rmsI[1], measurement.rmsI[2],measurement.rmsI[3],
                                 measurement.rmsVA[0], measurement.rmsVA[1], measurement.rmsVA[2],measurement.rmsVA[3],
                                 measurement.rmsP[0], measurement.rmsP[1], measurement.rmsP[2],measurement.rmsP[3],
                                 battery_getVoltage(), format_renderBatteryState(battery_getState()));
                        mqtt_send("pm/dev/data",(char*)payload,1,false);
                    }
                    else if(measurement.condition == PM_CONDITION_BAD_FREQUENCY)
                    {
                        ESP_LOGW(TAG, "measurement frequency bad");
                    }
                    else if(measurement.condition == PM_CONDITION_NO_VOLTAGE)
                    {
                        ESP_LOGW(TAG, "measurement shows no voltage present");
                    }
                    else
                    {
                        ESP_LOGE(TAG, "unknown measurement condition");
                    }
                }
                else
                {
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
                //esp_task_wdt_reset();
            }
        }
    }
    else
    {
        /* wifi connect failed, enable AP mode so the real wifi can be configured */
        wifi_switchToSoftAP();
        wifi_connect();
        web_server_init();
        /* after 10 minutes give up and restart */
        vTaskDelay(600000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    for(;;)
    {
        led_pretty_light_pattern();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/*************************** PRIVATE FUNCTIONS **************************/

bool app_main_mqttImpl(char* topic, char* payload, bool retain)
{
    return mqtt_send( topic, payload, 1, retain );
}