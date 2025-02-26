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
#include <math.h>
#include <system_error>
#include "gtest/gtest.h"
#include "vfs_streamer.h"
#include "test_config.h"

using namespace data_streamer;
using FileChunkerCls = FileChunker<>;
using FlatDirIterableCls = FlatDirIterable<>;


constexpr char TEST_FILE_PATH[] = TEST_RESOURCES_DIR "/test_data_1.txt";
constexpr char EMPTY_TEST_FILE_PATH[] = TEST_RESOURCES_DIR "/test_data_empty.txt";
constexpr char EMPTY_TEST_DIR[] = TEST_RESOURCES_DIR "/empty_dir";

TEST(vfs_streamer, test_file_chunker_open_existing_and_not_existing) {
    auto fc_good = FileChunkerCls(TEST_FILE_PATH);
    ASSERT_FALSE(fc_good.error());

    auto fc_bad = FileChunkerCls("not_a_file_path");
    ASSERT_EQ(fc_bad.error().value(), ENOENT);
}



template<typename T>
class FileChunkerSizeTest : public testing::Test {
protected:
    static constexpr size_t chunk_size = T::value;
    static constexpr size_t expected_iterations =
        (chunk_size >= TEST_DATA_1_FILE_SIZE) ? 1 :
        static_cast<size_t>(std::ceil(TEST_DATA_1_FILE_SIZE / static_cast<double>(chunk_size)));
};

// Define our test cases using std::integral_constant
using SingleChunkCase = std::integral_constant<size_t, TEST_DATA_1_FILE_SIZE + 1>;
using ExactSizeCase = std::integral_constant<size_t, TEST_DATA_1_FILE_SIZE>;
using MultiChunkCase = std::integral_constant<size_t, TEST_DATA_1_FILE_SIZE / 10>;

using TestTypes = testing::Types<SingleChunkCase, ExactSizeCase, MultiChunkCase>;

TYPED_TEST_SUITE(FileChunkerSizeTest, TestTypes);

TYPED_TEST(FileChunkerSizeTest, ChunkSizeIterations) {
    auto fc = FileChunker<TestFixture::chunk_size>(TEST_FILE_PATH);
    size_t iterations = 0;
    for (auto &chunk : fc) {
        iterations++;
        EXPECT_TRUE(chunk.size() <= TestFixture::chunk_size);
    }
    EXPECT_EQ(iterations, TestFixture::expected_iterations);
}

TEST(vfs_streamer, test_file_chunker_empty_file) {
    auto fc = FileChunkerCls(EMPTY_TEST_FILE_PATH);
    int iterations = 0;
    for (auto &_ : fc) {
        iterations++;
    }
    EXPECT_EQ(iterations, 0);
    EXPECT_FALSE(fc.error());
}

TEST(vfs_streamer, test_file_chunker_same_file_different_chunkers) {
    auto fc = FileChunkerCls(TEST_FILE_PATH);
    int iterations = 0;
    for (auto &_ : fc) {
        iterations++;
    }
    EXPECT_FALSE(fc.error());

    auto fc2 = FileChunkerCls(TEST_FILE_PATH);
    int iterations2 = 0;
    for (auto &_ : fc2) {
        iterations2++;
    }
    EXPECT_FALSE(fc2.error());
    EXPECT_EQ(iterations, iterations2);
}

TEST(vfs_streamer, test_file_chunker_same_file_error_iterator) {
    auto fc = FileChunkerCls(TEST_FILE_PATH);
    auto it_good = fc.begin();
    ASSERT_FALSE(fc.error());
    auto it_bad = fc.begin();
    ASSERT_EQ(fc.error(), EBUSY);  // creating a second iterator would have meant re-opening an open file
}

TEST(vfs_streamer, test_dir_iter_open_existing_and_not_existing) {
    auto d_good = FlatDirIterableCls(TEST_RESOURCES_DIR);
    ASSERT_FALSE(d_good.error());

    auto d_bad = FlatDirIterableCls("not_a_dir_path");
    ASSERT_EQ(d_bad.error().value(), ENOENT);
}

TEST(vfs_streamer, test_dir_iter_can_iterate) {
    auto d_iter = FlatDirIterableCls(TEST_RESOURCES_DIR);
    int iterations = 0;
    for (auto &chunker : d_iter) {
        iterations++;
        for (auto &chunk : chunker) {
            EXPECT_FALSE(chunk.empty());
        }
        EXPECT_FALSE(chunker.error());
    }
    EXPECT_FALSE(d_iter.error());
}

TEST(vfs_streamer, test_dir_iter_empty) {
    auto d_iter = FlatDirIterableCls(EMPTY_TEST_DIR);
    int iterations = 0;
    for (auto &_ : d_iter) {
        iterations++;
    }
    EXPECT_EQ(iterations, 0);
}