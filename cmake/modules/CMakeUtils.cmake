# Function to safely collect source files
function(collect_sources OUT_VAR ROOT_DIR)
    set(sources "")
    file(GLOB_RECURSE new_sources 
        "${ROOT_DIR}/*.cpp"
        "${ROOT_DIR}/*.hpp"
        "${ROOT_DIR}/core/*.cpp"
        "${ROOT_DIR}/network/*.cpp"
        "${ROOT_DIR}/database/*.cpp"
        "${ROOT_DIR}/service/*.cpp"
        "${ROOT_DIR}/utils/*.cpp"
    )
    if(new_sources)
        list(APPEND sources ${new_sources})
    else()
        message(WARNING "No source files found in ${ROOT_DIR}")
    endif()
    set(${OUT_VAR} ${sources} PARENT_SCOPE)
endfunction()
