set(inc_path "include")
set(src_path "src")
set(header_files
        ${inc_path}/config.h
        ${inc_path}/concepts.h
        ${inc_path}/server_ops.h
        ${inc_path}/streamer.h
        ${inc_path}/vfs_streamer.h
)

if (${ESP_PLATFORM})
    idf_component_register(
            INCLUDE_DIRS "${inc_path}"
            PRIV_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}
            REQUIRES "esp_http_server"
    )
else()
    project("data-streamer-test-host" C CXX)
    add_library(${PROJECT_NAME} INTERFACE)

    # test resources
    set(TEST_RESOURCES_DIR ${CMAKE_CURRENT_LIST_DIR}/test-host/data)
    file(SIZE "${TEST_RESOURCES_DIR}/test_data_1.txt" TEST_DATA_1_FILE_SIZE)
    configure_file(
            ${CMAKE_CURRENT_LIST_DIR}/test-host/test_config.h.in
            ${CMAKE_BINARY_DIR}/test-host/generated/test_config.h
    )
    target_include_directories(
            ${PROJECT_NAME}
            INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include
    )
    enable_testing()
    include(GoogleTest)
    add_subdirectory("test-host")
endif()
