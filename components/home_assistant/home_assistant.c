//
// Created by MattWood on 12/09/2021.
//

/**************************** LIB INCLUDES ******************************/
#include <ctype.h>
/**************************** USER INCLUDES *****************************/
#include "home_assistant.h"
#include "esp_log.h"
#include "cJSON.h"
/******************************* DEFINES ********************************/
#define HOME_ASSISTANT_BASE_TOPIC "homeassistant"
#define HOME_ASSISTANT_DISCOVERY_TOPIC "%s/%s/%s/config" /* printf args: BASE_TOPIC, COMPONENT_CLASS, OBJECT_ID */
#define HOME_ASSISTANT_MIN_CHANNEL_NAME_LENGTH 3
#define HOME_ASSISTANT_MAX_CHANNEL_NAME_LENGTH 32
#define HOME_ASSISTANT_MIN_DEVICE_NAME_LENGTH 3
#define HOME_ASSISTANT_MAX_DEVICE_NAME_LENGTH 32

#define HOME_ASSISTANT_MAX_TOPIC_LENGTH 100
/***************************** STRUCTURES *******************************/

/************************** FUNCTION PROTOTYPES *************************/
bool home_assistant_sendDiscoveryMessageForChannel(home_assistant_deviceChannel_t* channel);
bool home_assistant_publishCombinedValues();
bool home_assistant_publishChannelValue(home_assistant_deviceChannel_t* channel);
void home_assistant_convertToSafeString(char* input);
/******************************* CONSTANTS ******************************/

static const char *TAG = "Home Assistant";

static const char* home_assistant_deviceClassStrings[HA_CLASS_MAX] =
        {
                [HA_CLASS_HA_CLASS_NONE]        = "None",
                [HA_CLASS_AQI]                  = "aqi",
                [HA_CLASS_BATTERY]              = "battery",
                [HA_CLASS_CARBON_DIOXIDE]       = "carbon_dioxide",
                [HA_CLASS_CARBON_MONOXIDE]      = "carbon_monoxide",
                [HA_CLASS_CURRENT]              = "current",
                [HA_CLASS_ENERGY]               = "energy",
                [HA_CLASS_GAS]                  = "gas",
                [HA_CLASS_HUMIDITY]             = "humidity",
                [HA_CLASS_ILLUMINANCE]          = "illuminance",
                [HA_CLASS_MONETARY]             = "monetary",
                [HA_CLASS_NITROGEN_DIOXIDE]     = "nitrogen_dioxide",
                [HA_CLASS_NITROGEN_MONOXIDE]    = "nitrogen_monoxide",
                [HA_CLASS_NITROUS_OXIDE]        = "nitrous_oxide",
                [HA_CLASS_OZONE]                = "ozone",
                [HA_CLASS_PM1]                  = "pm1",
                [HA_CLASS_PM10]                 = "pm10",
                [HA_CLASS_PM25]                 = "pm25",
                [HA_CLASS_POWER_FACTOR]         = "power_factor",
                [HA_CLASS_POWER]                = "power",
                [HA_CLASS_PRESSURE]             = "pressure",
                [HA_CLASS_SIGNAL_STRENGTH]      = "signal_strength",
                [HA_CLASS_SULPHUR_DIOXIDE]      = "sulphur_dioxide",
                [HA_CLASS_TEMPERATURE]          = "temperature",
                [HA_CLASS_TIMESTAMP]            = "timestamp",
                [HA_CLASS_VOC]                  = "volatile_organic_compounds",
                [HA_CLASS_VOLTAGE]              = "voltage"
        };
static const char* home_assistant_stateClassStrings[HA_STATE_CLASS_MAX] =
        {
                [HA_STATE_CLASS_MEASUREMENT]        = "measurement",
                [HA_STATE_CLASS_TOTAL_INCREASING]   = "total_increasing",
        };
static const char* home_assistant_deviceComponentStrings[HA_COMPONENT_MAX] =
        {
                [HA_COMPONENT_SENSOR]          = "sensor",
        };
static const char* home_assistant_UnitOfMeasureStrings[HA_UOM_MAX] =
        {
                [HA_UOM_WATT]                   = "W",
                [HA_UOM_VA]                     = "VA",
                [HA_UOM_VAR]                    = "VAR",
                [HA_UOM_VOLT]                   = "V",
                [HA_UOM_KWH]                    = "kWh",
                [HA_UOM_HZ]                     = "Hz",
                [HA_UOM_AMP]                    = "A",
        };
/******************************* VARIABLES ******************************/

static home_assistant_device_t ha_device;
static home_assistant_deviceChannel_t *ha_channels[CONFIG_HOME_ASSISTANT_MAX_CHANNELS];

/*************************** PUBLIC FUNCTIONS ***************************/

bool home_assistant_init(home_assistant_deviceConfig_t config)
{
    bool retVal = false;

    do
    {
        if( strlen(config.deviceName) > HOME_ASSISTANT_MAX_DEVICE_NAME_LENGTH)
        {
            ESP_LOGE(TAG, "Provided channel name is too long");
            break;
        }

        if( strlen(config.deviceName) < HOME_ASSISTANT_MIN_DEVICE_NAME_LENGTH)
        {
            ESP_LOGE(TAG, "Provided channel name is too short");
            break;
        }

        /* allocate the name string */
        ha_device.deviceName = malloc( strlen(config.deviceName) + 1);
        ha_device.deviceNameSafe = malloc( strlen(config.deviceName) + 1);
        if((ha_device.deviceName == NULL) || (ha_device.deviceNameSafe == NULL))
        {
            ESP_LOGE(TAG, "Could not allocate memory for a device name");
            break;
        }
        strcpy(ha_device.deviceName, config.deviceName);
        strcpy(ha_device.deviceNameSafe, config.deviceName);
        home_assistant_convertToSafeString(ha_device.deviceNameSafe);

        ha_device.manufacturer = malloc( strlen(config.manufacturer) + 1);
        ha_device.model = malloc( strlen(config.model) + 1);
        if((ha_device.manufacturer == NULL) || (ha_device.model == NULL))
        {
            ESP_LOGE(TAG, "Could not allocate memory for device manufacturer and model");
            break;
        }
        /* copy it into place */
        strcpy(ha_device.manufacturer, config.manufacturer);
        strcpy(ha_device.model, config.model);

        ha_device.useCombinedPayload = config.useCombinedPayload;
        ha_device.componentType = config.componentType;
        memcpy(ha_device.macAddress, config.macAddress, 6);
        ha_device.mqttImplementation = config.mqttImplementation;

        retVal = true;
    } while (false);

    return retVal;
}

bool home_assistant_sendDiscoveryMessage()
{
    bool retVal = true;
    ESP_LOGI(TAG, "Sending discovery messages");
    for(uint8_t channelIdx = 0; channelIdx < CONFIG_HOME_ASSISTANT_MAX_CHANNELS; channelIdx++)
    {
        if(ha_channels[channelIdx] != NULL)
        {
            ESP_LOGI(TAG, "Sending discovery message for channel %d", channelIdx);
            if(false == home_assistant_sendDiscoveryMessageForChannel(ha_channels[channelIdx]))
            {
                retVal = false;
                break;
            }
        }
    }
    return retVal;
}

home_assistant_deviceChannel_t* home_assistant_createChannel(home_assistant_deviceChannelConfig_t config)
{
    uint8_t idx = 0;
    ESP_LOGI(TAG, "Creating channel %s", config.name);
    home_assistant_deviceChannel_t* channel = ha_channels[0];
    do
    {
        /* check last allowable slot is clear */
        if(ha_channels[CONFIG_HOME_ASSISTANT_MAX_CHANNELS-1] != NULL)
        {
            ESP_LOGE(TAG, "No spare channel slots left, increase HOME_ASSISTANT_MAX_CHANNELS in menuconfig");
            break;
        }

        /* find the earliest free slot */
        while(ha_channels[idx] != NULL)
        {
            idx++;
        }
        ESP_LOGI(TAG, "Selected channel: %d", idx);

        /* allocate it */
        channel = malloc( sizeof(home_assistant_deviceChannel_t));
        ha_channels[idx] = channel;

        if(channel == NULL)
        {
            ESP_LOGE(TAG, "Could not allocate memory for a new channel");
            break;
        }

        if(config.name == NULL)
        {
            ESP_LOGE(TAG, "Provided channel name is NULL");
            break;
        }

        if( strlen(config.name) > HOME_ASSISTANT_MAX_CHANNEL_NAME_LENGTH)
        {
            ESP_LOGE(TAG, "Provided channel name is too long");
            break;
        }

        if( strlen(config.name) < HOME_ASSISTANT_MIN_CHANNEL_NAME_LENGTH)
        {
            ESP_LOGE(TAG, "Provided channel name is too short");
            break;
        }

        /* allocate the name string */
        channel->name = malloc( strlen(config.name) + 1);
        channel->nameSafe = malloc( strlen(config.name) + 1);
        if((channel->name == NULL) || (channel->nameSafe == NULL))
        {
            ESP_LOGE(TAG, "Could not allocate memory for a new channel name");
            break;
        }

        /* copy it into place */
        strcpy(channel->name, config.name);
        strcpy(channel->nameSafe, config.name);
        home_assistant_convertToSafeString(channel->nameSafe);

        /* copy non-dynamic values over to the final location */
        channel->class = config.class;
        channel->unitOfMeasurement = config.unitOfMeasurement;
        channel->decimalPlaces = config.decimalPlaces;
        channel->latestValue = config.initialValue;
        channel->valueType = config.valueType;
        channel->stateClass = config.stateClass;

    } while (false);

    return channel;
}

bool home_assistant_submitDoubleValue(home_assistant_deviceChannel_t* channel, double value)
{
    bool retVal = false;
    if(channel->valueType == HA_TYPE_DOUBLE)
    {
        channel->latestValue.d = value;
        retVal = true;
    }
    return retVal;
}

bool home_assistant_publishValues()
{
    bool retVal = true;
    char tempBuffer[HOME_ASSISTANT_MAX_TOPIC_LENGTH];
    memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
    sprintf(tempBuffer, "%s_%02x%02x%02x%02x%02x%02x/status", ha_device.deviceNameSafe, ha_device.macAddress[0], ha_device.macAddress[1],
            ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5]);
    ha_device.mqttImplementation(tempBuffer, "connected", false);
    if(ha_device.useCombinedPayload == true)
    {
        ESP_LOGI(TAG, "Sending value message");
        home_assistant_publishCombinedValues();
    }
    else
    {
        ESP_LOGI(TAG, "Sending value messages");
        for(uint8_t channelIdx = 0; channelIdx < CONFIG_HOME_ASSISTANT_MAX_CHANNELS; channelIdx++)
        {
            if(ha_channels[channelIdx] != NULL)
            {
                ESP_LOGI(TAG, "Sending value message for channel %d", channelIdx);
                if(false == home_assistant_publishChannelValue(ha_channels[channelIdx]))
                {
                    retVal = false;
                    break;
                }
            }
        }
    }
    return retVal;
}

/*************************** PRIVATE FUNCTIONS **************************/

bool home_assistant_sendDiscoveryMessageForChannel(home_assistant_deviceChannel_t* channel)
{
    bool retVal = true;
    char tempBuffer[HOME_ASSISTANT_MAX_TOPIC_LENGTH];
    char* output;
    cJSON *discoveryPayload = cJSON_CreateObject();

    if(HA_CLASS_HA_CLASS_NONE != channel->class)
    {
        cJSON_AddStringToObject(discoveryPayload, "device_class", home_assistant_deviceClassStrings[channel->class]);
    }
    cJSON_AddStringToObject(discoveryPayload, "name", channel->name);
    cJSON_AddStringToObject(discoveryPayload, "unit_of_measurement", home_assistant_UnitOfMeasureStrings[channel->unitOfMeasurement]);
    cJSON_AddStringToObject(discoveryPayload, "state_class", home_assistant_stateClassStrings[channel->stateClass]);
    memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
    sprintf(tempBuffer, "%s_%02x%02x%02x%02x%02x%02x/status", ha_device.deviceNameSafe, ha_device.macAddress[0], ha_device.macAddress[1],
            ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5]);
    cJSON_AddStringToObject(discoveryPayload, "avty_t", tempBuffer);
    cJSON_AddStringToObject(discoveryPayload, "pl_avail", "connected");
    cJSON_AddStringToObject(discoveryPayload, "pl_not_avail", "disconnected");
    memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
    sprintf(tempBuffer, "%s_%02x%02x%02x%02x%02x%02x_%s", ha_device.deviceNameSafe, ha_device.macAddress[0], ha_device.macAddress[1],
            ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5], channel->nameSafe);
    cJSON_AddStringToObject(discoveryPayload, "unique_id", tempBuffer);
    if(ha_device.useCombinedPayload == true)
    {
        memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
        sprintf(tempBuffer, "%s_%02x%02x%02x%02x%02x%02x/state", ha_device.deviceNameSafe, ha_device.macAddress[0], ha_device.macAddress[1],
                ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5]);
        cJSON_AddStringToObject(discoveryPayload, "state_topic", tempBuffer);
        memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
        sprintf(tempBuffer, "{{value_json.%s}}", channel->nameSafe);
        cJSON_AddStringToObject(discoveryPayload, "value_template", tempBuffer);
    }
    else
    {
        memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
        sprintf(tempBuffer, "%s_%02x%02x%02x%02x%02x%02x_%s/state", ha_device.deviceNameSafe, ha_device.macAddress[0], ha_device.macAddress[1],
                ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5], channel->nameSafe);
        cJSON_AddStringToObject(discoveryPayload, "state_topic", tempBuffer);
    }

    cJSON *deviceSection = cJSON_AddObjectToObject(discoveryPayload, "device");
    if(deviceSection != NULL)
    {
        memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
        sprintf( tempBuffer, "%02x:%02x:%02x:%02x:%02x:%02x", ha_device.macAddress[0], ha_device.macAddress[1],
                 ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5] );
        /*cJSON *connections = cJSON_AddArrayToObject(deviceSection, "connections");
        cJSON *macConnection = cJSON_CreateArray();
        cJSON_AddItemToArray(macConnection, cJSON_CreateString("mac"));
        cJSON_AddItemToArray(macConnection, cJSON_CreateString(tempBuffer));
        cJSON_AddItemToArray(connections, macConnection);*/
        cJSON_AddStringToObject(deviceSection, "identifiers", tempBuffer);
        cJSON_AddStringToObject(deviceSection, "manufacturer", ha_device.manufacturer);
        cJSON_AddStringToObject(deviceSection, "model", ha_device.model);
        cJSON_AddStringToObject(deviceSection, "name", ha_device.deviceName);
    }

    output = cJSON_PrintUnformatted(discoveryPayload);
    memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
    sprintf(tempBuffer, "%s/%s/%s_%02x%02x%02x%02x%02x%02x_%s/config", HOME_ASSISTANT_BASE_TOPIC, home_assistant_deviceComponentStrings[ha_device.componentType], ha_device.deviceNameSafe, ha_device.macAddress[0], ha_device.macAddress[1],
            ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5], channel->nameSafe);
    ESP_LOGI(TAG, "%s -> %s", tempBuffer, output);
    if(false == ha_device.mqttImplementation(tempBuffer, output, true))
    {
        retVal = false;
    }
    cJSON_Delete(discoveryPayload);
    return retVal;
}

bool home_assistant_publishCombinedValues()
{
    bool retVal = true;
    char tempBuffer[HOME_ASSISTANT_MAX_TOPIC_LENGTH];
    char* output;
    cJSON* payload = cJSON_CreateObject();
    for (uint8_t channelIdx = 0; channelIdx < CONFIG_HOME_ASSISTANT_MAX_CHANNELS; channelIdx++)
    {
        if (ha_channels[channelIdx] != NULL)
        {
            switch (ha_channels[channelIdx]->valueType)
            {

                case HA_TYPE_DOUBLE:
                    memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
                    sprintf(tempBuffer, "%.*f", ha_channels[channelIdx]->decimalPlaces, ha_channels[channelIdx]->latestValue.d);
                    cJSON_AddRawToObject(payload, ha_channels[channelIdx]->nameSafe, tempBuffer);
                    break;
                case HA_TYPE_UINT:
                    memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
                    sprintf(tempBuffer, "%u", ha_channels[channelIdx]->latestValue.ui);
                    cJSON_AddRawToObject(payload, ha_channels[channelIdx]->nameSafe, tempBuffer);
                    break;
                case HA_TYPE_INT:
                    memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
                    sprintf(tempBuffer, "%d", ha_channels[channelIdx]->latestValue.i);
                    cJSON_AddRawToObject(payload, ha_channels[channelIdx]->nameSafe, tempBuffer);
                    break;
                case HA_TYPE_BOOL:
                    if(ha_channels[channelIdx]->latestValue.b)
                    {
                        cJSON_AddTrueToObject(payload, ha_channels[channelIdx]->nameSafe);
                    }
                    else
                    {
                        cJSON_AddFalseToObject(payload, ha_channels[channelIdx]->nameSafe);
                    }
                    break;
                default:
                    ESP_LOGE(TAG, "unsupported channel value type: %d", ha_channels[channelIdx]->valueType);
                    retVal = false;
                    break;
            }
        }
    }
    output = cJSON_PrintUnformatted(payload);
    memset(tempBuffer, 0, HOME_ASSISTANT_MAX_TOPIC_LENGTH);
    sprintf(tempBuffer, "%s_%02x%02x%02x%02x%02x%02x/state", ha_device.deviceNameSafe, ha_device.macAddress[0], ha_device.macAddress[1],
            ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5]);
    ESP_LOGI(TAG, "%s -> %s", tempBuffer, output);
    if(false == ha_device.mqttImplementation(tempBuffer, output, false))
    {
        retVal = false;
    }
    cJSON_Delete(payload);
    return retVal;
}

bool home_assistant_publishChannelValue(home_assistant_deviceChannel_t* channel)
{
    bool retVal = true;
    char topicBuffer[HOME_ASSISTANT_MAX_TOPIC_LENGTH];
    char payloadBuffer[16];
    switch (channel->valueType)
    {
        case HA_TYPE_DOUBLE:
            memset( payloadBuffer, 0, 16);
            sprintf( payloadBuffer, "%.*f", channel->decimalPlaces, channel->latestValue.d);
            ESP_LOGI(TAG, "VALUE: %s", payloadBuffer);
            break;
        case HA_TYPE_UINT:
            memset( payloadBuffer, 0, 16);
            sprintf( payloadBuffer, "%u", channel->latestValue.ui);
            break;
        case HA_TYPE_INT:
            memset( payloadBuffer, 0, 16);
            sprintf( payloadBuffer, "%d", channel->latestValue.i);
            break;
        case HA_TYPE_BOOL:
            if(channel->latestValue.b)
            {
                sprintf( payloadBuffer, "true");
            }
            else
            {
                sprintf( payloadBuffer, "false");
            }
            break;
        default:
            ESP_LOGE(TAG, "unsupported channel value type: %d", channel->valueType);
            retVal = false;
            break;
    }
    sprintf(topicBuffer, "%s_%02x%02x%02x%02x%02x%02x_%s/state", ha_device.deviceNameSafe, ha_device.macAddress[0], ha_device.macAddress[1],
            ha_device.macAddress[2], ha_device.macAddress[3], ha_device.macAddress[4], ha_device.macAddress[5], channel->nameSafe);
    ESP_LOGI(TAG, "%s -> %s", topicBuffer, payloadBuffer);
    if(false == ha_device.mqttImplementation(topicBuffer, payloadBuffer, false))
    {
        retVal = false;
    }
    return retVal;
}

void home_assistant_convertToSafeString(char* input)
{
    uint8_t i;
    for(i = 0; input[i] != '\0'; i++)
    {
        input[i] = tolower(input[i]);
        if (input[i] == ' ') input[i] = '_';
    }
}