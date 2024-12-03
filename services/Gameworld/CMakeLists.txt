cmake_minimum_required(VERSION 3.20)

# Опция выбора компилятора
option(USE_CLANG "Build with clang" OFF)


# Проверка и установка компилятора
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

# Установка имени проекта через переменную
if(NOT DEFINED SERVICE_NAME)
   set(SERVICE_NAME "gameworld" CACHE STRING "Name of the microservice")
endif()

project(${SERVICE_NAME} VERSION 0.0.1 LANGUAGES CXX)

# Настройка Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0 
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


# Включить тестирование
enable_testing()
include(GoogleTest)

# Установить флаг для сборки тестов
set(BUILD_TESTING ON CACHE BOOL "Enable tests" FORCE)

# Установим пути для выходных файлов
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Установим пути для выходных файлов
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Настроим директории для сгенерированных файлов
set(GENERATED_DIR "${CMAKE_BINARY_DIR}")
set(GENERATED_INCLUDE_DIR "${GENERATED_DIR}/include")
file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR}/core)

# Сгенерируем config.hpp
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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Общие настройки компилятора
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Опции проекта
option(USE_POSTGRESQL "Enable PostgreSQL support" ON)
option(USE_GRPC "Enable gRPC support" ON)
option(USE_TLS "Enable TLS support" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_DOCS "Build documentation" ON)
option(ENABLE_STATIC_ANALYSIS "Enable static analysis tools (clang-tidy, cppcheck)" OFF)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Поиск зависимостей
find_package(Boost 1.81 REQUIRED COMPONENTS 
   system 
   filesystem 
   program_options 
   log 
   json
   log_setup
   thread
)

if(USE_POSTGRESQL)
   find_package(PostgreSQL REQUIRED)
   find_package(libpqxx REQUIRED)
endif()

if(USE_GRPC)
   find_package(gRPC CONFIG REQUIRED)
   find_package(Protobuf REQUIRED)

    if(NOT TARGET gRPC::grpc++_reflection)
        message(FATAL_ERROR "gRPC reflection library not found")
    endif()

   # Функция для компиляции proto файлов
   function(compile_proto TARGET_NAME PROTO_FILES)
    get_target_property(gRPC_CPP_PLUGIN_PATH gRPC::grpc_cpp_plugin LOCATION)
    
    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        get_filename_component(PROTO_PATH ${PROTO_FILE} PATH)
        
        set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
        file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
        
        set(PROTO_SRCS "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDRS "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.h")
        set(GRPC_SRCS "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.cc")
        set(GRPC_HDRS "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.h")
        
        add_custom_command(
            OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            ARGS --grpc_out=${PROTO_GEN_DIR}
                 --cpp_out=${PROTO_GEN_DIR}
                 --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_PATH}
                 -I ${PROTO_PATH}
                 ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating gRPC files for ${PROTO_FILE}"
            VERBATIM
        )
        
        target_sources(${TARGET_NAME}
            PRIVATE
                ${PROTO_SRCS}
                ${GRPC_SRCS}
        )
        
        target_include_directories(${TARGET_NAME}
            PUBLIC
                ${PROTO_GEN_DIR}
        )
    endforeach()
endfunction()
endif() 

if(USE_TLS)
   find_package(OpenSSL REQUIRED)
endif()

# Подключение модулей CMake
include(cmake/modules/Dependencies.cmake)
include(cmake/modules/StaticAnalysis.cmake)



# Подключение директорий
add_subdirectory(src)
add_subdirectory(proto)


if(BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()

# Копирование compile_commands.json
add_custom_target(copy-compile-commands ALL
   ${CMAKE_COMMAND} -E copy_if_different
   ${CMAKE_BINARY_DIR}/compile_commands.json
   ${CMAKE_SOURCE_DIR}/compile_commands.json
   DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
)

# Установка
install(TARGETS ${SERVICE_NAME} ${SERVICE_NAME}_proto
   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
   DESTINATION include/${SERVICE_NAME}
   FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)
