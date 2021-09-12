//
// Created by MattWood on 24/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "format.h"
/******************************* DEFINES ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
/******************************* CONSTANTS ******************************/
/******************************* VARIABLES ******************************/
/*************************** PUBLIC FUNCTIONS ***************************/

char* format_renderBatteryState(battery_state_e state)
{
    char* output;
    switch (state)
    {
        case BATTERY_STATE_NOT_FITTED:
            output = "Not fitted";
            break;
        case BATTERY_STATE_CHARGING:
            output = "Charging";
            break;
        case BATTERY_STATE_DISCHARGING:
            output = "Discharging";
            break;
        case BATTERY_STATE_ABNORMAL:
            output = "Fault or no battery";
            break;
        case BATTERY_STATE_FULL:
            output = "Full";
            break;
        default:
            output = "Unknown";
            break;
    }

    return output;
}

char* format_renderPowerMonitorCondition(power_monitor_measurement_condition_e state)
{
    char* output;

    switch (state)
    {
        case PM_CONDITION_OK:
            output = "OK";
            break;
        case PM_CONDITION_NO_VOLTAGE:
            output = "No mains voltage present";
            break;
        case PM_CONDITION_BAD_FREQUENCY:
            output = "Mains frequency bad";
            break;
        case PM_CONDITION_INVALID:
        default:
            output = "Unknown";
            break;
    }

    return output;
}
/*************************** PRIVATE FUNCTIONS **************************/
