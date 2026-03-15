cmake_minimum_required(VERSION 3.20)
project(custom-memory-allocator
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Warnings
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wnull-dereference
    >
)

# Sanitizers for Debug builds
option(ENABLE_SANITIZERS "Enable sanitizers in debug builds" ON)

if (ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
        message(STATUS "Sanitizers ENABLED")
    endif()
endif()

# Library Target
add_library(allocators INTERFACE
        include/utils.hpp
        include/linear_allocator.hpp)

target_include_directories(allocators
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Dependencies via FetchContent
include(FetchContent)

# Google Test
FetchContent_Declare(
        googletest
        GIT_REPOSITORY  https://github.com/google/googletest.git
        GIT_TAG         v1.17.0
        GIT_SHALLOW     TRUE
)
FetchContent_MakeAvailable(googletest)

# Google Benchmark
FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY  https://github.com/google/benchmark.git
        GIT_TAG         v1.9.5
        GIT_SHALLOW     TRUE
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

enable_testing()

# Subdirectories
add_subdirectory(tests)
add_subdirectory(benchmarks)
