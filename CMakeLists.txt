cmake_minimum_required(VERSION 3.16)
project(simular.allocators VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(${PROJECT_NAME} INTERFACE)
add_library(simular::allocators ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} INTERFACE ${PROJECT_SOURCE_DIR}/include)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(
        ${PROJECT_NAME}
        #INTERFACE -fsanitize=address
        INTERFACE -fanalyzer
        INTERFACE -fdiagnostics-parseable-fixits
        INTERFACE -Wanalyzer-double-free
        INTERFACE -Wanalyzer-null-dereference
        INTERFACE -Wall
        INTERFACE -Wextra
        INTERFACE -Wpedantic
    )
    target_link_options(
        ${PROJECT_NAME}
        #INTERFACE -fsanitize=address
        INTERFACE -fanalyzer
        INTERFACE -fdiagnostics-parseable-fixits
    )
endif()

# Do you want to build the tests?
option(SIMULAR_ALLOCATORS_BUILD_TESTS "Build tests" OFF)
if(SIMULAR_ALLOCATORS_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    ON
    )
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)
    add_subdirectory(tests)
endif()

option(SIMULAR_ALLOCATORS_BUILD_EXAMPLE "Build example" OFF)
if(SIMULAR_ALLOCATORS_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
