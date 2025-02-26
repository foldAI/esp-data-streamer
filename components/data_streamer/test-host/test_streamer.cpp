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
#include "config.h"
#include "gtest/gtest.h"
#include "streamer.h"
#include "esp_http_server.h"
#include "esp_err.h"

using namespace data_streamer;

template<int CHUNK_SIZE=CHUNK_SIZE>
class DummyChunkable {
public:
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::span<char>;
        using difference_type = long;
        using pointer = const std::span<char>*;
        using reference = std::span<char>&;

        Iterator(): parent(nullptr), is_end(true) {}
        Iterator(DummyChunkable* p, bool end = false)
            : parent(p), is_end(end) {
            if (!is_end) {
                parent->readChunk();
            }
        }
        Iterator& operator++() {
            if (!is_end) {
                parent->readChunk();
                if (parent->cur_chunk.empty() || parent->last_error) {
                    is_end = true;
                }
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        std::span<char>& operator*() const {return parent->cur_chunk;}

        bool operator==(const Iterator& other) const {
            return is_end == other.is_end;
        }

    private:
        DummyChunkable *parent;
        bool is_end;
    };
    using iterator = Iterator;

    static inline std::optional<int> last_error = std::nullopt;

    DummyChunkable(std::string_view path, char fill_value='1', int total_bytes=100):
        path{path},
        entry_total_bytes{total_bytes},
        fill_value{fill_value},
        cur_pos{0} {
        buf.resize(total_bytes);
        std::fill(buf.begin(), buf.end(), fill_value);
    }
    std::optional<int> error() {return last_error;}

    [[nodiscard]] std::string_view name() const {
        return std::string_view(path);
    }

    void readChunk() {
        if (cur_pos == entry_total_bytes) {
            cur_chunk = std::span<char>();
            return;
        }
        if (entry_total_bytes - cur_pos >= CHUNK_SIZE) {
            cur_chunk = std::span(buf.data(), CHUNK_SIZE);
            cur_pos += CHUNK_SIZE;
        } else {
            cur_chunk = std::span(buf.data(), entry_total_bytes - cur_pos);
            cur_pos = entry_total_bytes;
        }
    }

    iterator begin() {
        return Iterator(this, false);
    }

    iterator end() {
        return Iterator(this, true);
    }
private:
    std::string path;
    int entry_total_bytes;
    char fill_value;
    size_t cur_pos;
    std::vector<char> buf;
    std::span<char> cur_chunk;
};

using DummyChunkableCls = DummyChunkable<>;


class DummyIterableOfChunkables {
public:
    using iterator = std::vector<DummyChunkableCls>::iterator;
    using const_iterator = std::vector<DummyChunkableCls>::const_iterator;
    using value_type = DummyChunkableCls;

    static inline std::optional<int> last_error = std::nullopt;

    explicit DummyIterableOfChunkables(std::string_view path) {
        entries.emplace_back(path, '0');
        entries.emplace_back(path, '1');
        entries.emplace_back(path, '2');
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return entries.begin(); }
    [[nodiscard]] constexpr iterator end() noexcept { return entries.end(); }
    std::optional<int> error() { return last_error; }
    const std::string_view get_error_msg() { return {};}

private:
    std::vector<DummyChunkableCls> entries;
};


#define MOCK_STATIC_RETURN(name, params) \
static inline esp_err_t name##_ret = ESP_OK; \
static esp_err_t name params { \
return name##_ret; \
}
struct MockHttpServerOps {
    MOCK_STATIC_RETURN(register_uri_handler, (httpd_handle_t server, const httpd_uri_t* uri_desc))
    MOCK_STATIC_RETURN(unregister_uri_handler, (httpd_handle_t server, const char* uri, http_method method))
    MOCK_STATIC_RETURN(resp_sendstr_chunk, (httpd_req_t* req, const char* chunk))
    MOCK_STATIC_RETURN(resp_send_chunk, (httpd_req_t* req, const char* chunk, ssize_t size))
    MOCK_STATIC_RETURN(resp_send_err, (httpd_req_t* req, httpd_err_code_t error, const char* msg))
    MOCK_STATIC_RETURN(resp_set_type, (httpd_req_t* req, const char* type))
    MOCK_STATIC_RETURN(resp_set_hdr, (httpd_req_t* req, const char* field, const char* value))
    MOCK_STATIC_RETURN(resp_set_status, (httpd_req_t* r, const char* status))
    MOCK_STATIC_RETURN(req_get_url_query_str, (httpd_req_t *r, char *buf, size_t buf_len))
    MOCK_STATIC_RETURN(query_key_value, (const char *qry, const char *key, char *val, size_t val_size))

    static inline size_t req_get_url_query_len_ret = 0;
    static size_t req_get_url_query_len(httpd_req_t *r) { return req_get_url_query_len_ret; }

    static void reset() {
        register_uri_handler_ret = ESP_OK;
        unregister_uri_handler_ret = ESP_OK;
        resp_sendstr_chunk_ret = ESP_OK;
        resp_send_chunk_ret = ESP_OK;
        resp_send_err_ret = ESP_OK;
        resp_set_type_ret = ESP_OK;
    }
};


class StreamerTest : public ::testing::Test {
protected:
    void SetUp() override {
        DummyIterableOfChunkables::last_error = std::nullopt;
        DummyChunkableCls::last_error = std::nullopt;
        MockHttpServerOps::reset();
    }
};

using ChunkableDataStreamer = DataStreamer<DummyChunkableCls, MockHttpServerOps>;
using ChunkableIterDataStreamer = DataStreamer<DummyIterableOfChunkables, MockHttpServerOps>;


TEST_F(StreamerTest, test_bind){
    auto streamer = ChunkableDataStreamer("path");
    httpd_handle_t server = nullptr;
    // Expect failure, as server handle is empty
    EXPECT_EQ(streamer.bind(server, std::string("hello"), HTTP_GET), ESP_FAIL);

    // use whatever as server value, as server handle is internally just a void pointer
    int server_val = 0;
    server = &server_val;
    EXPECT_EQ(streamer.bind(server, std::string("hello"), HTTP_GET), ESP_OK);
}

TEST_F(StreamerTest, test_unbind){
    auto streamer = ChunkableDataStreamer("path");
    EXPECT_EQ(streamer.unbind(), ESP_ERR_INVALID_STATE);  // as we didn't bind it before

    // server_ptr is a void ptr, can point to anything; let's make it point to something that's not nullptr
    int server = 1;
    httpd_handle_t server_ptr = &server;
    streamer.bind(server_ptr, std::string("hello"), HTTP_GET);
    EXPECT_EQ(streamer.unbind(), ESP_OK);  // now unbind works, as we did bind first
}

TEST_F(StreamerTest, test_handler_wrapper_chunkable){
    auto streamer = ChunkableDataStreamer("path");
    httpd_req_t req;
    req.user_ctx = &streamer;
    EXPECT_EQ(ChunkableDataStreamer::handler_wrapper(&req), ESP_OK);

    DummyChunkableCls::last_error = ESP_FAIL;
    EXPECT_EQ(ChunkableDataStreamer::handler_wrapper(&req), ESP_FAIL);
    DummyChunkableCls::last_error.reset();

    MockHttpServerOps::resp_send_chunk_ret = ESP_FAIL;
    EXPECT_EQ(ChunkableDataStreamer::handler_wrapper(&req), ESP_FAIL);
}

TEST_F(StreamerTest, test_handler_wrapper_iterable_of_chunkables){
    auto streamer = ChunkableIterDataStreamer("path");
    httpd_req_t req;
    req.user_ctx = &streamer;
    EXPECT_EQ(ChunkableIterDataStreamer::handler_wrapper(&req), ESP_OK);

    DummyIterableOfChunkables::last_error = ESP_FAIL;
    EXPECT_EQ(ChunkableIterDataStreamer::handler_wrapper(&req), ESP_FAIL);
    DummyIterableOfChunkables::last_error.reset();

    MockHttpServerOps::resp_send_chunk_ret = ESP_FAIL;
    EXPECT_EQ(ChunkableIterDataStreamer::handler_wrapper(&req), ESP_FAIL);
}

