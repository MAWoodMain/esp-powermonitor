idf_component_register(SRCS "app_main.c" "led.c" "power_monitor.c" "web_server.c" "wifi.c" "mqtt.c" "config.c"
                    INCLUDE_DIRS "includes"
                    EMBED_FILES "upload_script.html")

spiffs_create_partition_image(storage ../filesystem FLASH_IN_PROJECT)