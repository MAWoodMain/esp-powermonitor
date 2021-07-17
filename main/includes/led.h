//
// Created by MattWood on 15/07/2021.
//

#ifndef LED_H
#define LED_H
/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/

#include "std_includes.h"

/******************************* DEFINES ********************************/

#define LED_MAX_BRIGHTNESS 255

/******************************** ENUMS *********************************/
/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/

bool led_init(void);
bool led_setLed(uint16_t red, uint16_t green, uint16_t blue, uint16_t fadeTime);
void led_pretty_light_pattern(void);

/******************************* GLOBALS ********************************/



#endif /* LED_H */
