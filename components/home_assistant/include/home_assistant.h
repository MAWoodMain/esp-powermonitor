//
// Created by MattWood on 12/09/2021.
//

#ifndef HOME_ASSISTANT_H
#define HOME_ASSISTANT_H

/**************************** LIB INCLUDES ******************************/
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
/**************************** USER INCLUDES *****************************/
#include "home_assistant_constants.h"
/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/
typedef enum
{
    HA_TYPE_DOUBLE,
    HA_TYPE_UINT,
    HA_TYPE_INT,
    HA_TYPE_BOOL,
} home_assistant_valueType_e;
/****************************** TYPEDEFS ********************************/
typedef bool      (* home_assistant_mqttImpl_t)(char* topic, char* payload, bool retain);

typedef union
{
    double d;
    uint32_t ui;
    int32_t i;
    bool b;
} home_assistant_valueType_t;


typedef struct
{
    char* deviceName;
    char* model;
    char* manufacturer;
    uint8_t macAddress[6];
    home_assistant_component_e componentType;
    bool useCombinedPayload;
    home_assistant_mqttImpl_t mqttImplementation;
} home_assistant_deviceConfig_t;

typedef struct
{
    char* deviceName;
    char* deviceNameSafe;
    char* model;
    char* manufacturer;
    uint8_t macAddress[6];
    home_assistant_component_e componentType;
    bool useCombinedPayload;
    home_assistant_mqttImpl_t mqttImplementation;
} home_assistant_device_t;

typedef struct
{
    char *name;
    home_assistant_unitOfMeasure_e unitOfMeasurement;
    home_assistant_deviceClass_e class;
    home_assistant_stateClass_e stateClass;
    uint8_t decimalPlaces;
    home_assistant_valueType_e valueType;
    home_assistant_valueType_t initialValue;
} home_assistant_deviceChannelConfig_t;

typedef struct
{
    char *name;
    char *nameSafe;
    home_assistant_unitOfMeasure_e unitOfMeasurement;
    home_assistant_deviceClass_e class;
    home_assistant_stateClass_e stateClass;
    uint8_t decimalPlaces;
    home_assistant_valueType_e valueType;
    home_assistant_valueType_t latestValue;
} home_assistant_deviceChannel_t;
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
bool home_assistant_init(home_assistant_deviceConfig_t config);
bool home_assistant_sendDiscoveryMessage();
home_assistant_deviceChannel_t* home_assistant_createChannel(home_assistant_deviceChannelConfig_t config);
bool home_assistant_submitDoubleValue(home_assistant_deviceChannel_t* channel, double value);
bool home_assistant_publishValues();
/******************************* GLOBALS ********************************/
#endif /* HOME_ASSISTANT_H */

