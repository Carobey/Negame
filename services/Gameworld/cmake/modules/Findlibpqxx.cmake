# cmake/modules/Findlibpqxx.cmake
find_path(PQXX_INCLUDE_DIR pqxx/pqxx)
find_library(PQXX_LIBRARY NAMES pqxx)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libpqxx
    REQUIRED_VARS PQXX_LIBRARY PQXX_INCLUDE_DIR
)

if(libpqxx_FOUND AND NOT TARGET libpqxx::pqxx)
    add_library(libpqxx::pqxx UNKNOWN IMPORTED)
    set_target_properties(libpqxx::pqxx PROPERTIES
        IMPORTED_LOCATION "${PQXX_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PQXX_INCLUDE_DIR}"
    )
endif()