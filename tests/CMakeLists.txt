# Test Executable
add_executable(allocator_tests
    test.cpp
)

target_link_libraries(allocator_tests
        PRIVATE
        allocators
        project_warnings
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(allocator_tests)