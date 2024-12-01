if(NOT DEFINED PROTOC_PATH)
    find_program(PROTOC_PATH protoc
        HINTS
            "${Protobuf_DIR}/bin"
            "${PROTOBUF_ROOT}/bin"
            "/usr/local/bin"
            "/usr/bin"
    )
endif()

if(NOT DEFINED GRPC_CPP_PLUGIN_PATH)
    find_program(GRPC_CPP_PLUGIN_PATH grpc_cpp_plugin
        HINTS
            "${gRPC_DIR}/bin"
            "${GRPC_ROOT}/bin"
            "/usr/local/bin"
            "/usr/bin"
    )
endif()

function(compile_proto TARGET_NAME PROTO_FILES)
    if(NOT PROTOC_PATH)
        message(FATAL_ERROR "protoc not found! Please install protobuf compiler.")
    endif()

    if(NOT GRPC_CPP_PLUGIN_PATH)
        message(FATAL_ERROR "grpc_cpp_plugin not found! Please install gRPC.")
    endif()

    set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

    set(ALL_GENERATED_SRCS)

    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        get_filename_component(PROTO_PATH ${PROTO_FILE} PATH)

        set(PROTO_SRCS "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDRS "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.h")
        set(GRPC_SRCS "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.cc")
        set(GRPC_HDRS "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.h")

        list(APPEND ALL_GENERATED_SRCS ${PROTO_SRCS} ${GRPC_SRCS})

        add_custom_command(
            OUTPUT 
                ${PROTO_SRCS} 
                ${PROTO_HDRS} 
                ${GRPC_SRCS} 
                ${GRPC_HDRS}
            COMMAND ${CMAKE_COMMAND} -E echo "Generating gRPC files for ${PROTO_FILE}"
            COMMAND ${PROTOC_PATH}
            ARGS --grpc_out=${PROTO_GEN_DIR}
                 --cpp_out=${PROTO_GEN_DIR}
                 --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_PATH}
                 -I ${PROTO_PATH}
                 ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating gRPC files for ${PROTO_FILE}"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )

        set_source_files_properties(
            ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
            PROPERTIES GENERATED TRUE
        )
    endforeach()

    target_sources(${TARGET_NAME}
        PRIVATE
            ${ALL_GENERATED_SRCS}
    )

    target_include_directories(${TARGET_NAME}
        PUBLIC
            ${PROTO_GEN_DIR}
    )
endfunction()