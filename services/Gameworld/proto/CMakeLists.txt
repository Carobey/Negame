add_library(${SERVICE_NAME}_proto STATIC)

# Задаем пути для сгенерированных файлов
set(PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# Собираем все .proto файлы
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/v1/*.proto")

# Компилируем proto файлы
compile_proto(${SERVICE_NAME}_proto "${PROTO_FILES}")

# Добавляем директорию с сгенерированными файлами в include path
target_include_directories(${SERVICE_NAME}_proto
    PUBLIC
        ${PROTO_GEN_DIR}
)

# Подключаем зависимости
target_link_libraries(${SERVICE_NAME}_proto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

# Устанавливаем свойства для правильной сборки
set_target_properties(${SERVICE_NAME}_proto
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)