//
// Created by MattWood on 24/07/2021.
//

#ifndef FORMAT_H
#define FORMAT_H

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "std_includes.h"
#include "battery.h"
#include "power_monitor.h"
/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/
/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
char* format_renderBatteryState(battery_state_e state);
char* format_renderPowerMonitorCondition(power_monitor_measurement_condition_e state);
/******************************* GLOBALS ********************************/
#endif /* FORMAT_H */
