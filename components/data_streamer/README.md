# Data Streamer Component

A flexible ESP-IDF component for streaming files and directories over HTTP(s),
with support for chunked transfer encoding and multipart responses.

## Features

- **Single File Streaming**: Stream individual files with chunked transfer encoding
- **Directory Streaming**: Stream multiple files using chunked encoding and multipart/mixed responses
- **Range Support**: Filter directory contents using `from` and `to` query parameters
- **Type Safe**: Utilizes C++20 concepts for compile-time interface validation

Note that the base classes in `streamer.h` don't assume that data is organized in files and directories, however the
utility classes in `vfs_streamer.h`, which assume ESP's Virtual File System, do.

If you want to stream non-file data out of the ESP via HTTP chunked encoding, you'll need to write containers compatible
with the concepts in `concepts.h`, that return chunks of the data to stream out.
More about that in the **Custom streamers** section below.

## Installation

Add this component to your ESP-IDF project's `components`.

## Configuration

Configure the component through `menuconfig`:

```bash
idf.py menuconfig
```

Under "Component config → Data Streamer":
- `CONFIG_DATA_STREAMER_CHUNK_SIZE`: Size of chunks for file streaming (default: 1024).
  Bigger values might speed up transmission, but at the cost of memory.
- `CONFIG_DATA_STREAMER_MULTIPART_BOUNDARY`: Boundary string for multipart responses

## Usage

### Single File Streaming

```cpp
#include "data_streamer/vfs_streamer.h"

// server is from the esp http server component
void register_endpoints(httpd_handle_t server) {
    auto streamer = data_streamer::VFSFileStreamer("/spiffs/myfile.txt");  // or any mounted vfs
    streamer.bind(server, "/download", HTTP_GET);
}
```

### Directory Streaming

```cpp
#include "data_streamer/vfs_streamer.h"

// server is from the esp http server component
void register_endpoints(httpd_handle_t server) {
    auto streamer = data_streamer::VFSFlatDirStreamer("/spiffs");  // or any mounted vfs
    streamer.bind(server, "/download-all", HTTP_GET);
}
```

Directory streaming supports optional URL parameters:
- `?from=file1.txt`: Start streaming from this filename (lexicographic ordering)
- `?to=file2.txt`: Stop streaming at this filename (lexicographic ordering)

## Custom Streamers

Create custom streamers by implementing either the `Chunkable` or `IterableOfChunkables` concept:

```cpp
class CustomChunker {
    // Must provide:
    std::string_view name();
    iterator begin();
    iterator end();
    std::optional<int> error();
    // Must have an Iterator returning std::span<char>
};
```

## License

[Apache 2.0](http://www.apache.org/licenses/LICENSE-2.0)




