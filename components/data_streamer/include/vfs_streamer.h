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
#include <span>
#include <iterator>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <optional>
#include <memory>
#include "config.h"
#include "streamer.h"


namespace data_streamer {

/**
 * @brief A file chunker that reads a file in fixed-size chunks.
 *
 * FileChunker provides an iterator-based interface for reading a file in chunks,
 * which is memory efficient and suitable for streaming large files. It implements
 * the Chunkable concept required by DataStreamer.
 *
 * @tparam CHUNK_SIZE Size of each chunk in bytes. Defaults to value from Kconfig.
 *
 * Example usage:
 * @code
 * auto chunker = FileChunker("/path/to/file");
 * for (const auto& chunk : chunker) {
 *     // Process chunk (std::span<char>)
 * }
 * if (chunker.error()) {
 *     // Handle error
 * }
 * @endcode
 */
template<int CHUNK_SIZE=CHUNK_SIZE>
class FileChunker {
public:
    /**
     * @brief Input iterator for reading file chunks.
     *
     * Provides sequential access to file chunks. The iterator is input-only
     * and single-pass (cannot go backwards or be reused).
     */
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = std::span<char>;
        using difference_type = long;
        using pointer = const std::span<char>*;
        using reference = std::span<char>&;

        Iterator(): parent(nullptr), is_end(true) {}
        Iterator(FileChunker* p, bool end)
            : parent(p), is_end(end) {
            ++(*this);  // trigger reading of first chunk
        }

        Iterator& operator++() {
            if (!is_end) {
                parent->read_chunk();
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
        FileChunker *parent;
        bool is_end;
    };
    using iterator = Iterator;

    /**
     * @brief Constructs a FileChunker for the specified file path.
     *
     * @param path Path to the file to chunk
     * @note The file is opened immediately and closed in destructor
     */
    explicit FileChunker(std::string_view path):
        path{path},
        file{nullptr},
        last_error{std::nullopt},
        has_active_iterator{false} {
        file = fopen(this->path.c_str(), "r");
        if (file == nullptr) {
            last_error = errno;
        }
    }

    // prevent FILE handle duplication by removing copy and move constructor / assignment
    FileChunker(const FileChunker&) = delete;
    FileChunker& operator=(const FileChunker&) = delete;
    FileChunker(FileChunker&&) = delete;
    FileChunker& operator=(FileChunker&&) = delete;

    ~FileChunker() {
        if (file != nullptr) {
            fclose(file);
        }
    }

    /**
     * @brief Gets the base name of the file (without path).
     *
     * @return std::string_view Name of the file
     */
    std::string_view name() {
        size_t pos = path.find_last_of('/');
        return (pos == std::string::npos) ?
            std::string_view(path) :
            std::string_view(path).substr(pos + 1);
    }


    /**
     * @brief Returns any error that occurred during operations.
     *
     * @return std::optional<int> errno value if error occurred, nullopt otherwise
     */
    std::optional<int> error() {
        return last_error;
    }

    /**
     * @brief Gets an iterator to the beginning of the file.
     *
     * @return Iterator
     * @note Only one active iterator is allowed at a time
     */
    iterator begin() {
        if (has_active_iterator) {
            ESP_LOGE(TAG, "There is an active iterator on this file already");
            last_error = EBUSY;
            return {this, true};
        }
        has_active_iterator = true;
        return {this, false};
    }

    /**
     * @brief Gets an iterator representing the end of the file.
     *
     * @return Iterator
     */
    iterator end() {
        return {this, true};
    }
private:
    void read_chunk() {
        auto bytes_read = fread(buf.data(), 1, CHUNK_SIZE, file);
        cur_chunk = std::span(buf.data(), bytes_read);
        if (bytes_read != CHUNK_SIZE) {
            if (ferror(file) != 0) {
                last_error = errno;
            }
        }
    }

    std::string path;
    FILE *file;
    std::optional<int> last_error;
    bool has_active_iterator;
    std::array<char, CHUNK_SIZE> buf;
    std::span<char> cur_chunk;
};


/**
 * @brief Provides iteration over regular files in a directory (non-recursive).
 *
 * FlatDirIterable traverses a directory and provides access to each regular file
 * through a FileChunker. It implements the IterableOfChunkables concept required
 * by DataStreamer.
 *
 * @tparam CHUNK_SIZE Size of chunks for the underlying FileChunker
 *
 * Example usage:
 * @code
 * auto dir = FlatDirIterable("/path/to/dir");
 * for (auto& file_chunker : dir) {
 *     // file_chunker is a FileChunker instance
 *     for (const auto& chunk : file_chunker) {
 *         // Process chunk
 *     }
 * }
 * if (dir.error()) {
 *     // Handle error
 * }
 * @endcode
 */
template<int CHUNK_SIZE=CHUNK_SIZE>
class FlatDirIterable {
public:
    /**
     * @brief Input iterator for accessing files in the directory.
     *
     * Provides sequential access to FileChunker instances for each regular
     * file in the directory.
     */
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = FileChunker<CHUNK_SIZE>;
        using difference_type = std::ptrdiff_t;
        using pointer = FileChunker<CHUNK_SIZE>*;
        using reference = FileChunker<CHUNK_SIZE>&;

        Iterator(): parent{nullptr}, is_end{false} {}

        Iterator(FlatDirIterable* p, bool end)
            : parent{p}, is_end{end} {
            ++(*this);  // trigger processing of first file
        }

        Iterator& operator++() {
            if (!is_end) {
                if (!parent->next_file_chunker() || parent->last_error) {
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

        bool operator==(const Iterator& other) const {
            return is_end == other.is_end;
        }

        FileChunker<CHUNK_SIZE>& operator*() const {
            return *(parent->current_chunker);
        }

    private:
        FlatDirIterable* parent;
        bool is_end;
    };

    using iterator = Iterator;

    /**
     * @brief Constructs a FlatDirIterable for the specified directory.
     *
     * @param base_path Path to the directory to traverse
     * @note The directory is opened immediately and closed in destructor
     */
    explicit FlatDirIterable(std::string_view base_path)
        : dir{nullptr},
          last_error{std::nullopt},
          base_path{base_path},
          full_path{} {
        dir = opendir(this->base_path.c_str());
        if (dir == nullptr) {
            last_error = errno;
        }
    }

    ~FlatDirIterable() {
        if (dir != nullptr) {
            closedir(dir);
        }
    }

    /**
     * @brief Returns any error that occurred during operations.
     *
     * @return std::optional<int> errno value if error occurred, nullopt otherwise
     */
    [[nodiscard]] std::optional<int> error() const { return last_error; }

    /**
     * @brief Gets an iterator to the first file in the directory.
     *
     * @return Iterator
     */
    Iterator begin() { return Iterator(this, false); }

    /**
     * @brief Gets an iterator representing the end of directory.
     *
     * @return Iterator
     */
    Iterator end() { return Iterator(this, true); }
private:
    /**
     * @brief Advances to the next regular file in the directory.
     *
     * @return bool true if next file found, false if no more files or error
     */
    bool next_file_chunker() {
        current_chunker.reset();  // cause deletion, file closing

        struct stat st{};
        if (!dir) return false;

        while (dirent* entry = readdir(dir)) {
            // Skip . and ..
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
                }
            full_path = base_path + "/" + entry->d_name;
            if (stat(full_path.c_str(), &st) == -1) {
                ESP_LOGE(TAG, "Can't stat path");
                last_error = errno;
                return false;
            }
            if (S_ISREG(st.st_mode)) {
                current_chunker.emplace(full_path);
                return true;
            }
        }
        return false;
    }
    DIR* dir;
    std::optional<int> last_error;
    std::string base_path;
    std::string full_path;
    std::optional<FileChunker<CHUNK_SIZE>> current_chunker;
};

/**
 * @brief Type alias for a file-based data streamer
 */
using VFSFileStreamer = DataStreamer<FileChunker<>>;

/**
 * @brief Type alias for a directory-based data streamer
 */
using VFSFlatDirStreamer = DataStreamer<FlatDirIterable<>>;
}  // namespace data_streamer
