# IGESio C++ Library

[![CI](https://github.com/YayoiHabami/IGESio/workflows/CI/badge.svg)](https://github.com/YayoiHabami/IGESio/actions)
[![codecov](https://codecov.io/github/YayoiHabami/IGESio/graph/badge.svg)](https://codecov.io/github/YayoiHabami/IGESio)
[![License](https://img.shields.io/github/license/YayoiHabami/IGESio)](LICENSE)

日本語のドキュメントは [README_ja.md](README_ja.md) をご覧ください。

## Overview

IGESio is a modern, cross-platform C++ library for handling IGES (Initial Graphics Exchange Specification) files. It provides comprehensive functionality for reading, writing, and manipulating IGES files in accordance with the IGES 5.3 specification.

The current version can be checked using [`igesio::GetVersion()`](src/common/versions.cpp) (e.g., 0.3.1).

For design principles and IGES specification interpretations, please refer to the [policy documentation (Japanese)](docs/policy_ja.md).

<!-- omit in toc -->
## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Usage Examples](#usage-examples)
  - [Basic Read/Write Operations](#basic-readwrite-operations)
    - [Reading and Writing with Intermediate Data Structure](#reading-and-writing-with-intermediate-data-structure)
    - [Why Use an Intermediate Data Structure?](#why-use-an-intermediate-data-structure)
    - [Important Notes](#important-notes)
- [System Requirements](#system-requirements)
  - [Tested Environments](#tested-environments)
  - [Environment Setup](#environment-setup)
    - [Windows Environment](#windows-environment)
    - [Ubuntu Environment](#ubuntu-environment)
  - [Third-Party Dependencies](#third-party-dependencies)
- [Building](#building)
  - [CMake Project Integration](#cmake-project-integration)
    - [Component Names for CMake Linking](#component-names-for-cmake-linking)
  - [Standalone Building](#standalone-building)
    - [Platform-Specific Alternatives](#platform-specific-alternatives)
    - [Running Tests](#running-tests)
- [Directory Structure](#directory-structure)
- [Documentation](#documentation)
  - [File-Specific Documentation](#file-specific-documentation)
- [Copyright \& License](#copyright--license)

## Key Features

The IGESio library provides the following core functionality:

- **IGES File Reading**: Parse and load IGES files using [`igesio::ReadIges`](src/reader.cpp)
- **IGES File Writing**: Generate and output IGES files using [`igesio::WriteIges`](src/writer.cpp)
- **Entity Data Management**: Efficient management and manipulation of IGES entities
- **Global Parameter Control**: Manage global section parameters via [`igesio::models::GlobalParam`](include/igesio/models/global_param.h)

## Usage Examples

### Basic Read/Write Operations

The IGESio library employs a two-stage conversion process for reading IGES files:

1. **IGES File → Intermediate Data Structure** (`IntermediateIgesData`)
2. **Intermediate Data Structure → Data Class** (`IGESData` class - under development)

#### Reading and Writing with Intermediate Data Structure

Currently available method allows reading and writing IGES files using the intermediate data structure (`IntermediateIgesData`). For detailed information, please refer to the [intermediate data structure documentation](docs/intermediate_data_structure.md).

```cpp
#include <igesio/reader.h>
#include <igesio/writer.h>

int main() {
  try {
    // Read IGES file into intermediate data structure
    auto data = igesio::ReadIgesIntermediate("input.igs");

    // Modify data as needed
    // ...

    // Write modified data to a new file
    igesio::WriteIgesIntermediate(data, "output.igs");
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
```

#### Why Use an Intermediate Data Structure?

Reasons for adopting the two-stage approach:

- **IGES Format Complexity**: Processes conversion between raw data in IGES files and practical data models in stages
- **Error Handling Separation**: Clearly distinguishes between file parsing errors and data structure conversion errors
- **Development Flexibility**: Minimizes impact of design changes to the final `IGESData` class

#### Important Notes

> **Warning**: The intermediate data structure (`IntermediateIgesData`) is an internal implementation detail and may change in future versions.
>
> For production use, we strongly recommend using the planned `IGESData` class once completed:
>
> ```cpp
> // Future API (under development)
> auto iges_data = igesio::ReadIges("example.igs");  // Returns IGESData class
> igesio::WriteIges(iges_data, "output.igs");
> ```
>
> Please limit the use of intermediate data structure to development/debugging purposes or temporary usage until the `IGESData` class is completed.

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
| ![macOS](https://img.shields.io/badge/macOS-latest-lightgrey?logo=apple) | ![GCC](https://img.shields.io/badge/GCC-✓-green) ![Clang](https://img.shields.io/badge/Clang-✓-green) |

> **Cross-platform Support**: This library has been verified to work on Windows, Ubuntu, and macOS.
>
> - **Other Linux environments**: Should work on similar environments (Linux distributions with GCC and CMake available), though not explicitly tested
> - **Compilers**: Verified to work with GCC, Clang, and MSVC (Windows)

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
| [Eigen3](https://eigen.tuxfamily.org/) | MPL-2.0 | Linear algebra operations | Yes (disable with `-DENABLE_EIGEN=OFF`) |
| [Google Test](https://github.com/google/googletest) | BSD-3-Clause | Unit testing | Yes (only when `IGESIO_BUILD_TESTING` is enabled) |

**License Compatibility**: All dependencies use licenses compatible with MIT. See [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES) for full license texts.

**Note**:
- Eigen is header-only and only included when explicitly enabled
- Google Test is only used for development and not distributed with the library
- You can build this library without any third-party dependencies


## Building

### CMake Project Integration

The IGESio library can be easily integrated into other projects using CMake's `FetchContent` feature. Here's an example CMake configuration for integrating the IGESio library into `my_project`, which builds a `main.cpp` file as an executable:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable FetchContent
include(FetchContent)

# Fetch IGESio library
FetchContent_Declare(
  igesio
  GIT_REPOSITORY https://github.com/YayoiHabami/IGESio
  GIT_TAG main  # You can also specify a specific tag or commit hash
)

# Make IGESio available
FetchContent_MakeAvailable(igesio)

# Create executable
add_executable(my_app main.cpp)

# Link IGESio library
target_link_libraries(my_app PRIVATE IGESio::IGESio)
```

**Note**: When using FetchContent, IGESio tests are not built by default. This reduces build time for user projects.

#### Component Names for CMake Linking

The IGESio library can be linked using the following component names. Choose the appropriate component based on your needs:

| Component Name | Description | Use Case |
|----------------|-------------|----------|
| `IGESio::IGESio` | Main library containing all IGESio functionality | General use (recommended) |
| `IGESio::common` | Common modules (metadata, error handling, etc.) | When only basic functionality is needed |
| `IGESio::utils` | Utilities like data type conversion | When only utility functions are needed |
| `IGESio::entities` | IGES entity-related functionality | When only entity processing is needed |
| `IGESio::models` | Intermediate data structures and overall IGES data management | When only data model functionality is needed |

**Note**: We generally recommend linking with `IGESio::IGESio`. Individual components should only be considered when you need specific functionality only or want to minimize dependencies.

### Standalone Building

For development or testing purposes, you can build the IGESio library individually by following these steps:

**1. Clone the repository**:
```bash
git clone https://github.com/YayoiHabami/IGESio.git
cd IGESio
```

**2. Create build directory**:
```bash
mkdir build
cd build
```

**3. Run CMake and build**:
```bash
cmake ..
cmake --build .
```

#### Platform-Specific Alternatives

**Windows Environment**: You can also use the `build.bat` script. However, this requires Ninja to be installed and added to your PATH in addition to CMake. You can specify either `debug` or `release` options during build. Additionally, specifying `doc` as the second argument will generate documentation, which requires Doxygen and Graphviz to be installed.

```bat
.\build.bat debug
```

**Linux Environment**: You can also use the `build.sh` script. CMake and Ninja are required. Grant execution permissions before the first run. Other options are the same as Windows.

```bash
# Grant execution permissions
chmod +x build.sh

# Run the script
./build.sh debug
```

#### Running Tests

Even when building standalone, tests are not built by default. Use the `IGESIO_BUILD_TESTING` option to control this:

```bash
# Build with tests
cmake -DIGESIO_BUILD_TESTING=ON ..

# Build without tests (default)
cmake ..
```

After building, you can run tests using the following methods. Navigate to the build output directory first (`build\debug_win` for Windows, `build/debug_linux` for Linux).

**Using CTest**:
```bash
ctest
```

**Using individual test executables**:
Run each test executable directly (e.g., `test_common`, `test_utils`, `test_entities`, `test_models`, `test_igesio`).

For detailed test definitions, refer to the `CMakeLists.txt` files in each `tests` subdirectory (e.g., [tests/CMakeLists.txt](tests/CMakeLists.txt), [tests/common/CMakeLists.txt](tests/common/CMakeLists.txt)).

## Directory Structure

```
IGESio/
├── build.bat, build.sh     # Build scripts for Windows and Linux
├── CMakeLists.txt          # Main CMake build script
├── include/                # Public header files
│   └── igesio/
├── src/                    # Source files
│   ├── common/             # Common modules (metadata, error handling, etc.)
│   ├── entities/           # Entity-related modules
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

Detailed project documentation is organized in the `docs` directory:

- **[policy (ja)](docs/policy_ja.md)**: Library design principles and IGES specification interpretations
- **[class-reference (ja)](docs/class_reference_ja.md)**: Reference for classes used in this library
- **[flow/reader (ja)](docs/flow/reader_ja.md)**: Documentation about the reading process flow
- **[entity-analysis (en)](docs/entity_analysis.md)**: Analysis of entity classifications and parameters in IGES 5.3
- **[additional-notes (ja)](docs/additional_notes_ja.md)**: Additional notes and supplementary information
- **[todo (ja)](docs/todo.md)**: TODO list

### File-Specific Documentation

The following documentation is available for individual source files. For files not listed below, please refer to the comments within the source code as documentation has not been created yet.

**common module**

- [Matrix](docs/common/matrix_ja.md): Fixed and dynamic size matrix classes

**entities module**

- [IEntity and derived classes](docs/entities/entity_base_class_architecture_ja.md): Design of `IEntity` class and its derived classes
- [RawEntityDE and RawEntityPD](docs/intermediate_data_structure_ja.md): Intermediate data structures for IGES file Directory Entry and Parameter Data sections

**models module**

- [Intermediate](docs/intermediate_data_structure_ja.md): Intermediate data structures for IGES file input/output operations

**utils module**

## Copyright & License

This library is provided under the MIT License. See the [LICENSE](LICENSE) file for details.

&copy; 2025 Yayoi Habami

For detailed copyright information on individual source files, refer to the header comments in each file (e.g., [`include/igesio/common/iges_metadata.h`](include/igesio/common/iges_metadata.h)).
