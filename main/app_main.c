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

/******************************* DEFINES ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
/******************************* CONSTANTS ******************************/
static const char *TAG = "MAIN";
/******************************* VARIABLES ******************************/
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
    led_init();
    config_init();
    power_monitor_init();
    battery_init();
    wifi_init();
    wifi_connect();

    if(wifi_waitForConnection(portMAX_DELAY))
    {
        web_server_init();
        mqtt_init();
        mqtt_connect();
        if(mqtt_waitForConnection(portMAX_DELAY))
        {
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