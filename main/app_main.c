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
    uint8_t payload[128];

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    led_init();
    power_monitor_init();
    wifi_init();
    wifi_connect();
    if(wifi_waitForConnection(portMAX_DELAY))
    {
        web_server_init();
        mqtt_init();
        mqtt_connect();
        if(wifi_waitForConnection(portMAX_DELAY))
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
                    if(measurement.condition == PM_CONDITION_OK)
                    {
                        memset(payload, 0, sizeof(payload));
                        sprintf( (char*) payload, "{\"rmsV\":%.3f,\"frequency\":%.2f, \"rmsI\":[%.3f,%.3f,%.3f,%.3f]}", measurement.rmsV,
                                 measurement.frequency, measurement.rmsI[0], measurement.rmsI[1], measurement.rmsI[2], measurement.rmsI[3] );
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

    for(;;)
    {
        led_pretty_light_pattern();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/*************************** PRIVATE FUNCTIONS **************************/