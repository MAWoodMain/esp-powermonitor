//
// Created by MattWood on 23/07/2021.
//

#ifndef BATTERY_H
#define BATTERY_H

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "std_includes.h"
/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/
typedef enum
{
    BATTERY_STATE_UNKNOWN,
    BATTERY_STATE_NOT_FITTED,
    BATTERY_STATE_CHARGING,
    BATTERY_STATE_DISCHARGING,
    BATTERY_STATE_ABNORMAL,
    BATTERY_STATE_FULL
} battery_state_e;
/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
bool battery_init(void);
battery_state_e battery_getState(void);
void battery_handleVoltageMeasurement(float voltage);
float battery_getVoltage();
/******************************* GLOBALS ********************************/
#endif /* BATTERY_H */
