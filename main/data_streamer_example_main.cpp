/*
 * Copyright 2025 OIST
 * Copyright 2025 fold ecosystemics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "lwip/sys.h"
#include "vfs_streamer.h"
#include "esp_https_server.h"
#include "sdkconfig.h"


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
// Counter for wifi connection retries
static int s_retry_num = 0;
// mDNS hostname
constexpr std::string_view HOSTNAME = CONFIG_EXAMPLE_DATA_STREAMER_HOSTNAME;

// Certificate data (convert your certificates to C strings)
extern const uint8_t server_cert_pem_start[] asm("_binary_server_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_server_crt_end");
extern const uint8_t server_key_pem_start[] asm("_binary_server_key_start");
extern const uint8_t server_key_pem_end[] asm("_binary_server_key_end");
extern const uint8_t ca_cert_pem_start[] asm("_binary_ca_crt_start");
extern const uint8_t ca_cert_pem_end[] asm("_binary_ca_crt_end");

static const char *TAG = "main";


/**
 * @brief Initializes and mounts the SD card over SPI
 *
 * Configures and initializes the SPI bus for SD card communication with the following pins:
 * - MOSI: GPIO42
 * - MISO: GPIO21
 * - SCLK: GPIO39
 * - CS: GPIO45
 * (This example specifically targets the Adafruit Metro ESP32S3 N16R8)
 *
 * Mounts the SD card at "/sdcard" using FAT filesystem.
 *
 * @note The function will panic (ESP_ERROR_CHECK) if mounting fails
 */
void setup_sd_card () {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    const char mount_point[] = "/sdcard";

    // SPI bus configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = GPIO_NUM_42,
        .miso_io_num = GPIO_NUM_21,
        .sclk_io_num = GPIO_NUM_39,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4092,
    };

    // Initialize the SPI bus
    auto ret = spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    ESP_LOGI(TAG, "Spi bus init: %d", ret);

    // SD Card configuration for SPI mode
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_45;
    slot_config.host_id = SPI3_HOST;

    sdmmc_card_t* card;
    ESP_ERROR_CHECK(esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card));
    ESP_LOGI(TAG, "SD card mounted");
}

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_EXAMPLE_DATA_STREAMER_RETRY_MAX) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


/**
 * @brief Initializes WiFi in station mode
 *
 * Configures and starts WiFi with the following:
 * - SSID and password from menuconfig
 * - WPA2-PSK authentication by default
 * - WPA3 compatibility mode enabled
 *
 * Waits for either successful connection or connection failure before returning.
 * Connection status is indicated through log messages.
 *
 * @note WiFi credentials must be configured through menuconfig:
 *       CONFIG_EXAMPLE_DATA_STREAMER_WIFI_SSID
 *       CONFIG_EXAMPLE_DATA_STREAMER_WIFI_PASS
 */
void wifi_init_sta() {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        nullptr,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        nullptr,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_EXAMPLE_DATA_STREAMER_WIFI_SSID,
            .password = CONFIG_EXAMPLE_DATA_STREAMER_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold = {.authmode = WIFI_AUTH_WPA2_PSK},
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 CONFIG_EXAMPLE_DATA_STREAMER_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                 CONFIG_EXAMPLE_DATA_STREAMER_WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

/**
 * @brief Initializes mDNS service
 *
 * Sets up mDNS with either:
 * - MAC address-based hostname (if HOSTNAME is "MAC")
 * - Custom hostname from HOSTNAME constant
 *
 * Advertises HTTPS service on port 443.
 *
 * @note When using MAC as hostname, appropriate SSL certificates must be in place
 */
void setup_mdns() {
    char hostname[32];
    ESP_ERROR_CHECK(mdns_init());
    if constexpr (std::string_view(HOSTNAME) == "MAC") {
        ESP_LOGW(TAG, "Using MAC as hostname. This requires an appropriate server cert to be in place.");
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(hostname, sizeof(hostname), "esp-%02x%02x%02x%02x%02x%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        snprintf(hostname, sizeof(hostname), "%s", HOSTNAME.data());
    }
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set(hostname));
    ESP_LOGI(TAG, "mDNS hostname set to: %s", hostname);
    ESP_ERROR_CHECK(mdns_service_add(hostname, "_https", "_tcp", 443, nullptr, 0));
}


/**
 * @brief Initializes HTTPS server with streaming endpoints
 *
 * Sets up an HTTPS server with:
 * - SSL/TLS support using provided certificates
 * - Increased stack size (20000 bytes)
 * - Optional file streaming endpoint (/file_stream)
 * - Optional directory streaming endpoint (/dir_stream)
 *
 * Endpoints are created based on menuconfig settings:
 * - CONFIG_EXAMPLE_DATA_STREAMER_FILE_PATH
 * - CONFIG_EXAMPLE_DATA_STREAMER_DIR_PATH
 */
void setup_http_server() {
    httpd_handle_t server = nullptr;
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.stack_size = 20000;

    // Load certificates
    conf.servercert = server_cert_pem_start;
    conf.servercert_len = server_cert_pem_end - server_cert_pem_start;
    conf.prvtkey_pem = server_key_pem_start;
    conf.prvtkey_len = server_key_pem_end - server_key_pem_start;
    conf.cacert_pem = ca_cert_pem_start;
    conf.cacert_len = ca_cert_pem_end - ca_cert_pem_start;

    ESP_LOGI(TAG, "Starting HTTPS Server on port: '%d'", conf.port_secure);
    ESP_ERROR_CHECK(httpd_ssl_start(&server, &conf));

    if (strlen(CONFIG_EXAMPLE_DATA_STREAMER_FILE_PATH) > 0) {
        ESP_LOGI(TAG, "Creating file_stream endpoint");
        static auto file_streamer = data_streamer::VFSFileStreamer("/sdcard/" CONFIG_EXAMPLE_DATA_STREAMER_FILE_PATH);
        ESP_ERROR_CHECK(file_streamer.bind(server, "/file_stream:443", HTTP_GET));
    }

    if (strlen(CONFIG_EXAMPLE_DATA_STREAMER_DIR_PATH) > 0) {
        ESP_LOGI(TAG, "Creating dir_stream endpoint, bound to /sdcard/" CONFIG_EXAMPLE_DATA_STREAMER_DIR_PATH);
        static auto dir_streamer = data_streamer::VFSFlatDirStreamer("/sdcard/" CONFIG_EXAMPLE_DATA_STREAMER_DIR_PATH);
        ESP_ERROR_CHECK(dir_streamer.bind(server, "/dir_stream:443", HTTP_GET));
    }
}

extern "C" void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize sd card, wifi, mDNS and http services
    setup_sd_card();
    wifi_init_sta();
    setup_mdns();
    setup_http_server();

    ESP_LOGI(TAG, "Initialization complete");
    while (1) {  // loop forever
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
