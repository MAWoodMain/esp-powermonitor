//
// Created by MattWood on 21/07/2021.
//

#ifndef MQTT_H
#define MQTT_H

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "std_includes.h"
/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/
/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
bool mqtt_init(void);
bool mqtt_connect();
bool mqtt_waitForConnection(uint32_t timeoutMs);
bool mqtt_send(const char *topic, const char *data, int qos, int retain);
/******************************* GLOBALS ********************************/
#endif /* MQTT_H */
