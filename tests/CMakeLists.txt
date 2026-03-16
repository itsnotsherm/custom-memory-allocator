# Test Executable
add_executable(allocator_tests
        test_linear_allocator.cpp
        test_utils.cpp
)

target_link_libraries(allocator_tests
        PRIVATE
        allocators
        project_warnings
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(allocator_tests)