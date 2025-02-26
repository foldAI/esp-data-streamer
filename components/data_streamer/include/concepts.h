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

#include <concepts>
#include <span>
#include <optional>
#include <iterator>

namespace data_streamer {


/**
 * @brief Concept for iterators that yield chunks of data
 *
 * Defines requirements for iterators used in streaming operations. A ChunkIterator
 * must be an input iterator that yields chunks of data as std::span<char>.
 *
 * Requirements:
 * - Must satisfy std::input_iterator
 * - Dereferencing must yield std::span<char>
 *
 * Example implementation:
 * @code
 * class MyChunkIterator {
 * public:
 *     using iterator_category = std::input_iterator_tag;
 *     using value_type = std::span<char>;
 *
 *     MyChunkIterator& operator++();
 *     std::span<char> operator*() const;
 *     bool operator==(const MyChunkIterator&) const;
 * };
 * @endcode
 */
template<typename T>
concept ChunkIterator = std::input_iterator<T> &&
    // dereference operator returns a span of char (a "chunk")
    requires(T it) {
    { *it } -> std::convertible_to<std::span<char>>;
    };


/**
 * @brief Concept for types that can be streamed chunk by chunk
 *
 * Defines requirements for types that provide chunked access to their data.
 * These types are used for streaming single items (like files).
 *
 * Requirements:
 * - Must have an associated iterator type
 * - Must provide a name() method returning std::string_view
 * - Must provide begin() and end() methods returning its iterator type
 * - Must provide an error() method returning std::optional<int>
 * - Its iterator must satisfy ChunkIterator
 * - Must be constructible from std::string_view (typically a path)
 *
 * Example implementation:
 * @code
 * class MyChunkable {
 * public:
 *     using iterator = MyChunkIterator;
 *
 *     explicit MyChunkable(std::string_view path);
 *     std::string_view name();
 *     iterator begin();
 *     iterator end();
 *     std::optional<int> error();
 * };
 * @endcode
 */
template<typename T>
concept Chunkable = requires(T c) {
    typename T::iterator;
    // has a name (useful in multipart streaming)
    { c.name() } -> std::same_as<std::string_view>;
    // has .begin() and .end()
    { c.begin() } -> std::same_as<typename T::iterator>;
    { c.end() } -> std::same_as<typename T::iterator>;
    // get last error
    { c.error() } -> std::same_as<std::optional<int>>;
    // its iterator is a ChunkIterator
    requires ChunkIterator<typename T::iterator>;
    // can be constructed with string (e.g. a vfs path)
    requires std::constructible_from<T, std::string_view>;
};


/**
 * @brief Concept for types that iterate over Chunkable items
 *
 * Defines requirements for types that provide iteration over multiple Chunkable
 * items (like directories containing files).
 *
 * Requirements:
 * - Must have an associated iterator type that is an input iterator
 * - Must provide begin() and end() methods returning its iterator type
 * - Must provide an error() method returning std::optional<int>
 * - Its iterator must yield types that satisfy Chunkable
 * - Must be constructible from std::string_view (typically a path)
 *
 * Example implementation:
 * @code
 * class MyIterableOfChunkables {
 * public:
 *     class iterator {
 *         // input iterator implementation
 *         // operator* returns a Chunkable
 *     };
 *
 *     explicit MyIterableOfChunkables(std::string_view path);
 *     iterator begin();
 *     iterator end();
 *     std::optional<int> error();
 * };
 * @endcode
 */
template<typename T>
concept IterableOfChunkables = requires (T c) {
    requires std::input_iterator<typename T::iterator>;
    // has .begin() and .end()
    { c.begin() } -> std::same_as<typename T::iterator>;
    { c.end() } -> std::same_as<typename T::iterator>;
    // get last error
    { c.error() } -> std::same_as<std::optional<int>>;
    // its iterator returns a Chunkable
    requires Chunkable<std::iter_value_t<typename T::iterator>>;
    // can be constructed with string (a vfs path)
    requires std::constructible_from<T, std::string_view>;
};
}  // namespace data_streamer
