include(FetchContent)

# Boost
find_package(Boost 1.81 REQUIRED COMPONENTS 
    system 
    filesystem 
    program_options 
    log 
    json
)

# PostgreSQL
if(USE_POSTGRESQL)
    find_package(PostgreSQL REQUIRED)
    find_package(libpqxx REQUIRED)
endif()

# gRPC
if(USE_GRPC)
    find_package(gRPC CONFIG REQUIRED)
    find_package(Protobuf REQUIRED)
endif()

# OpenSSL для TLS
if(USE_TLS)
    find_package(OpenSSL REQUIRED)
endif()