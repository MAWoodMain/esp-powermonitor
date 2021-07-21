//
// Created by MattWood on 16/07/2021.
//

#ifndef POWER_MONITOR_H
#define POWER_MONITOR_H

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/

#include "std_includes.h"

/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/

typedef enum
{
    PM_CONDITION_OK,
    PM_CONDITION_NO_VOLTAGE,
    PM_CONDITION_BAD_FREQUENCY
} power_monitor_measurement_condition_e;
/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/

typedef struct
{
    /* Measurement condition */
    power_monitor_measurement_condition_e condition;
    /* Voltage RMS */
    float rmsV;
    /* Average frequency */
    float frequency;
    /* Current RMS */
    float rmsI[CONFIG_PM_SAMPLING_CHANNEL_COUNT];
    /* Real power RMS */
    float rmsP[CONFIG_PM_SAMPLING_CHANNEL_COUNT];
    /* Apparent power RMS */
    float rmsVA[CONFIG_PM_SAMPLING_CHANNEL_COUNT];
    /* Peak instantaneous power */
    float peakP[CONFIG_PM_SAMPLING_CHANNEL_COUNT];
    /* This is the time since boot in ms for time referencing */
    uint32_t sampleTimestamp;
} power_monitor_measurement_t;
/************************** FUNCTION PROTOTYPES *************************/

bool power_monitor_init(void);

MessageBufferHandle_t power_monitor_getOutputMessageHandle(void);

/******************************* GLOBALS ********************************/

#endif /* POWER_MONITOR_H */
