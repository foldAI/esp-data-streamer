# ESP Data Streamer Component

A header-only template-based C++ ESP-ISF component for streaming data over HTTP(S) from a ESP host, 
with support for both single-file and directory streaming.


## Features

- **Flexible Streaming Patterns**:
  - Single item streaming
  - Collection streaming with multipart responses
  - Range-based filtering
- **Memory Efficient**: Uses chunked transfer encoding
- **Template-Based Design**:
  - Generic streaming interface through C++ concepts
  - Easily adaptable to different data sources
- **Type-Safety**: Compile-time interface validation using C++ concepts

## Structure

```
├── components
│   └── data_streamer
│       ├── include/
│       │   ├── concepts.h              # Interface definitions using C++ concepts
│       │   ├── streamer.h              # Core streaming implementation
│       │   ├── vfs_streamer.h          # VFS (Virtual File System) implementation
│       │   ├── server_ops.h            # Utility interface class for http operations (helps testing) 
│       │   └── config.h                # Config variables (use menuconfig to set)
│       ├── test-host/                  # Tests to be run on host (not on ESP)
│       └── Kconfig                     # Component configuration
├── main                                # example project using the data_streamer library 
│   ├── certs/                          # certificates for ESP server in example
│   ├── data_streamer_example_main.cpp  # main entrypoint for example
│   └── Kconfig.projbuild               # example config variables (use menuconfig to set)
├── ca_scripts/                         # scripts used to create certificates for example project. NOT production-ready
├── python                              
│   └── data-streamer                   # python client for downloading data from device running example project
│       ├── certs/                      # client certificates
│       ├── client.py                   # actual client code
│       ├── poetry.lock                 # dependencies lock
│       ├── pyproject.toml              # python project file for dependencies / packaging
│       └── README.md                   # readme for the client
├── libs                                # external libraries                              
├── sdkconfig.defaults                  # default sdk configuration variables for example project
├── LICENSE                             # Apache 2.0 license text                             
├── NOTICE                              # Apache 2.0 license notice
└── README.md                           # This file
 
```

## Usage

First, clone the repository:

```
git clone https://[repository-url]/esp-data-streamer.git
git submodule update --init --recursive
```

### Basic Integration

1. Add the component to your project's `components` directory
2. Include the necessary headers:
   ```cpp
   #include "vfs_streamer.h"  // For VFS implementation
   // or
   #include "streamer.h"      // For custom implementations
   ```

### Example: VFS File Streaming

```cpp
#include "vfs_streamer.h"

// Create a file streamer
auto file_streamer = data_streamer::VFSFileStreamer("<vfs_mount_point>/path/to/file");

// Bind to HTTP server
file_streamer.bind(server, "/stream", HTTP_GET);
```

### Example: VFS Directory Streaming

```cpp
#include "vfs_streamer.h"

// Create a directory streamer
auto dir_streamer = data_streamer::VFSFlatDirStreamer("<vfs_mount_point>/path/to/dir");

// Bind to HTTP server
dir_streamer.bind(server, "/stream", HTTP_GET);
```

## Creating Custom Streamers

The component uses C++ concepts to define streaming interfaces. To create a custom streamer, implement either:

### Single Item Streaming
```cpp
class CustomChunker {
public:
    // Required methods
    std::string_view name();          // Item identifier
    std::optional<int> error();       // Error reporting
    iterator begin();                 // Returns ChunkIterator, see concepts.h
    iterator end();                   // Returns ChunkIterator
    
    // Iterator must yield std::span<char>
};
```

### Collection Streaming
This results in a multipart chunked transfer endpoint.

```cpp
class CustomCollection {
public:
    // Required methods
    std::optional<int> error();       // Error reporting
    iterator begin();                 // Returns iterator of Chunkable items
    iterator end();                   // Returns iterator of Chunkable items
};
```

## Configuration

Configure through  `idf.py menuconfig`. 
Options for the example project are under `Data Streamer example`, whereas options 
for the core library are under `Component config → Data streamer configuration`.

## Example Project

See the [example project](https://github.com/foldAI/esp-data-streamer) for a complete implementation using:
- Adafruit Metro ESP32-S3 N16R8 as development device
- SD card as VFS mounting point
- mDNS for device discovery
- HTTPS with mutual TLS
- Python client for data retrieval (at `python/data-streamer/`)

## Technical Details

### Memory Usage

- Configurable chunk size (default: 4KB)
- Single allocation per streaming session
- No dynamic allocations during streaming

### Performance Considerations

- Chunk size affects memory usage vs. performance trade-off
- Directory streaming uses flat traversal (non-recursive)
- Range filtering performed during traversal

## License

See LICENSE file
