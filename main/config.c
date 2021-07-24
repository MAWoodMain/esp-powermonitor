//
// Created by MattWood on 21/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "config.h"
#include "esp_spiffs.h"
#include "jsmn.h"
/******************************* DEFINES ********************************/
#define CONFIG_SAVE_FILE_BUFFER_LENGTH 512
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
bool config_parseConfigFile();
void config_extractStringField(config_string_field_e field, jsmntok_t *t, char *json);
void config_extractFloatField(config_float_field_e field, jsmntok_t *t, char *json);
/******************************* CONSTANTS ******************************/
static const char *TAG = "CONFIG";
static const char* config_configFilename = "config.json";
static const char* config_jsonStringFieldNames[CONFIG_STRING_FIELD_MAX] =
        {
                [CONFIG_STRING_FIELD_WIFI_SSID]         = "wifi_ssid",
                [CONFIG_STRING_FIELD_WIFI_PASSWORD]     = "wifi_password",
                [CONFIG_STRING_FIELD_MQTT_URI]          = "mqtt_uri",
                [CONFIG_STRING_FIELD_MQTT_USERNAME]     = "mqtt_username",
                [CONFIG_STRING_FIELD_MQTT_PASSWORD]     = "mqtt_password"
        };
static const char* config_jsonFloatFieldNames[CONFIG_FLOAT_FIELD_MAX] =
        {
                [CONFIG_FLOAT_FIELD_V_CAL]      = "cal_v_cal",
                [CONFIG_FLOAT_FIELD_I1_CAL]     = "cal_i1_cal",
                [CONFIG_FLOAT_FIELD_I2_CAL]     = "cal_i2_cal",
                [CONFIG_FLOAT_FIELD_I3_CAL]     = "cal_i3_cal",
                [CONFIG_FLOAT_FIELD_I4_CAL]     = "cal_i4_cal"
        };
/******************************* VARIABLES ******************************/
char config_configSaveFileBuffer[CONFIG_SAVE_FILE_BUFFER_LENGTH];

static char* config_stringFieldValues[CONFIG_STRING_FIELD_MAX] =
        {
                [CONFIG_STRING_FIELD_WIFI_SSID]         = "default_ssid",
                [CONFIG_STRING_FIELD_WIFI_PASSWORD]     = "default_password",
                [CONFIG_STRING_FIELD_MQTT_URI]          = "default_uri",
                [CONFIG_STRING_FIELD_MQTT_USERNAME]     = "default_username",
                [CONFIG_STRING_FIELD_MQTT_PASSWORD]     = "default_password"
        };
static float config_floatFieldValues[CONFIG_FLOAT_FIELD_MAX] =
        {
                [CONFIG_FLOAT_FIELD_V_CAL]      = 1.0f,
                [CONFIG_FLOAT_FIELD_I1_CAL]     = 1.0f,
                [CONFIG_FLOAT_FIELD_I2_CAL]     = 1.0f,
                [CONFIG_FLOAT_FIELD_I3_CAL]     = 1.0f,
                [CONFIG_FLOAT_FIELD_I4_CAL]     = 1.0f
        };
/*************************** PUBLIC FUNCTIONS ***************************/
bool config_init()
{
    bool retVal = true;

    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = false
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        retVal = false;
    }
    else
    {
        size_t total = 0, used = 0;
        ret = esp_spiffs_info(NULL, &total, &used);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
        }

        if(false == config_parseConfigFile())
        {
            if(false == config_save())
            {
                retVal = false;
            }
        }
    }

    return retVal;
}

char* config_getStringField(config_string_field_e field)
{
    char *retVal = NULL;

    if(field < CONFIG_STRING_FIELD_MAX)
    {
        retVal = config_stringFieldValues[field];
    }

    if(retVal == NULL)
    {
        retVal = "";
    }

    return retVal;
}

void config_setStringField(config_string_field_e field, char* newValue)
{
    uint32_t newStringLength = strlen(newValue);

    if(field < CONFIG_STRING_FIELD_MAX)
    {
        free(config_stringFieldValues[field]);
        config_stringFieldValues[field] = calloc(newStringLength + 1, sizeof(char));
        strcpy(config_stringFieldValues[field], newValue);
    }
}

float config_getFloatField(config_float_field_e field)
{
    if(field < CONFIG_FLOAT_FIELD_MAX)
    {
        return config_floatFieldValues[field];
    }
    return 0.0f;
}

void config_setFloatField(config_float_field_e field, float newValue)
{
    if(field < CONFIG_FLOAT_FIELD_MAX)
    {
        config_floatFieldValues[field] = newValue;
    }
}

bool config_save()
{
    bool retVal = false;
    char fileName[64];
    FILE* f;
    uint32_t length = 0;
    memset(config_configSaveFileBuffer, 0, CONFIG_SAVE_FILE_BUFFER_LENGTH);

    sprintf(config_configSaveFileBuffer, "{");
    for(uint32_t idx = 0; idx < CONFIG_STRING_FIELD_MAX; idx++)
    {
        if(idx != 0)
        {
            sprintf(config_configSaveFileBuffer + strlen(config_configSaveFileBuffer), ",");
        }
        sprintf(config_configSaveFileBuffer + strlen(config_configSaveFileBuffer), "\"%s\":\"%s\"", config_jsonStringFieldNames[idx], config_stringFieldValues[idx]);
    }

    for(uint32_t idx = 0; idx < CONFIG_FLOAT_FIELD_MAX; idx++)
    {
        sprintf(config_configSaveFileBuffer + strlen(config_configSaveFileBuffer), ",");
        sprintf(config_configSaveFileBuffer + strlen(config_configSaveFileBuffer), "\"%s\":%f", config_jsonFloatFieldNames[idx], config_floatFieldValues[idx]);
    }
    sprintf(config_configSaveFileBuffer + strlen(config_configSaveFileBuffer), "}");
    ESP_LOGI(TAG, "new config: %s", config_configSaveFileBuffer);

    length = strlen(config_configSaveFileBuffer);

    memset(fileName, 0, sizeof(fileName));
    strcpy(fileName, "/spiffs/");
    strcat(fileName, config_configFilename);

    f = fopen(fileName, "w");
    retVal = length == fwrite(config_configSaveFileBuffer, 1, length, f);

    fclose(f);
    return retVal;
}

/*************************** PRIVATE FUNCTIONS **************************/
bool config_parseConfigFile()
{
    bool retVal = true;
    FILE* f;
    uint32_t fileSize = 0;
    char fileName[64];
    char *fileBuffer = config_configSaveFileBuffer;
    jsmn_parser p;
    jsmntok_t t[128]; /* We expect no more than 128 JSON tokens */
    int32_t tokens = 0;

    memset(t, 0, sizeof(t));
    memset(fileName, 0, sizeof(fileName));
    strcpy(fileName, "/spiffs/");
    strcat(fileName, config_configFilename);
    f = fopen(fileName, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", fileName);
        retVal = false;
    }
    else
    {
        fseek(f, 0L, SEEK_END);
        fileSize = ftell(f);
        fseek(f, 0L, SEEK_SET);
        ESP_LOGI(TAG, "Config file size %d", fileSize);
        //fileBuffer = calloc(fileSize+1, sizeof(char));
        if(fileBuffer != NULL)
        {
            memset(fileBuffer, 0, CONFIG_SAVE_FILE_BUFFER_LENGTH);
            ESP_LOGI(TAG, "Read %u characters", fread(&fileBuffer[0], 1, fileSize, f));
            ESP_LOGI(TAG, "Read file: \"%s\"", fileBuffer);
            fclose(f);

            jsmn_init(&p);
            tokens = (int32_t)jsmn_parse(&p, fileBuffer, fileSize, t, 128);
            if(tokens > 0)
            {
                ESP_LOGI(TAG, "Tokens extracted %d", tokens);
                for(config_string_field_e stringField = 0; stringField < CONFIG_STRING_FIELD_MAX; stringField++)
                {
                    ESP_LOGI(TAG, "Extracting string field %d", stringField);
                    config_extractStringField(stringField, t, fileBuffer);
                }
                for(config_float_field_e floatField = 0; floatField < CONFIG_FLOAT_FIELD_MAX; floatField++)
                {
                    ESP_LOGI(TAG, "Extracting float field %d", floatField);
                    config_extractFloatField(floatField, t, fileBuffer);
                }
            }
            else
            {
                ESP_LOGE(TAG, "JSMN error %d", tokens);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to allocate file buffer");
        }
    }
    return retVal;
}

void config_extractStringField(config_string_field_e field, jsmntok_t *t, char *json)
{
    const char* fieldName = config_jsonStringFieldNames[field];
    uint32_t idx = 0;
    uint32_t parentId = 0;
    uint32_t size = 0;
    while(t[idx].type != JSMN_UNDEFINED)
    {
        if(0 == memcmp(&json[t[idx].start], fieldName, MIN(strlen(fieldName),t[idx].end - t[idx].start)))
        {
            parentId = idx;
            break;
        }

        idx++;
    }

    if(parentId != 0)
    {
        idx = 0;
        while(t[idx].type != JSMN_UNDEFINED)
        {
            if(t[idx].parent == parentId)
            {
                size = t[idx].end - t[idx].start;
                config_stringFieldValues[field] = calloc(size + 1, sizeof(char));
                memcpy(config_stringFieldValues[field], &json[t[idx].start], size);
                ESP_LOGI(TAG, "Extracted string %s, value is %s", fieldName, config_stringFieldValues[field]);
                break;
            }

            idx++;
        }
    }
}

void config_extractFloatField(config_float_field_e field, jsmntok_t *t, char *json)
{
    const char* fieldName = config_jsonFloatFieldNames[field];
    uint32_t idx = 0;
    uint32_t parentId = 0;
    uint32_t size = 0;
    uint8_t buffer[16];
    while(t[idx].type != JSMN_UNDEFINED)
    {
        if(0 == memcmp(&json[t[idx].start], fieldName, MIN(strlen(fieldName),t[idx].end - t[idx].start)))
        {
            parentId = idx;
            break;
        }

        idx++;
    }

    if(parentId != 0)
    {
        idx = 0;
        while(t[idx].type != JSMN_UNDEFINED)
        {
            if(t[idx].parent == parentId)
            {
                size = t[idx].end - t[idx].start;
                memset(buffer, 0, sizeof(buffer));
                memcpy(buffer, &json[t[idx].start], size);
                config_floatFieldValues[field] = strtof((char*)buffer, NULL);
                ESP_LOGI(TAG, "Extracted float %s, value is %f", fieldName, config_floatFieldValues[field]);
                break;
            }

            idx++;
        }
    }
}