#pragma once

#define ESP_OK 0
#define ESP_FAIL -1

#define ESP_ERR 1
#define ESP_ERR_INVALID_STATE       0x103   /*!< Invalid state */

typedef int esp_err_t;

// We throw in the mock, so we can catch in the tests
#define ESP_ERROR_CHECK(x) \
if (x != ESP_OK) {       \
throw 1;               \
}

const char* esp_err_to_name(esp_err_t code);