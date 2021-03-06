//
// Created by MattWood on 21/07/2021.
//

#ifndef WIFI_H
#define WIFI_H

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "std_includes.h"
/******************************* DEFINES ********************************/
/******************************** ENUMS *********************************/
/****************************** TYPEDEFS ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
bool wifi_init(void);
bool wifi_connect();
bool wifi_switchToSoftAP();
bool wifi_waitForConnection(uint32_t timeoutMs);
/******************************* GLOBALS ********************************/
#endif /* WIFI_H */
