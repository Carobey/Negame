add_library(${SERVICE_NAME}_proto STATIC)

set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/v1/*.proto")

if(NOT PROTO_FILES)
    message(WARNING "No .proto files found in ${CMAKE_CURRENT_SOURCE_DIR}/v1/")
    return()
endif()

get_target_property(PROTO_SOURCES ${SERVICE_NAME}_proto SOURCES)
if(NOT PROTO_SOURCES)
    compile_proto(${SERVICE_NAME}_proto "${PROTO_FILES}")
endif()

target_include_directories(${SERVICE_NAME}_proto
    PUBLIC
        ${PROTO_GEN_DIR}
)

target_link_libraries(${SERVICE_NAME}_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

set_target_properties(${SERVICE_NAME}_proto
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)