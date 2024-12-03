if(ENABLE_STATIC_ANALYSIS)
    # Настройка cppcheck
    find_program(CPPCHECK cppcheck)
    if(CPPCHECK)
        list(APPEND CPPCHECK_ARGS
            "--enable=all"
            "--inconclusive"
            "--force"
            "--inline-suppr"
            "--suppress=missingIncludeSystem"
            "-I${CMAKE_SOURCE_DIR}/include"
        )
        
        add_custom_target(
            cppcheck
            COMMAND ${CPPCHECK} ${CPPCHECK_ARGS} ${CMAKE_SOURCE_DIR}/src
            COMMENT "Running cppcheck"
        )
    endif()

    # Настройка clang-tidy
    find_program(CLANG_TIDY clang-tidy)
    if(CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY 
            ${CLANG_TIDY};
            -checks=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type;
            -header-filter=.;
        )
    endif()

    # Настройка clang-format
    find_program(CLANG_FORMAT "clang-format")
    if(CLANG_FORMAT)
        file(GLOB_RECURSE ALL_SOURCE_FILES 
            ${PROJECT_SOURCE_DIR}/src/*.cpp
            ${PROJECT_SOURCE_DIR}/include/*.hpp
            ${PROJECT_SOURCE_DIR}/tests/*.cpp
        )

        add_custom_target(
            format
            COMMAND ${CLANG_FORMAT}
            -i
            ${ALL_SOURCE_FILES}
        )
    endif()
else()
    # Если статический анализ отключен, создаем пустые таргеты
    add_custom_target(cppcheck
        COMMAND ${CMAKE_COMMAND} -E echo "Static analysis is disabled. Enable with -DENABLE_STATIC_ANALYSIS=ON"
    )
    
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "Static analysis is disabled. Enable with -DENABLE_STATIC_ANALYSIS=ON"
    )
endif()