//
// Created by MattWood on 21/07/2021.
//

#ifndef CONFIG_H
#define CONFIG_H

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "std_includes.h"
/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/
typedef enum
{
    CONFIG_STRING_FIELD_WIFI_SSID,
    CONFIG_STRING_FIELD_WIFI_PASSWORD,
    CONFIG_STRING_FIELD_MQTT_URI,
    CONFIG_STRING_FIELD_MQTT_USERNAME,
    CONFIG_STRING_FIELD_MQTT_PASSWORD,
    CONFIG_STRING_FIELD_MAX
} config_string_field_e;
/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
bool config_init();
char* config_getStringField(config_string_field_e field);
void config_setStringField(config_string_field_e field, char* newValue);
bool config_save();
/******************************* GLOBALS ********************************/
#endif /* CONFIG_H */
