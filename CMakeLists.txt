cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts-diagnostics-depth=6")

if (${ESP_PLATFORM})
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    set(EXTRA_COMPONENT_DIRS components)
    project("esp-data-streamer")
else()
    add_subdirectory(libs/googletest)
    add_subdirectory(components/data_streamer)
    project("host-test")
endif()
