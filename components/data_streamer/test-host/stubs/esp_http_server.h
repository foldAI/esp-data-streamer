#pragma once

#include <cstdint>
#include <iostream>

#include "esp_err.h"
#include "http_parser.h"

#define HTTPD_RESP_USE_STRLEN (-1)

#define HTTPD_DEFAULT_CONFIG() \
{}

typedef httpd_method_t http_method;

typedef enum {
    HTTPD_500_INTERNAL_SERVER_ERROR = 0,
  } httpd_err_code_t;

typedef struct httpd_req {
    void *user_ctx;
} httpd_req_t;

typedef struct httpd_uri {
    const char* uri;                      /*!< URI of the handler */
    httpd_method_t method;                /*!< Method supported by the handler */
    esp_err_t (*handler)(httpd_req_t* r); /*!< The handler function */
    void* user_ctx;                       /*!< User context pointer */
} httpd_uri_t;

typedef struct httpd_config {
    uint16_t server_port;
} httpd_config_t;

typedef void* httpd_handle_t;
typedef esp_err_t (*httpd_err_handler_func_t)(httpd_req_t* req,
                                              httpd_err_code_t error);
inline esp_err_t httpd_register_err_handler(httpd_handle_t handle, httpd_err_code_t error, httpd_err_handler_func_t handler_fn) {return ESP_OK;}
inline esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t* uri_handler) {return ESP_OK;}
inline esp_err_t httpd_req_get_url_query_str(httpd_req_t* r, char* buf, size_t buf_len) {return ESP_OK;}
inline esp_err_t httpd_resp_send(httpd_req_t* r, const char* buf, ssize_t buf_len) {return ESP_OK;}
inline esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t buf_len) {
    std::cout.write(buf, buf_len);
    return ESP_OK;
}
inline esp_err_t httpd_resp_send_err(httpd_req_t* r, httpd_err_code_t error, const char* err_str) {return ESP_OK;}
inline esp_err_t httpd_resp_sendstr_chunk(httpd_req_t *r, const char *str) {return ESP_OK;}
inline esp_err_t httpd_resp_set_hdr(httpd_req_t* r, const char* field, const char* value) {return ESP_OK;}
inline esp_err_t httpd_resp_set_status(httpd_req_t* r, const char* status) {return ESP_OK;}
inline esp_err_t httpd_resp_set_type(httpd_req_t* r, const char* type) {return ESP_OK;}
inline esp_err_t httpd_unregister_uri_handler(httpd_handle_t handle, const char *uri, httpd_method_t method) {return ESP_OK;}
inline size_t httpd_req_get_url_query_len(httpd_req_t* r) {return ESP_OK;}
inline esp_err_t httpd_query_key_value(const char *qry, const char *key, char *val, size_t val_size) {return ESP_OK;}
inline esp_err_t httpd_start(httpd_handle_t* handle, const httpd_config_t* config) {return ESP_OK;}
inline void httpd_stop(httpd_handle_t handle) {}

#define HTTPD_200      "200 OK"                     /*!< HTTP Response 200 */
#define HTTPD_204      "204 No Content"             /*!< HTTP Response 204 */
#define HTTPD_207      "207 Multi-Status"           /*!< HTTP Response 207 */
#define HTTPD_400      "400 Bad Request"            /*!< HTTP Response 400 */
#define HTTPD_404      "404 Not Found"              /*!< HTTP Response 404 */
#define HTTPD_408      "408 Request Timeout"        /*!< HTTP Response 408 */
#define HTTPD_500      "500 Internal Server Error"  /*!< HTTP Response 500 */