//
// Created by MattWood on 21/07/2021.
//

/**************************** LIB INCLUDES ******************************/
/**************************** USER INCLUDES *****************************/
#include "web_server.h"
#include "esp_http_server.h"
/******************************* DEFINES ********************************/
/***************************** STRUCTURES *******************************/
/************************** FUNCTION PROTOTYPES *************************/
esp_err_t web_server_get_handler(httpd_req_t *req);
/******************************* CONSTANTS ******************************/
extern const unsigned char upload_script_start[] asm("_binary_upload_script_html_start");
extern const unsigned char upload_script_end[]   asm("_binary_upload_script_html_end");
/******************************* VARIABLES ******************************/

httpd_uri_t web_server_uri_get = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = web_server_get_handler,
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
