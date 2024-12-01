#!/bin/bash
source "$(dirname "$0")/common-utils.sh"

clean_build() {
    log info "Cleaning build directory..."
    if [ -d "${PROJECT_DIR}/build" ]; then
        rm -rf "${PROJECT_DIR}/build/*"
        log info "Build directory cleaned"
    else
        log info "Build directory doesn't exist, skipping cleanup"
    fi
}

build_project() {
    local build_type=$1
    local build_dir="${PROJECT_DIR}/build/$build_type"

    local num_cores
    if command -v nproc >/dev/null 2>&1; then
        num_cores=$(nproc)
    else
        num_cores=4
    fi

    log info "Configuring project with CMake (${build_type})..."
    
    if ! cmake -B "$build_dir" -GNinja \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DUSE_POSTGRESQL=ON \
        -DBUILD_TESTS=OFF \
        -DUSE_CCACHE=ON \
        -DCMAKE_FIND_PACKAGE_PARALLEL_LEVEL="$num_cores"; then
        log error "CMake configuration failed"
        return 1
    fi

    log info "Building project..."
    if ! cmake --build "$build_dir" --parallel "$num_cores"; then
        log error "Build failed"
        return 1
    fi

    log info "Build successful!"
    return 0
}

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

main() {
    local command="${1:-all}"
    local build_type="${2:-Debug}"
    
    case "$build_type" in
        "Debug"|"Release"|"RelWithDebInfo"|"MinSizeRel")
            ;;
        *)
            log error "Invalid build type: $build_type"
            show_help
            exit 1
            ;;
    esac

    case "$command" in
        "clean")
            clean_build
            ;;
        "deps")
            main 
            ;;
        # "proto" команда больше не нужна, так как proto генерируется через CMake
        "build")
            build_project "$build_type"
            ;;
        "test")
            run_tests "$build_type"
            ;;
        "all")
            clean_build && \
            main && \
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

main "$@"
