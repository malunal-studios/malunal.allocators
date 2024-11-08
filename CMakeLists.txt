cmake_minimum_required(VERSION 3.16)
project(malunal.allocators VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(${PROJECT_NAME} INTERFACE)
add_library(malunal::allocators ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_compile_options(
    ${PROJECT_NAME}
    INTERFACE -Wall
    INTERFACE -Wextra
    INTERFACE -Wpedantic
)

option(MALUNAL_ALLOCATORS_ENABLE_DIAGNOSTICS "Enable address sanitizer and static analyzer" OFF)
if(MALUNAL_ALLOCATORS_ENABLE_DIAGNOSTICS)
    target_compile_options(
        ${PROJECT_NAME}
        INTERFACE -fsanitize=address
        INTERFACE -fanalyzer
        INTERFACE -fdiagnostics-parseable-fixits
        INTERFACE -Wanalyzer-double-free
        INTERFACE -Wanalyzer-null-dereference
    )
    target_link_options(
        ${PROJECT_NAME}
        INTERFACE -fsanitize=address
        INTERFACE -fanalyzer
        INTERFACE -fdiagnostics-parseable-fixits
    )
endif()

# Do you want to build the tests?
option(MALUNAL_ALLOCATORS_BUILD_TESTS "Build tests" OFF)
if(MALUNAL_ALLOCATORS_BUILD_TESTS)
    message(STATUS "Tests compilation enabled for Malunal.Allocators")
    message(STATUS "Fetching Google Test library...")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    ON
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    message(STATUS "Fetching Google Test library - done")

    enable_testing()
    include(GoogleTest)
    add_subdirectory(tests)
endif()


option(MALUNAL_ALLOCATORS_BUILD_BENCHMARKS "Build benchmarks" OFF)
if(MALUNAL_ALLOCATORS_BUILD_BENCHMARKS)
    # Benchmarks should always run in release mode so capture old build type,
    # configure the benchmarks then reset the build type after.
    set(OLD_BUILD_TYPE ${CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE "Release")

    message(STATUS "Benchmarks compilation enabled for Malunal.Allocators")
    message(STATUS "Fetching Google Benchmark library...")
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.5
        GIT_SHALLOW    ON
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)
    message(STATUS "Fetching Google Benchmark library - done")

    add_subdirectory(benchmarks)

    # Reset build type.
    set(CMAKE_BUILD_TYPE ${OLD_BUILD_TYPE})
endif()

option(MALUNAL_ALLOCATORS_BUILD_EXAMPLE "Build example" OFF)
if(MALUNAL_ALLOCATORS_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
