include(FetchContent)

set(DEPENDENCY_PATHS
    "/usr/local"
    "/usr"
)

if(NOT Boost_FOUND)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Boost_NO_SYSTEM_PATHS OFF)
    
    set(BOOST_ROOT_HINTS
        "/usr/local/boost"
        "/usr/local/opt/boost"
        "/opt/homebrew/opt/boost"
    )
    
    foreach(path ${BOOST_ROOT_HINTS})
        if(EXISTS "${path}")
            set(BOOST_ROOT "${path}")
            break()
        endif()
    endforeach()

    find_package(Boost 1.81 REQUIRED COMPONENTS 
        system 
        filesystem 
        program_options 
        log 
        json
        log_setup
        thread
    )
endif()

if(USE_POSTGRESQL)
    if(NOT PostgreSQL_FOUND)
        set(PostgreSQL_ADDITIONAL_VERSIONS "16" "15" "14")
        set(PostgreSQL_ADDITIONAL_PATHS
            "/usr/local/pgsql"
            "/usr/pgsql-16"
            "/usr/pgsql-15" 
            "/usr/pgsql-14"
            "/usr/lib/postgresql/16"
            "/usr/lib/postgresql/15"
            "/usr/lib/postgresql/14"
            "/usr/local/opt/postgresql@16"
            "/usr/local/opt/postgresql@15"
            "/usr/local/opt/postgresql@14"
        )
        
        find_package(PostgreSQL REQUIRED)
        if(NOT PostgreSQL_FOUND)
            message(FATAL_ERROR "PostgreSQL 14 or higher not found.")
        endif()
        
        if(PostgreSQL_VERSION_STRING VERSION_LESS "14")
            message(FATAL_ERROR "PostgreSQL version must be 14 or higher. Found: ${PostgreSQL_VERSION_STRING}")
        endif()
    endif()

    find_path(PQXX_INCLUDE_DIR pqxx/pqxx
        PATHS
            /usr/include
            /usr/local/include
            ${PostgreSQL_INCLUDE_DIRS}
    )

    find_library(PQXX_LIBRARY NAMES pqxx libpqxx
        PATHS
            /usr/lib
            /usr/lib64
            /usr/local/lib
            /usr/local/lib64
            ${PostgreSQL_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libpqxx
        REQUIRED_VARS 
            PQXX_LIBRARY 
            PQXX_INCLUDE_DIR
    )

    if(libpqxx_FOUND AND NOT TARGET libpqxx::pqxx)
        add_library(libpqxx::pqxx UNKNOWN IMPORTED)
        set_target_properties(libpqxx::pqxx PROPERTIES
            IMPORTED_LOCATION "${PQXX_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${PQXX_INCLUDE_DIR}"
        )
    endif()

    mark_as_advanced(PQXX_INCLUDE_DIR PQXX_LIBRARY)
endif()

if(USE_GRPC)
    if(NOT gRPC_FOUND)
        set(gRPC_ROOT_HINTS
            "/usr/local/grpc"
            "/usr/local/opt/grpc"
            "/opt/homebrew/opt/grpc"
        )
        
        foreach(path ${gRPC_ROOT_HINTS})
            if(EXISTS "${path}")
                set(gRPC_ROOT "${path}")
                break()
            endif()
        endforeach()
        
        find_package(gRPC CONFIG REQUIRED)
        if(NOT gRPC_FOUND)
            message(FATAL_ERROR "gRPC not found. Please install gRPC or specify gRPC_ROOT")
        endif()
        
        find_package(Protobuf REQUIRED)
        if(NOT Protobuf_FOUND)
            message(FATAL_ERROR "Protobuf not found. Please install Protobuf")
        endif()
    endif()
    
    if(NOT TARGET gRPC::grpc++_reflection)
        message(FATAL_ERROR "gRPC reflection library not found")
    endif()
endif()

if(USE_TLS)
    if(NOT OpenSSL_FOUND)
        set(OPENSSL_ROOT_HINTS
            "/usr/local/openssl"
            "/usr/local/opt/openssl@1.1"
            "/opt/homebrew/opt/openssl@1.1"
        )
        
        foreach(path ${OPENSSL_ROOT_HINTS})
            if(EXISTS "${path}")
                set(OPENSSL_ROOT_DIR "${path}")
                break()
            endif()
        endforeach()
        
        find_package(OpenSSL REQUIRED)
        if(NOT OpenSSL_FOUND)
            message(FATAL_ERROR "OpenSSL not found. Please install OpenSSL or specify OPENSSL_ROOT_DIR")
        endif()
    endif()
endif()

mark_as_advanced(
    BOOST_ROOT
    PostgreSQL_ROOT
    gRPC_ROOT
    OPENSSL_ROOT_DIR
    libpqxx_ROOT
)