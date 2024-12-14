add_library(${SERVICE_NAME}_lib STATIC)

collect_sources(LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}")

target_sources(${SERVICE_NAME}_lib
    PRIVATE
        ${LIB_SOURCES}
)

add_dependencies(${SERVICE_NAME}_lib ${SERVICE_NAME}_proto)

target_include_directories(${SERVICE_NAME}_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SERVICE_NAME}_lib
    PUBLIC
        ${SERVICE_NAME}_proto
        Boost::system
        Boost::filesystem
        Boost::log
        Boost::log_setup  
        Boost::thread     
        Boost::program_options
        Boost::json
        PostgreSQL::PostgreSQL
        libpqxx::pqxx
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
)

add_executable(${SERVICE_NAME}
    main.cpp
)

target_link_libraries(${SERVICE_NAME}
    PRIVATE
        ${SERVICE_NAME}_lib
)