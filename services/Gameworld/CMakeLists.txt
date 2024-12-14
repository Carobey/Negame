cmake_minimum_required(VERSION 3.20)

include(ProcessorCount)
ProcessorCount(N)
set(CMAKE_FIND_PACKAGE_PARALLEL_LEVEL ${N})
set(CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY OFF)

list(APPEND CMAKE_PREFIX_PATH 
    "/usr/local/boost"
    "/usr/local/grpc"
    "/usr/local/openssl"
)

option(USE_CLANG "Build with clang" OFF)
option(USE_CCACHE "Use ccache if available" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" OFF)
option(USE_GRPC "Enable gRPC support" ON)
option(USE_TLS "Enable TLS support" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_DOCS "Build documentation" ON)
option(ENABLE_STATIC_ANALYSIS "Enable static analysis tools" OFF)

find_program(CCACHE_PROGRAM ccache)
if(USE_CCACHE AND CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

if(USE_CLANG)
    find_program(CLANG_COMPILER "clang++")
    if(NOT CLANG_COMPILER)
        message(FATAL_ERROR "Clang compiler not found!")
    endif()
    set(CMAKE_CXX_COMPILER ${CLANG_COMPILER})
    set(CMAKE_C_COMPILER "clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fstandalone-debug")
    endif()
else()
    find_program(GCC_COMPILER "g++")
    if(NOT GCC_COMPILER)
        message(FATAL_ERROR "GCC compiler not found!")
    endif()
    set(CMAKE_CXX_COMPILER ${GCC_COMPILER})
    set(CMAKE_C_COMPILER "gcc")
endif()

if(NOT DEFINED SERVICE_NAME)
    set(SERVICE_NAME "gameworld" CACHE STRING "Name of the microservice")
endif()

project(${SERVICE_NAME} VERSION 0.0.1 LANGUAGES CXX)

set(_BOOST_REQUIRED_COMPONENTS 
    system 
    filesystem 
    program_options 
    log 
    json
    log_setup
    thread
)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
    GIT_CONFIG advice.detachedHead=false
)
FetchContent_MakeAvailable(googletest)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
enable_testing()
include(GoogleTest)

# Global settings
set(BUILD_TESTING ON CACHE BOOL "Enable tests" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(GENERATED_DIR "${CMAKE_BINARY_DIR}")
set(GENERATED_INCLUDE_DIR "${GENERATED_DIR}/include")
file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR}/core)

configure_file(
    ${CMAKE_SOURCE_DIR}/include/core/config.hpp.in
    ${GENERATED_INCLUDE_DIR}/core/config.hpp
    @ONLY
)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${GENERATED_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}/generated
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/modules")
include(CMakeUtils)
include(Dependencies)
include(StaticAnalysis)
include(ProtoCompile)

add_subdirectory(src)
add_subdirectory(proto)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_custom_target(copy-compile-commands ALL
    ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
)

if(TARGET ${SERVICE_NAME} AND TARGET ${SERVICE_NAME}_proto)
    install(TARGETS ${SERVICE_NAME} ${SERVICE_NAME}_proto
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()

install(DIRECTORY include/
    DESTINATION include/${SERVICE_NAME}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)