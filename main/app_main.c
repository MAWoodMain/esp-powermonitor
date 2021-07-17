//
// Created by MattWood on 17/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/

#include "std_includes.h"
#include "led.h"
#include "power_monitor.h"

/******************************* DEFINES ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
/******************************* CONSTANTS ******************************/
/******************************* VARIABLES ******************************/
/*************************** PUBLIC FUNCTIONS ***************************/

_Noreturn void app_main(void)
{
    led_init();
    power_monitor_init();
    led_pretty_light_pattern();

    for (;;) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/*************************** PRIVATE FUNCTIONS **************************/