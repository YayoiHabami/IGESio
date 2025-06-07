# IGESio C++ Library

日本語のドキュメントは [README_ja.md](README_ja.md) をご覧ください。

## Overview

IGESio is a modern, cross-platform C++ library for handling IGES (Initial Graphics Exchange Specification) files. It provides comprehensive functionality for reading, writing, and manipulating IGES files in accordance with the IGES 5.3 specification.

The current version can be checked using [`igesio::GetVersion()`](src/common/versions.cpp) (e.g., 0.1.0).

For design principles and IGES specification interpretations, please refer to the [policy documentation (Japanese)](docs/policy_ja.md).

<!-- omit in toc -->
## Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Usage Examples](#usage-examples)
  - [Basic Read/Write Operations](#basic-readwrite-operations)
- [System Requirements](#system-requirements)
  - [Tested Environments](#tested-environments)
  - [Environment Setup](#environment-setup)
    - [Windows Environment](#windows-environment)
    - [Ubuntu Environment](#ubuntu-environment)
  - [Third-Party Dependencies](#third-party-dependencies)
- [Building](#building)
  - [Platform-Specific Alternatives](#platform-specific-alternatives)
- [Running Tests](#running-tests)
  - [Test Build Configuration](#test-build-configuration)
  - [How to Run Tests](#how-to-run-tests)
- [Directory Structure](#directory-structure)
- [Documentation](#documentation)
- [Copyright \& License](#copyright--license)

## Key Features

The IGESio library provides the following core functionality:

- **IGES File Reading**: Parse and load IGES files using [`igesio::ReadIges`](src/reader.cpp)
- **IGES File Writing**: Generate and output IGES files using [`igesio::WriteIges`](src/writer.cpp)
- **Entity Data Management**: Efficient management and manipulation of IGES entities
- **Global Parameter Control**: Manage global section parameters via [`igesio::models::GlobalParam`](include/igesio/models/global_param.h)

## Usage Examples

### Basic Read/Write Operations

```cpp
#include <igesio/reader.h>

int main() {
    std::string file_name = "example.igs";
    // TODO: Update this implementation after defining IGESData class

    return 0;
}
```

## System Requirements

To build the IGESio library, you need:

- **C++17 compatible compiler**: Required for modern C++ features
- **CMake 3.14 or later**: Used as the build system

### Tested Environments

This library has been built and tested on the following environments:

| Environment | OS | Compiler | CMake | Ninja |
|:-----------:|----|---------:|------:|------:|
| Windows | Windows 11 | GCC 11.2.0 | 3.27.0-rc2 | 1.10.2 |
| Linux<br>(Ubuntu) | Ubuntu 22.04 LTS<br>(including WSL2) | GCC 11.4.0 | 3.22.1 | 1.10.1 |

> **Cross-platform Support**: This library has been verified to work on Windows and Ubuntu.
>
> - **Other Linux environments**: Should work on similar environments (Linux distributions with GCC and CMake available), though not explicitly tested
> - **Version compatibility**: Newer versions than those listed should generally work, but older versions may have compatibility issues
> - **macOS**: Should work but may require adjustments depending on your environment

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

For more details, see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

## Building

Follow these steps to build the IGESio library:

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

### Platform-Specific Alternatives

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

## Running Tests

### Test Build Configuration

Tests are not built by default. Use the `IGESIO_BUILD_TESTING` option to control this:

```bash
# Build with tests
cmake -DIGESIO_BUILD_TESTING=ON ..

# Build without tests (default)
cmake ..
```

### How to Run Tests

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

## Copyright & License

This library is provided under the MIT License. See the [LICENSE](LICENSE) file for details.

&copy; 2025 Yayoi Habami

For detailed copyright information on individual source files, refer to the header comments in each file (e.g., [`include/igesio/common/iges_metadata.h`](include/igesio/common/iges_metadata.h)).
