idf_component_register(
        SRCS "data_streamer_example_main.cpp"
        INCLUDE_DIRS "."
        REQUIRES
        "fatfs"
        "spiffs"
        "nvs_flash"
        "esp_wifi"
        "mdns"
        "esp_http_server"
        "esp_https_server"
        "data_streamer"
        EMBED_TXTFILES
        "certs/server.crt"
        "certs/server.key"
        "certs/ca.crt"
)
