//
// Created by MattWood on 21/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include <ctype.h>
#include "web_server.h"
#include "esp_http_server.h"
#include "format.h"
#include "config.h"
/******************************* DEFINES ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
esp_err_t web_server_get_handler(httpd_req_t *req);
esp_err_t web_server_getBatteryHandler(httpd_req_t *req);
esp_err_t web_server_getMacHandler(httpd_req_t *req);
esp_err_t web_server_getPmHandler(httpd_req_t *req);
esp_err_t web_server_getWifiHandler(httpd_req_t *req);
esp_err_t web_server_getMqttHandler(httpd_req_t *req);
esp_err_t web_server_getCalHandler(httpd_req_t *req);
void web_server_urldecode2(char *src);
/******************************* CONSTANTS ******************************/
extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
static const char *TAG = "WEB_SERVER";
/******************************* VARIABLES ******************************/
uint8_t web_server_responseBuffer[256];

httpd_uri_t web_server_uri_get = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = web_server_get_handler,
        .user_ctx = NULL
};

httpd_uri_t web_server_uriGetBattery = {
        .uri      = "/battery",
        .method   = HTTP_GET,
        .handler  = web_server_getBatteryHandler,
        .user_ctx = NULL
};

httpd_uri_t web_server_uriGetMac = {
        .uri      = "/mac",
        .method   = HTTP_GET,
        .handler  = web_server_getMacHandler,
        .user_ctx = NULL
};

httpd_uri_t web_server_uriGetPm = {
        .uri      = "/pm",
        .method   = HTTP_GET,
        .handler  = web_server_getPmHandler,
        .user_ctx = NULL
};

httpd_uri_t web_server_uriGetWifi = {
        .uri      = "/wifi",
        .method   = HTTP_GET,
        .handler  = web_server_getWifiHandler,
        .user_ctx = NULL
};

httpd_uri_t web_server_uriGetMqtt = {
        .uri      = "/mqtt",
        .method   = HTTP_GET,
        .handler  = web_server_getMqttHandler,
        .user_ctx = NULL
};

httpd_uri_t web_server_uriGetCal = {
        .uri      = "/cal",
        .method   = HTTP_GET,
        .handler  = web_server_getCalHandler,
        .user_ctx = NULL
};
/*************************** PUBLIC FUNCTIONS ***************************/

bool web_server_init(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &web_server_uri_get);
        httpd_register_uri_handler(server, &web_server_uriGetBattery);
        httpd_register_uri_handler(server, &web_server_uriGetMac);
        httpd_register_uri_handler(server, &web_server_uriGetPm);
        httpd_register_uri_handler(server, &web_server_uriGetWifi);
        httpd_register_uri_handler(server, &web_server_uriGetMqtt);
        httpd_register_uri_handler(server, &web_server_uriGetCal);
    }

    /* TODO: check for errors */
    return true;
}

/*************************** PRIVATE FUNCTIONS **************************/

esp_err_t web_server_get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    //const char resp[] = "URI GET Response";
    httpd_resp_send(req, (char *)upload_script_start, (ssize_t)(upload_script_end-upload_script_start));
    return ESP_OK;
}

esp_err_t web_server_getBatteryHandler(httpd_req_t *req)
{
    memset(web_server_responseBuffer, 0, sizeof(web_server_responseBuffer));
    sprintf((char*) web_server_responseBuffer, "{\"status\":\"%s\",\"voltage\":%.02f,\"uptime\":%lu}",
            format_renderBatteryState(battery_getState()),
            battery_getVoltage(),
            pdTICKS_TO_MS(xTaskGetTickCount())/1000l),
    httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t web_server_getMacHandler(httpd_req_t *req)
{
    uint8_t mac[6];
    if(ESP_OK == esp_read_mac(mac, ESP_MAC_WIFI_STA))
    {
        memset(web_server_responseBuffer, 0, sizeof(web_server_responseBuffer));
        sprintf((char*) web_server_responseBuffer, "{\"MACAddress\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        httpd_resp_send_500(req);
    }
    return ESP_OK;
}

esp_err_t web_server_getPmHandler(httpd_req_t *req)
{
    power_monitor_measurement_t measurement = power_monitor_getLastMeasurement();
    memset(web_server_responseBuffer, 0, sizeof(web_server_responseBuffer));
    sprintf((char*) web_server_responseBuffer, "{\"condition\":\"%s\",\"vrms\":%.02f,\"frequency\":%.02f,\"irms\":[%.03f,%.03f,%.03f,%.03f],\"varms\":[%.03f,%.03f,%.03f,%.03f],\"prms\":[%.03f,%.03f,%.03f,%.03f]}",
            format_renderPowerMonitorCondition(measurement.condition),
            measurement.rmsV,
            measurement.frequency, measurement.rmsI[0], measurement.rmsI[1], measurement.rmsI[2],measurement.rmsI[3],
            measurement.rmsVA[0], measurement.rmsVA[1], measurement.rmsVA[2],measurement.rmsVA[3],
            measurement.rmsP[0], measurement.rmsP[1], measurement.rmsP[2],measurement.rmsP[3]);
    httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


esp_err_t web_server_getWifiHandler(httpd_req_t *req)
{
    uint32_t buffLen = sizeof(web_server_responseBuffer);
    memset(web_server_responseBuffer, 0, sizeof(web_server_responseBuffer));
    httpd_req_get_url_query_str(req, (char*) &web_server_responseBuffer[buffLen/2], buffLen);

    httpd_query_key_value((char*) &web_server_responseBuffer[buffLen/2], "ssid", (char*) web_server_responseBuffer, (buffLen/2)-1 );
    web_server_urldecode2((char*) web_server_responseBuffer);
    config_setStringField(CONFIG_STRING_FIELD_WIFI_SSID, (char*) web_server_responseBuffer);

    memset(web_server_responseBuffer, 0, (buffLen/2)-1);
    httpd_query_key_value((char*) &web_server_responseBuffer[buffLen/2], "password", (char*) web_server_responseBuffer, (buffLen/2)-1 );
    web_server_urldecode2((char*) web_server_responseBuffer);
    config_setStringField(CONFIG_STRING_FIELD_WIFI_PASSWORD, (char*) web_server_responseBuffer);

    memset(web_server_responseBuffer, 0, sizeof(web_server_responseBuffer));

    if(true == config_save())
    {
        sprintf((char*) web_server_responseBuffer, "{\"msg\":\"New details saved, restarting to apply\",\"success\":true}");
        httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
        vTaskDelay( pdMS_TO_TICKS(100));
        esp_restart();
    }
    else
    {
        sprintf((char*) web_server_responseBuffer, "{\"msg\":\"Failed to save new settings\",\"success\":false}}");
        httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
    }

    return ESP_OK;
}

esp_err_t web_server_getMqttHandler(httpd_req_t *req)
{
    uint32_t buffLen = sizeof(web_server_responseBuffer);
    memset(web_server_responseBuffer, 0, sizeof(web_server_responseBuffer));
    httpd_req_get_url_query_str(req, (char*) &web_server_responseBuffer[buffLen/2], buffLen);

    httpd_query_key_value((char*) &web_server_responseBuffer[buffLen/2], "uri", (char*) web_server_responseBuffer, (buffLen/2)-1 );
    web_server_urldecode2((char*) web_server_responseBuffer);
    ESP_LOGI(TAG, "URI: %s", (char*) web_server_responseBuffer);
    config_setStringField(CONFIG_STRING_FIELD_MQTT_URI, (char*) web_server_responseBuffer);

    memset(web_server_responseBuffer, 0, (buffLen/2)-1);
    httpd_query_key_value((char*) &web_server_responseBuffer[buffLen/2], "username", (char*) web_server_responseBuffer, (buffLen/2)-1 );
    web_server_urldecode2((char*) web_server_responseBuffer);
    ESP_LOGI(TAG, "USER: %s", (char*) web_server_responseBuffer);
    config_setStringField(CONFIG_STRING_FIELD_MQTT_USERNAME, (char*) web_server_responseBuffer);

    memset(web_server_responseBuffer, 0, (buffLen/2)-1);
    httpd_query_key_value((char*) &web_server_responseBuffer[buffLen/2], "password", (char*) web_server_responseBuffer, (buffLen/2)-1 );
    web_server_urldecode2((char*) web_server_responseBuffer);
    ESP_LOGI(TAG, "PASS: %s", (char*) web_server_responseBuffer);
    config_setStringField(CONFIG_STRING_FIELD_MQTT_PASSWORD, (char*) web_server_responseBuffer);

    memset(web_server_responseBuffer, 0, sizeof(web_server_responseBuffer));

    if(true == config_save())
    {
        sprintf((char*) web_server_responseBuffer, "{\"msg\":\"New details saved, restarting to apply\",\"success\":true}");
        httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
        vTaskDelay( pdMS_TO_TICKS(100));
        esp_restart();
    }
    else
    {
        sprintf((char*) web_server_responseBuffer, "{\"msg\":\"Failed to save new settings\",\"success\":false}}");
        httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
    }

    return ESP_OK;
}

esp_err_t web_server_getCalHandler(httpd_req_t *req)
{
    uint32_t channel = 0;
    float cal = 0;
    uint32_t buffLen = sizeof(web_server_responseBuffer);
    memset(web_server_responseBuffer, 0, sizeof(web_server_responseBuffer));
    httpd_req_get_url_query_str(req, (char*) &web_server_responseBuffer[buffLen/2], buffLen);

    httpd_query_key_value((char*) &web_server_responseBuffer[buffLen/2], "channel", (char*) web_server_responseBuffer, (buffLen/2)-1 );
    web_server_urldecode2((char*) web_server_responseBuffer);
    ESP_LOGI(TAG, "raw: %s", (char*) web_server_responseBuffer);
    channel = strtol((char*) web_server_responseBuffer, NULL, 10);

    memset(web_server_responseBuffer, 0, (buffLen/2)-1);
    httpd_query_key_value((char*) &web_server_responseBuffer[buffLen/2], "factor", (char*) web_server_responseBuffer, (buffLen/2)-1 );
    web_server_urldecode2((char*) web_server_responseBuffer);
    ESP_LOGI(TAG, "raw: %s", (char*) web_server_responseBuffer);
    cal = strtof((char*) web_server_responseBuffer, NULL);

    switch (channel)
    {
        case 0:
            config_setFloatField(CONFIG_FLOAT_FIELD_V_CAL, cal);
            break;
        case 1:
            config_setFloatField(CONFIG_FLOAT_FIELD_I1_CAL, cal);
            break;
        case 2:
            config_setFloatField(CONFIG_FLOAT_FIELD_I2_CAL, cal);
            break;
        case 3:
            config_setFloatField(CONFIG_FLOAT_FIELD_I3_CAL, cal);
            break;
        case 4:
            config_setFloatField(CONFIG_FLOAT_FIELD_I4_CAL, cal);
            break;
        default:
            break;
    }

    if(true == config_save())
    {
        sprintf((char*) web_server_responseBuffer, "{\"msg\":\"New details saved, restarting to apply\",\"success\":true}");
        httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
        vTaskDelay( pdMS_TO_TICKS(100));
        esp_restart();
    }
    else
    {
        sprintf((char*) web_server_responseBuffer, "{\"msg\":\"Failed to save new settings\",\"success\":false}}");
        httpd_resp_send(req, (char*)web_server_responseBuffer, HTTPD_RESP_USE_STRLEN);
    }

    return ESP_OK;
}

void web_server_urldecode2(char *src)
{
    char *sSource = src;
    char *sDest = src;
    int nLength;
    for (nLength = 0; *sSource; nLength++) {
        if (*sSource == '%' && sSource[1] && sSource[2] && isxdigit(sSource[1]) && isxdigit(sSource[2])) {
            sSource[1] -= sSource[1] <= '9' ? '0' : (sSource[1] <= 'F' ? 'A' : 'a')-10;
            sSource[2] -= sSource[2] <= '9' ? '0' : (sSource[2] <= 'F' ? 'A' : 'a')-10;
            sDest[nLength] = 16 * sSource[1] + sSource[2];
            sSource += 3;
            continue;
        }
        sDest[nLength] = *sSource++;
    }
    sDest[nLength] = '\0';
}