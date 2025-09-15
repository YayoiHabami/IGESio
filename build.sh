#!/bin/bash

set -e

ENABLE_DOCUMENTATION=OFF
ENABLE_SOURCE_BUILD=ON
BUILD_TYPE=debug
BUILD_EXAMPLE=OFF

# Function to display usage
show_usage() {
    echo "Usage1: ./build.sh [debug|release] <doc> <ex>"
    echo "If 'doc' is specified, documentation will be built."
    echo "If 'ex' is specified, example will be built."
    echo "If 'debug' or 'release' is specified, the source will be built."
    echo "Usage2: ./build.sh doc <ex>"
    echo "If 'doc' is specified, only documentation will be built without source."
    exit 1
}

# Check if at least one argument is provided
if [ $# -eq 0 ]; then
    show_usage
fi

# Parse arguments
if [ "$1" = "debug" ] || [ "$1" = "release" ]; then
    BUILD_TYPE=$1
else
    ENABLE_SOURCE_BUILD=OFF
fi

for arg in "$@"; do
    case "$arg" in
        doc)
            ENABLE_DOCUMENTATION=ON
            ;;
        ex)
            BUILD_EXAMPLE=ON
            ;;
        debug|release)
            # Already handled, do nothing
            ;;
        *)
            echo "Error: Invalid argument '$arg'"
            show_usage
            ;;
    esac
done

EXAMPLE_SUFFIX=""
if [ "$BUILD_EXAMPLE" = "ON" ]; then
    EXAMPLE_SUFFIX="_ex"
fi
BUILD_DIR="build/${BUILD_TYPE}${EXAMPLE_SUFFIX}_linux"

if [ "$ENABLE_SOURCE_BUILD" = "ON" ]; then
    echo "Starting source build..."

    # Create a build directory if it doesn't exist
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
        CMAKE_ARGS=("-S" "." "-B" "$BUILD_DIR" "-G" "Ninja" "-DIGESIO_BUILD_EXAMPLE=${BUILD_EXAMPLE}")
        if [ "$BUILD_TYPE" = "debug" ]; then
            CMAKE_ARGS+=("-DIGESIO_BUILD_TESTING=ON" "-DCMAKE_BUILD_TYPE=Debug")
        else
            CMAKE_ARGS+=("-DIGESIO_BUILD_TESTING=OFF" "-DCMAKE_BUILD_TYPE=Release")
        fi
        cmake "${CMAKE_ARGS[@]}"
    fi

    # Build the project using Ninja
    if [ "$ENABLE_DOCUMENTATION" = "ON" ]; then
        cmake --build "$BUILD_DIR" --target doc
    else
        cmake --build "$BUILD_DIR"
    fi
fi

echo "Build completed successfully!"
