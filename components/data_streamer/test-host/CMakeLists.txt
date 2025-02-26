macro(package_add_test TESTNAME)
    add_executable(${TESTNAME} ${ARGN})
    target_link_libraries(${TESTNAME} gtest gmock gtest_main ${PROJECT_NAME})
    target_include_directories(${TESTNAME}
            PRIVATE ${CMAKE_CURRENT_LIST_DIR}/stubs
                    ${CMAKE_BINARY_DIR}/test-host/generated
    )
    gtest_discover_tests(${TESTNAME}
            PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
    )
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

message("host-test: adding tests")
package_add_test(data_sync
        test_streamer.cpp
        test_vfs_streamer.cpp
)