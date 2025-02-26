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
#pragma once

#include "esp_http_server.h"

// Utility lightweight class allowing us to mock these operations when testing the DataStreamer
class EspHttpServerOps {
public:
    static esp_err_t register_uri_handler(httpd_handle_t server, const httpd_uri_t* uri_desc) {
        return httpd_register_uri_handler(server, uri_desc);
    }
    static esp_err_t unregister_uri_handler(httpd_handle_t server, const char* uri, http_method method) {
        return httpd_unregister_uri_handler(server, uri, method);
    }
    static esp_err_t resp_sendstr_chunk(httpd_req_t* req, const char* chunk) {
        return httpd_resp_sendstr_chunk(req, chunk);
    }
    static esp_err_t resp_send_chunk(httpd_req_t* req, const char* chunk, ssize_t size) {
        return httpd_resp_send_chunk(req, chunk, size);
    }
    static esp_err_t resp_send_err(httpd_req_t* req, httpd_err_code_t error, const char* msg) {
        return httpd_resp_send_err(req, error, msg);
    }
    static esp_err_t resp_set_type(httpd_req_t* req, const char* type) {
        return httpd_resp_set_type(req, type);
    }
    static esp_err_t resp_set_status(httpd_req_t* r, const char* status) {
        return httpd_resp_set_status(r, status);
    }
    static esp_err_t resp_set_hdr(httpd_req_t *r, const char *field, const char *value) {
        return httpd_resp_set_hdr(r, field, value);
    }
    static size_t req_get_url_query_len(httpd_req_t *r) {
        return httpd_req_get_url_query_len(r);
    }
    static esp_err_t req_get_url_query_str(httpd_req_t *r, char *buf, size_t buf_len) {
        return httpd_req_get_url_query_str(r, buf, buf_len);
    }
    static esp_err_t query_key_value(const char *qry, const char *key, char *val, size_t val_size) {
        return httpd_query_key_value(qry, key, val, val_size);
    }
};
