# cmake/ProtoCompile.cmake

# Функция для компиляции proto файлов
function(compile_proto TARGET_NAME PROTO_FILES)
    # Находим необходимые пакеты
    find_package(Protobuf REQUIRED)
    find_package(gRPC REQUIRED)

    # Получаем путь к плагину gRPC
    get_target_property(gRPC_CPP_PLUGIN_PATH gRPC::grpc_cpp_plugin LOCATION)

    # Для каждого proto файла
    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        get_filename_component(PROTO_PATH ${PROTO_FILE} PATH)

        set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
        file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

        # Генерируемые файлы
        set(PROTO_SRCS "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDRS "${PROTO_GEN_DIR}/${PROTO_NAME}.pb.h")
        set(GRPC_SRCS "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.cc")
        set(GRPC_HDRS "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.h")

        # Команда для генерации файлов
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

        # Добавляем файлы к цели
        target_sources(${TARGET_NAME}
            PRIVATE
                ${PROTO_SRCS}
                ${GRPC_SRCS}
        )

        # Добавляем директорию с генерируемыми файлами
        target_include_directories(${TARGET_NAME}
            PUBLIC
                ${PROTO_GEN_DIR}
        )
    endforeach()
endfunction()