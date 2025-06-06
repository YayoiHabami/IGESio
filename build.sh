#!/bin/bash

set -e

ENABLE_DOCUMENTATION=OFF
ENABLE_SOURCE_BUILD=ON
BUILD_TYPE=debug

# Function to display usage
show_usage() {
    echo "Usage1: ./build.sh [debug|release] [doc]"
    echo "If 'doc' is specified, documentation will be built."
    echo "If 'debug' or 'release' is specified, the source will be built."
    echo "Usage2: ./build.sh doc"
    echo "If 'doc' is specified, only documentation will be built without source."
    exit 1
}

# Check if at least one argument is provided
if [ $# -eq 0 ]; then
    show_usage
fi

# Parse arguments
if [ "$1" = "debug" ]; then
    BUILD_TYPE=debug
    if [ "$2" = "doc" ]; then
        ENABLE_DOCUMENTATION=ON
    fi
elif [ "$1" = "release" ]; then
    BUILD_TYPE=release
    if [ "$2" = "doc" ]; then
        ENABLE_DOCUMENTATION=ON
    fi
elif [ "$1" = "doc" ]; then
    # If the first argument is "doc", enable documentation and disable source build
    ENABLE_DOCUMENTATION=ON
    ENABLE_SOURCE_BUILD=OFF
else
    echo "Error: Invalid argument '$1'"
    show_usage
fi

BUILD_DIR="build/${BUILD_TYPE}_linux"

if [ "$ENABLE_SOURCE_BUILD" = "ON" ]; then
    echo "Starting source build..."

    # Create a build directory if it doesn't exist
    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
        if [ "$BUILD_TYPE" = "debug" ]; then
            cmake -S . -B "$BUILD_DIR" -G "Ninja" -DIGESIO_BUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
        else
            cmake -S . -B "$BUILD_DIR" -G "Ninja" -DIGESIO_BUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
        fi
    fi

    # Build the project using Ninja
    if [ "$ENABLE_DOCUMENTATION" = "ON" ]; then
        cmake --build "$BUILD_DIR" --target doc
    else
        cmake --build "$BUILD_DIR"
    fi
fi

echo "Build completed successfully!"