#!/bin/bash
source "$(dirname "$0")/common-utils.sh"

# Очистка сборочной директории
clean_build() {
    log info "Cleaning build directory..."
    if [ -d "${PROJECT_DIR}/build" ]; then
        rm -rf "${PROJECT_DIR}/build/*"
        log info "Build directory cleaned"
    else
        log info "Build directory doesn't exist, skipping cleanup"
    fi
}

# Генерация proto файлов
generate_proto() {
    log info "Generating proto files..."

    local proto_dir="${PROJECT_DIR}/proto/v1"
    local output_dir="${PROJECT_DIR}/build/generated"

    mkdir -p "$output_dir"

    for proto_file in "$proto_dir"/*.proto; do
        if [ ! -f "$proto_file" ]; then
            log warn "No proto files found in $proto_dir"
            return 0
        fi

        log info "Processing $(basename "$proto_file")..."

        if ! protoc \
            --proto_path="$proto_dir" \
            --cpp_out="$output_dir" \
            --grpc_out="$output_dir" \
            --plugin=protoc-gen-grpc="$(which grpc_cpp_plugin)" \
            "$proto_file"; then
            log error "Failed to generate proto files for $(basename "$proto_file")"
            return 1
        fi
    done

    log info "Proto files generated successfully"
    return 0
}

# Сборка проекта
build_project() {
    local build_type=$1
    local build_dir="${PROJECT_DIR}/build/$build_type"

    log info "Configuring project with CMake (${build_type})..."
    
    if ! cmake -B "$build_dir" -GNinja \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DUSE_POSTGRESQL=ON \
        -DBUILD_TESTS=OFF; then
        log error "CMake configuration failed"
        return 1
    fi

    log info "Building project..."
    if ! cmake --build "$build_dir" --parallel; then
        log error "Build failed"
        return 1
    fi

    log info "Build successful!"
    return 0
}

# Запуск тестов
run_tests() {
    local build_type=$1
    local build_dir="${PROJECT_DIR}/build/$build_type"

    if [ ! -d "$build_dir" ]; then
        log error "Build directory does not exist: $build_dir"
        return 1
    fi

    log info "Running tests..."
    cd "$build_dir" || { log error "Failed to change to build directory"; return 1; }

    if ! ctest --output-on-failure; then
        log error "Tests failed"
        return 1
    fi

    log info "All tests passed!"
    return 0
}

show_help() {
    echo "Usage: $0 [command] [build_type]"
    echo
    echo "Commands:"
    echo "  clean         Clean build directory"
    echo "  deps          Install dependencies"
    echo "  proto         Generate protocol buffers"
    echo "  build         Build project"
    echo "  test          Run tests"
    echo "  all           Execute all steps (clean, deps, proto, build, test)"
    echo
    echo "Build Types:"
    echo "  Debug         (default)"
    echo "  Release"
    echo "  RelWithDebInfo"
    echo "  MinSizeRel"
    echo
    echo "Examples:"
    echo "  $0 build Release     # Build in Release mode"
    echo "  $0 all Debug        # Full cycle in Debug mode"
}

# Основная функция
main() {
    local command="${1:-all}"
    local build_type="${2:-Debug}"
    
    # Проверка корректности типа сборки
    case "$build_type" in
        "Debug"|"Release"|"RelWithDebInfo"|"MinSizeRel")
            ;;
        *)
            log error "Invalid build type: $build_type"
            show_help
            exit 1
            ;;
    esac

    # Выполнение команды
    case "$command" in
        "clean")
            clean_build
            ;;
        "deps")
            main # вызываем main из install_dependencies.sh
            ;;
        "proto")
            generate_proto
            ;;
        "build")
            build_project "$build_type"
            ;;
        "test")
            run_tests "$build_type"
            ;;
        "all")
            clean_build && \
            main && \       # установка зависимостей
            generate_proto && \
            build_project "$build_type" && \
            run_tests "$build_type"
            ;;
        "help"|"--help"|"-h")
            show_help
            ;;
        *)
            log error "Invalid command: $command"
            show_help
            exit 1
            ;;
    esac
}

# Запуск скрипта
main "$@"
