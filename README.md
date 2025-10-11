# IGESio C++ Library

[![CI](https://github.com/YayoiHabami/IGESio/workflows/CI/badge.svg)](https://github.com/YayoiHabami/IGESio/actions)
[![codecov](https://codecov.io/github/YayoiHabami/IGESio/graph/badge.svg)](https://codecov.io/github/YayoiHabami/IGESio)
[![License](https://img.shields.io/github/license/YayoiHabami/IGESio)](LICENSE)

日本語のドキュメントは [README_ja.md](README_ja.md) をご覧ください。

## Overview

IGESio is a modern C++ library for handling the IGES (Initial Graphics Exchange Specification) file format. This library comprehensively provides functions for input/output, drawing, and manipulation of related data of IGES files based on the IGES 5.3 specification.

The current version can be checked using [`igesio::GetVersion()`](src/common/versions.cpp) (e.g., 0.5.0).

For design principles and IGES specification interpretations, please refer to the [policy documentation (Japanese)](docs/policy_ja.md).

For the current implementation status, please refer to [docs/implementation_progress.md](docs/implementation_progress.md).

<!-- omit in toc -->
## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Usage](#usage)
  - [Example: Integrating with a GUI Application](#example-integrating-with-a-gui-application)
  - [Reading and Writing IGES Files](#reading-and-writing-iges-files)
  - [Creating Entities](#creating-entities)
- [System Requirements](#system-requirements)
  - [Tested Environments](#tested-environments)
  - [Environment Setup](#environment-setup)
    - [Windows Environment](#windows-environment)
    - [Ubuntu Environment](#ubuntu-environment)
  - [Third-Party Dependencies](#third-party-dependencies)
- [Building](#building)
- [Directory Structure](#directory-structure)
- [Documentation](#documentation)
- [Copyright \& License](#copyright--license)

## Key Features

The IGESio library provides the following core functionality:

- **IGES File Input/Output**: Input/output using [`igesio::ReadIges`](src/reader.cpp) and [`igesio::WriteIges`](src/writer.cpp)
- **Entity Support**: Supports various entities from basic entities such as arcs and lines to NURBS curves and surfaces.
  - All entities are implemented as classes with a common interface based on the `EntityBase` class ([Implementation Status](docs/implementation_progress.md) / [Detailed Explanation](docs/entities/entities.md)).
  - IGES files containing unsupported entities can also be read (they are read as [`UnsupportedEntity` class](docs/entities/entities.md#unsupportedentity), and parameters can be obtained/modified).
- **Entity Rendering**: Visualization of entities using OpenGL.
  - Easy integration into GUI applications because GUI-dependent processing is separated.

## Usage

### Example: Integrating with a GUI Application

A simple GUI application example, [IGES viewer](docs/examples.md#gui), demonstrates how to use IGESio to read IGES files and render them with OpenGL. This application uses ImGui and GLFW, with IGESio providing the rendering functionality.

<img src="docs/images/curves_viewer_window.png" alt="IGES Viewer Example" width="600"/>

### Reading and Writing IGES Files

IGESio provides `igesio::ReadIges` and `igesio::WriteIges` functions for reading and writing IGES files. Both functions use the `igesio::models::IgesData` type to represent all IGES file data.

Entities not yet supported by IGESio are loaded as `igesio::entities::UnsupportedEntity`. These entities have their parameters parsed, but do not provide entity-specific functionality. For details, see [entities/UnsupportedEntity](docs/entities/entities.md#UnsupportedEntity).

```cpp
#include <iostream>
#include <unordered_map>
#include <igesio/reader.h>
#include <igesio/writer.h>

// Read IGES file
auto data = igesio::ReadIges("path/to/file.igs");

// Count entities by type and check support
std::unordered_map<igesio::entities::EntityType, int> type_counts;
std::unordered_map<igesio::entities::EntityType, bool> is_supported;
for (const auto& [id, entity] : data.GetEntities()) {
  type_counts[entity->GetType()]++;
  is_supported[entity->GetType()] = entity->IsSupported();
}

// Write IGES file
// Throws DataFormatError if unsupported entities are present
try {
  auto success = igesio::WriteIges(data, "path/to/output.igs");
  if (!success) {
    std::cerr << "Failed to write IGES file." << std::endl;
  }
} catch (const igesio::DataFormatError& e) {
  std::cerr << "Data format error: " << e.what() << std::endl;
}
```

For details on the data structures, see [class structure](docs/class_structure.md). For basic usage, refer to [examples](docs/examples.md) and [entities](docs/entities/entities.md).

### Creating Entities

You can also create entities programmatically. The following example creates a circle with center $(3, 0)$ and radius $1$, sets its color, and writes it to an IGES file. For examples of creating each entity, see [entities](docs/entities/entities.md).

```cpp
#include <memory>
#include <array>
#include <iostream>
#include <igesio/entities/curves/circular_arc.h>
#include <igesio/entities/structures/color_definition.h>
#include <igesio/writer.h>

// Create a Circular Arc entity (center: (3.0, 0.0), radius: 1.0)
auto circle = std::make_shared<igesio::entities::CircularArc>(
    igesio::Vector2d{3.0, 0.0}, 1.0);

// Set color using Color Definition entity (≈ #4C7FFF)
auto color_def = std::make_shared<igesio::entities::ColorDefinition>(
    std::array<double, 3>{30.0, 50.0, 100.0}, "Bright Blue");
circle->OverwriteColor(color_def);

// Create IgesData and add entities
igesio::models::IgesData iges_data;
iges_data.AddEntity(color_def);
iges_data.AddEntity(circle);

// Write to IGES file
auto success = igesio::WriteIges(iges_data, "created_circle.igs");
if (!success) {
  std::cerr << "Failed to write IGES file." << std::endl;
}
```

## System Requirements

To build the IGESio library, you need:

- **C++17 compatible compiler**: Required for modern C++ features
- **CMake 3.14 or later**: Used as the build system

### Tested Environments

This library has been built and tested on the following environments:

| OS | Compilers |
|----|----|
| ![Ubuntu](https://img.shields.io/badge/Ubuntu-latest-orange?logo=ubuntu) | ![GCC](https://img.shields.io/badge/GCC-✓-green) ![Clang](https://img.shields.io/badge/Clang-✓-green) |
| ![Windows](https://img.shields.io/badge/Windows-latest-blue?logo=windows) | ![GCC](https://img.shields.io/badge/GCC-✓-green) ![Clang](https://img.shields.io/badge/Clang-✓-green) ![MSVC](https://img.shields.io/badge/MSVC-✓-green) |
| ![macOS](https://img.shields.io/badge/macOS-latest-lightgrey?logo=apple) <sup>*1</sup> | ![GCC](https://img.shields.io/badge/GCC-✓-green) ![Clang](https://img.shields.io/badge/Clang-✓-green) |

> **Cross-platform Support**: This library has been verified to work on Windows, Ubuntu, and macOS.
>
> - **Other Linux environments**: Should work on similar environments (Linux distributions with GCC and CMake available), though not explicitly tested
> - **Compilers**: Verified to work with GCC, Clang, and MSVC (Windows)

> *1: This library requires OpenGL 4.3 or later, but macOS only supports up to OpenGL 4.1, so most graphics functions are not available.

### Environment Setup

#### Windows Environment

1. Install **MinGW-W64**
2. Install **CMake** (from [official website](https://cmake.org/))
3. Install **Ninja** (optional, only required when using `build.bat`)

#### Ubuntu Environment

```bash
# Install required packages
sudo apt update
sudo apt install build-essential cmake ninja-build

# Verify versions
gcc --version
cmake --version
ninja --version
```

### Third-Party Dependencies

This library includes optional dependencies with compatible licenses:

| Library | License | Usage | Optional |
|---------|---------|-------|----------|
| [Eigen3](https://eigen.tuxfamily.org/) | MPL-2.0 | Linear algebra operations | Yes (`-DIGESIO_ENABLE_EIGEN=OFF` to disable) |
| [Google Test](https://github.com/google/googletest) | BSD-3-Clause | Unit testing | Yes (only when `IGESIO_BUILD_TESTING` is enabled) |
| [glad](https://github.com/Dav1dde/glad) | MIT, Apache-2.0 | OpenGL loader | Yes (only when `IGESIO_ENABLE_GRAPHICS` or `IGESIO_BUILD_GUI` is enabled) |
| [glfw](https://www.glfw.org/) | Zlib | Window creation and input handling | Yes (only when `IGESIO_BUILD_GUI` is enabled) |
| [imgui](https://github.com/ocornut/imgui) | MIT | Graphical user interface | Yes (only when `IGESIO_BUILD_GUI` is enabled) |
| [stb](https://github.com/nothings/stb) | MIT | Image loading and saving | Yes (only when `IGESIO_ENABLE_TEXTURE_IO` is enabled) |

**License Compatibility**: All dependencies use licenses compatible with MIT. See [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES.md) for full license texts.

**Note**:
- Eigen is header-only and only included when explicitly enabled
- Google Test is only used for development and not distributed with the library
- glad is included only when `IGESIO_ENABLE_GRAPHICS` or `IGESIO_BUILD_GUI` is enabled. glad's source code is licensed under MIT license, and Khronos XML API Registry is licensed under Apache License 2.0
- This library can be built without any third-party dependencies

## Building

The IGESio library can be easily integrated into your project using CMake's `FetchContent`. For example, you can add IGESio as a dependency by modifying your `CMakeLists.txt` as follows:

````cmake
cmake_minimum_required(VERSION 3.16)
project(my_project)

# Fetch IGESio using FetchContent
include(FetchContent)
FetchContent_Declare(
  igesio
  GIT_REPOSITORY https://github.com/YayoiHabami/IGESio.git
  GIT_TAG main
)

# Enable IGESio with Eigen and OpenGL support
set(IGESIO_ENABLE_EIGEN ON)
set(IGESIO_ENABLE_GRAPHICS ON)
FetchContent_MakeAvailable(igesio)

# Create an executable
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE IGESio::IGESio)
````

For more detailed information and CMake options, refer to [docs/build.md](docs/build.md).

You can also clone the repository and build it standalone for running examples or tests. On Windows, use the `build.bat` script, and on Linux, use the `build.sh` script.

````bat
.\build.bat debug ex
````

```bash
./build.sh debug ex
```

See [docs/build.md](docs/build.md) for details.

## Directory Structure

```
IGESio/
├── build.bat, build.sh     # Build scripts for Windows and Linux
├── CMakeLists.txt          # Main CMake build script
├── ...
├── examples/               # Example usage
│   └── gui/                # Examples with GUI
├── include/                # Public header files
│   └── igesio/
├── src/                    # Source files
│   ├── common/             # Common modules (metadata, error handling, etc.)
│   ├── entities/           # Entity-related modules
│   ├── graphics/           # Graphics-related modules (OpenGL; does not include GUI)
│   ├── models/             # Data model modules
│   ├── utils/              # Utility modules
│   ├── reader.cpp          # IGES file reading implementation
│   └── writer.cpp          # IGES file writing implementation
├── tests/                  # Test code
│   └── test_data/          # Test data (IGES files, etc.)
├── docs/                   # Documentation
└── build/                  # Build artifacts (generated)
```

## Documentation

The documentation is available in the `docs` directory. See [docs/index.md](docs/index.md) for an overview and links to specific documents.

## Copyright & License

This library is provided under the MIT License. See the [LICENSE](LICENSE) file for details. For third-party dependencies used by this library, please refer to [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES.md).

&copy; 2025 Yayoi Habami
