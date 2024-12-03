# cmake/modules/FindgRPC.cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "grpc_cpp_plugin not found!")
endif()

find_program(PROTOC protoc)
if(NOT PROTOC)
    message(FATAL_ERROR "protoc not found!")
endif()