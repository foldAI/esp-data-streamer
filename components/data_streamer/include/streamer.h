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

#include <vector>
#include <ranges>
#include "concepts.h"
#include "server_ops.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"


namespace data_streamer {

// Max size for URL query parameters
constexpr size_t MAX_URL_PARAM_SIZE = 128;

// Helper type trait for static_assert
template<typename T>
constexpr bool always_false = false;

/**
 * @brief HTTP streaming handler for chunkable data sources
 *
 * DataStreamer provides an HTTP endpoint for streaming data from sources that implement
 * either the Chunkable or IterableOfChunkables concept. It supports both single-item
 * and multi-item streaming with chunked transfer encoding.
 *
 * Features:
 * - Single item streaming (for Chunkable types)
 * - Directory/collection streaming (for IterableOfChunkables types)
 * - Range-based filtering using 'from' and 'to' query parameters
 * - Chunked transfer encoding
 *
 * @tparam T The data source type (must satisfy Chunkable or IterableOfChunkables)
 * @tparam ServerOps Server operations interface (defaults to EspHttpServerOps)
 *
 * Example usage for single file:
 * @code
 * auto streamer = DataStreamer<FileChunker>("/path/to/file");
 * streamer.bind(server, "/stream", HTTP_GET);
 * @endcode
 *
 * Example usage for directory:
 * @code
 * auto streamer = DataStreamer<DirIterable>("/path/to/dir");
 * streamer.bind(server, "/stream", HTTP_GET);
 * // Access with range: GET /stream?from=file1.txt&to=file9.txt
 * @endcode
 */
template <typename T, typename ServerOps = EspHttpServerOps>
    requires(Chunkable<T> || IterableOfChunkables<T>)
class DataStreamer {
public:
    /**
     * @brief Constructs a DataStreamer for the given path
     *
     * @param vfs_path Path to the data source (file or directory)
     */
    explicit DataStreamer(std::string_view vfs_path)
    : vfs_path{vfs_path} {}

    ~DataStreamer() {
        unbind();
    }

    /**
     * @brief Binds the streamer to an HTTP server endpoint
     *
     * @param server HTTP server handle
     * @param uri Endpoint URI
     * @param method HTTP method (typically HTTP_GET)
     * @return esp_err_t ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t bind(httpd_handle_t server, const std::string &uri, http_method method) {
        if (!server) {
            ESP_LOGE(TAG, "Null server handle");
            return ESP_FAIL;
        }
        this->srv = server;
        this->uri = uri;
        this->method = method;
        httpd_uri_t streaming_endpoint = {
            .uri       = uri.c_str(),
            .method    = HTTP_GET,
            .handler   = &DataStreamer::handler_wrapper,
            .user_ctx  = this
        };
        return ServerOps::register_uri_handler(server, &streaming_endpoint);
    }

    /**
     * @brief Unbinds the streamer from the HTTP server
     *
     * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_STATE if not bound
     */
    esp_err_t unbind() {
        if (srv == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        return ServerOps::unregister_uri_handler(srv, uri.c_str(), method);
    }

    /**
     * @brief HTTP handler callback wrapper
     *
     * Static wrapper that routes the HTTP request to the appropriate instance.
     *
     * @param req HTTP request handle
     * @return esp_err_t ESP_OK on success, ESP_FAIL on error
     */
    static esp_err_t handler_wrapper(httpd_req_t* req) {
        // NOTE: unbind() is called at destruction, so instance will always be valid when this
        //       callback is called
        auto* instance = static_cast<DataStreamer*>(req->user_ctx);
        return instance->handler(req);
    }

private:

   /**
    * @brief Handles streaming for Chunkable types
    *
    * Sets up appropriate headers and streams the content as a single file.
    *
    * @param req HTTP request handle
    * @param chunk_provider The Chunkable instance
    * @return esp_err_t ESP_OK on success, ESP_FAIL on error
    */
    esp_err_t handle_chunkable(httpd_req_t *req, T &chunk_provider) {
        ServerOps::resp_set_status(req, HTTPD_200);
        ServerOps::resp_set_type(req, "application/octet-stream");
        auto content_disposition = std::string("attachment; filename=\"") + std::string(chunk_provider.name()) + std::string("\"");
        ServerOps::resp_set_hdr(req, "Content-Disposition", content_disposition.c_str());
        ServerOps::resp_set_hdr(req, "X-Part-Name", chunk_provider.name().data());
        ESP_LOGD(TAG, "Sending file...");
        return send_chunks(req, chunk_provider);
    }

   /**
    * @brief Handles streaming for IterableOfChunkables types
    *
    * Streams multiple items as a multipart response, with optional range filtering
    * based on 'from' and 'to' query parameters.
    *
    * @param req HTTP request handle
    * @param chunk_provider The IterableOfChunkables instance
    * @return esp_err_t ESP_OK on success, ESP_FAIL on error
    */
    esp_err_t handle_iterable_of_chunkables(httpd_req_t *req, T &chunk_provider) {
        std::optional<std::string> from_param;
        std::optional<std::string> to_param;
        size_t query_len = ServerOps::req_get_url_query_len(req);
        if (query_len > 0) {
            std::vector<char> query_buf(query_len + 1);
            if (ServerOps::req_get_url_query_str(req, query_buf.data(), query_buf.size()) == ESP_OK) {
                char value[MAX_URL_PARAM_SIZE];
                if (ServerOps::query_key_value(query_buf.data(), "from", value, sizeof(value)) == ESP_OK) {
                    from_param = std::string(value);
                }
                if (ServerOps::query_key_value(query_buf.data(), "to", value, sizeof(value)) == ESP_OK) {
                    to_param = std::string(value);
                }
            }
        }
        ServerOps::resp_set_status(req, HTTPD_200);
        auto content_type = std::string("multipart/mixed; boundary=") + std::string(BOUNDARY);
        ServerOps::resp_set_type(req, content_type.c_str());
        ESP_LOGD(TAG, "Sending parts...");
        auto filtered_range = chunk_provider | std::views::filter([&](auto& chunkable) {
            if (from_param && chunkable.name() < *from_param) return false;
            if (to_param && chunkable.name() > *to_param) return false;
            return true;
        });

        esp_err_t ret = ESP_FAIL;
        for (auto &chunkable: filtered_range) {
            ESP_LOGD(TAG, "Sending %s", chunkable.name().data());
            ServerOps::resp_send_chunk(req, "\r\n--", HTTPD_RESP_USE_STRLEN);
            ServerOps::resp_send_chunk(req, BOUNDARY, HTTPD_RESP_USE_STRLEN);
            ServerOps::resp_send_chunk(req, "\r\n", HTTPD_RESP_USE_STRLEN);

            ServerOps::resp_send_chunk(req, "Content-Type: application/octet-stream\r\n", HTTPD_RESP_USE_STRLEN);
            ServerOps::resp_send_chunk(req, "Content-Disposition: attachment;\r\n", HTTPD_RESP_USE_STRLEN);
            ServerOps::resp_send_chunk(req, "X-Part-Name: \"", HTTPD_RESP_USE_STRLEN);
            ServerOps::resp_send_chunk(req, chunkable.name().data(), HTTPD_RESP_USE_STRLEN);
            ServerOps::resp_send_chunk(req, "\"\r\n\r\n", HTTPD_RESP_USE_STRLEN);
            ret = send_chunks(req, chunkable);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send chunks, err %d", ret);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "File sent.");
        }
        // send final boundary
        ServerOps::resp_send_chunk(req, "\r\n--", HTTPD_RESP_USE_STRLEN);
        ServerOps::resp_send_chunk(req, BOUNDARY, HTTPD_RESP_USE_STRLEN);
        ServerOps::resp_send_chunk(req, "--\r\n", HTTPD_RESP_USE_STRLEN);
        ESP_LOGD(TAG, "All parts sent");
        if (chunk_provider.error()) {
            ESP_LOGE(TAG, "Chunk provider error, err %d", chunk_provider.error().value());
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    /**
     * @brief Main request handler
     *
     * Dispatches to either handle_chunkable or handle_iterable_of_chunkables
     * based on the type T.
     *
     * @param req HTTP request handle
     * @return esp_err_t ESP_OK on success, ESP_FAIL on error
     */
    esp_err_t handler(httpd_req_t* req) {
        auto chunk_provider = T(vfs_path);

        if constexpr (Chunkable<T>) {  // don't use multipart
            if (handle_chunkable(req, chunk_provider) != ESP_OK) {
                goto error;
            }
            ESP_LOGD(TAG, "File sent.");
        } else if constexpr (IterableOfChunkables<T>) {  // use multipart
            if (handle_iterable_of_chunkables(req, chunk_provider) != ESP_OK) {
                goto error;
            }
        } else {
            static_assert(always_false<T>, "Type must respect either the Chunkable or IterableOfChunkable concepts");
        }

        // Close chunked transmission by sending empty chunk
        ServerOps::resp_send_chunk(req, nullptr, 0);
        return ESP_OK;

        error:  // GOTO tag
        ServerOps::resp_sendstr_chunk(req, nullptr);
        ServerOps::resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
        return ESP_FAIL;
    }

    /**
     * @brief Streams chunks from a Chunkable source
     *
     * @tparam C Type satisfying Chunkable concept
     * @param req HTTP request handle
     * @param chunker The Chunkable instance
     * @return esp_err_t ESP_OK on success, ESP_FAIL on error
     */
    template<Chunkable C>
    esp_err_t send_chunks(httpd_req_t* req, C &chunker) {
        esp_err_t ret = ESP_OK;
        for (std::span<char> &chunk: chunker) {
            // Send the buffer contents as HTTP response chunk
            ret = ServerOps::resp_send_chunk(req, chunk.data(), chunk.size());
            if (ret != ESP_OK) {
                return ret;
            }
        }
        if (chunker.error()) {
            return ESP_FAIL;
        }
        return ret;
    }

    std::string vfs_path;
    httpd_handle_t srv{};
    std::string uri{};
    http_method method{};
};
}  // namespace data_streamer
