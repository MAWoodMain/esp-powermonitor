//
// Created by MattWood on 12/09/2021.
//

#ifndef HOME_ASSISTANT_CONSTANTS_H
#define HOME_ASSISTANT_CONSTANTS_H

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/

typedef enum
{
    HA_CLASS_HA_CLASS_NONE,
    HA_CLASS_AQI,
    HA_CLASS_BATTERY,
    HA_CLASS_CARBON_DIOXIDE,
    HA_CLASS_CARBON_MONOXIDE,
    HA_CLASS_CURRENT,
    HA_CLASS_ENERGY,
    HA_CLASS_GAS,
    HA_CLASS_HUMIDITY,
    HA_CLASS_ILLUMINANCE,
    HA_CLASS_MONETARY,
    HA_CLASS_NITROGEN_DIOXIDE,
    HA_CLASS_NITROGEN_MONOXIDE,
    HA_CLASS_NITROUS_OXIDE,
    HA_CLASS_OZONE,
    HA_CLASS_PM1,
    HA_CLASS_PM10,
    HA_CLASS_PM25,
    HA_CLASS_POWER_FACTOR,
    HA_CLASS_POWER,
    HA_CLASS_PRESSURE,
    HA_CLASS_SIGNAL_STRENGTH,
    HA_CLASS_SULPHUR_DIOXIDE,
    HA_CLASS_TEMPERATURE,
    HA_CLASS_TIMESTAMP,
    HA_CLASS_VOC,
    HA_CLASS_VOLTAGE,
    HA_CLASS_MAX
} home_assistant_deviceClass_e;

typedef enum
{
    HA_STATE_CLASS_MEASUREMENT,
    HA_STATE_CLASS_TOTAL_INCREASING,
    HA_STATE_CLASS_MAX
} home_assistant_stateClass_e;

typedef enum
{
    HA_COMPONENT_SENSOR,
    HA_COMPONENT_MAX
} home_assistant_component_e;

typedef enum
{
    HA_UOM_WATT,
    HA_UOM_VA,
    HA_UOM_VAR,
    HA_UOM_VOLT,
    HA_UOM_AMP,
    HA_UOM_KWH,
    HA_UOM_HZ,
    HA_UOM_MAX
} home_assistant_unitOfMeasure_e;

/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
/******************************* GLOBALS ********************************/
#endif /* HOME_ASSISTANT_CONSTANTS_H */
