idf_component_register(SRCS "app_main.c" "led.c" "power_monitor.c" "web_server.c" "wifi.c" "mqtt.c" "config.c" "battery.c" "format.c"
                    INCLUDE_DIRS "includes"
                    EMBED_FILES "upload_script.html"
                    REQUIRES "home_assistant" "nvs_flash" "esp_adc_cal" "esp_http_server" "mqtt" "spiffs" "jsmn")

spiffs_create_partition_image(storage ../filesystem FLASH_IN_PROJECT)