//
// Created by MattWood on 21/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "mqtt.h"
#include "mqtt_client.h"
#include "secrets.h"
/******************************* DEFINES ********************************/
#define MQTT_CONNECTED_BIT BIT0
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void mqtt_log_error_if_nonzero(const char *message, int error_code);
/******************************* CONSTANTS ******************************/
static const char *TAG = "MQTT";
/******************************* VARIABLES ******************************/
static EventGroupHandle_t mqtt_event_group;
static esp_mqtt_client_handle_t mqtt_client;
/*************************** PUBLIC FUNCTIONS ***************************/

bool mqtt_init(void)
{
    mqtt_event_group = xEventGroupCreate();

    esp_mqtt_client_config_t mqtt_cfg = {
            .uri = MQTT_HOSTNAME,
            .username = MQTT_USERNAME,
            .password = MQTT_PASSWORD
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    /* TODO: check for errors */
    return true;
}

bool mqtt_connect()
{
    return ESP_OK == esp_mqtt_client_start(mqtt_client);
}

bool mqtt_waitForConnection(uint32_t timeoutMs)
{
    bool retVal = false;
    EventBits_t bits = xEventGroupWaitBits(mqtt_event_group,
                                           MQTT_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(timeoutMs));

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & MQTT_CONNECTED_BIT) {
        retVal = true;
    }

    return retVal;
}

bool mqtt_send(const char* topic, const char* data, int qos, int retain)
{

    return 0 <= esp_mqtt_client_publish(mqtt_client, topic,data,0,qos,retain);
}

/*************************** PRIVATE FUNCTIONS **************************/

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_PUBLISHED:
#if CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC
            gpio_set_level(PINMAP_TRANSMIT_TIMING, 0);
#endif /* CONFIG_PM_SAMPLING_ENABLE_TIMING_DIAGNOSTIC */
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                mqtt_log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                mqtt_log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                mqtt_log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}


static void mqtt_log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}
