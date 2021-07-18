//
// Created by MattWood on 14/07/2021.
//

#ifndef STD_INCLUDES_H
#define STD_INCLUDES_H
/**************************** LIB INCLUDES ******************************/

/* stdlib */
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

/**************************** USER INCLUDES *****************************/

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/message_buffer.h"
#include "freertos/event_groups.h"

/* ESP-IDF */
#include "sdkconfig.h"
#include "esp_log.h"

/* APP */
#include "pinmap.h"

/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/
/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
/******************************* GLOBALS ********************************/

#endif /* STD_INCLUDES_H */
