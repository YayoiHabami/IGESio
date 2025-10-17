# Build Instructions

This library can be easily integrated into other projects using CMake's `FetchContent` feature. It can also be built individually for development or testing purposes. Below, we explain these methods.

## Table of Contents

- [Table of Contents](#table-of-contents)
  - [CMake Project Integration](#cmake-project-integration)
    - [Component Names for CMake Linking](#component-names-for-cmake-linking)
    - [CMake Options](#cmake-options)
  - [Standalone Building](#standalone-building)
    - [Platform-Specific Alternatives](#platform-specific-alternatives)
    - [Running Tests](#running-tests)

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
  GIT_REPOSITORY https://github.com/YayoiHabami/IGESio.git
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

| Component Name | Description |
|----------------|-------------|
| `IGESio::IGESio` | Main library containing all IGESio functionality <br> General use (recommended) |
| `IGESio::common` | Common modules (metadata, error handling, etc.) |
| `IGESio::utils` | Utilities like data type conversion |
| `IGESio::entities` | IGES entity-related functionality |
| `IGESio::models` | Intermediate data structures and overall IGES data management |
| `IGESio::graphics` | Graphics functionality (OpenGL)<br>Does not include GUI |

**Note**: We generally recommend linking with `IGESio::IGESio`. Individual components should only be considered when you need specific functionality only or want to minimize dependencies.

#### CMake Options

The IGESio library provides the following CMake options to customize the build process:

| Option | Description | Default |
|--------|-------------|---------|
| `IGESIO_BUILD_DOCS` | Build documentation | OFF |
| `IGESIO_BUILD_EXAMPLE` | Build examples | OFF |
| `IGESIO_BUILD_GUI` | Build GUI example using GLFW and ImGui<br>Note: When this option is ON, `IGESIO_ENABLE_GRAPHICS`, `IGESIO_ENABLE_TEXTURE_IO` are also enabled<br>Note: Requires Python environment and jinja2 installation | OFF |
| `IGESIO_BUILD_TESTING` | Build tests | OFF |
| `IGESIO_ENABLE_COVERAGE` | Enable coverage reporting | OFF |
| `IGESIO_ENABLE_EIGEN` | Enable Eigen support | OFF |
| `IGESIO_ENABLE_GRAPHICS` | Enable OpenGL (glad) support | OFF |
| `IGESIO_ENABLE_TEXTURE_IO` | Enable image file I/O support | OFF |

These options must be set before making IGESio available with `FetchContent_MakeAvailable`. They can also be specified as CMake command-line arguments (e.g., `cmake -DIGESIO_BUILD_TESTING=ON ..`).

```cmake
# ...
FetchContent_Declare(
    igesio
    GIT_REPOSITORY https://github.com/YayoiHabami/IGESio.git
)

# Enable Eigen and OpenGL support
set(IGESIO_ENABLE_EIGEN ON)
set(IGESIO_ENABLE_GRAPHICS ON)

# Make IGESio available
FetchContent_MakeAvailable(igesio)

# ...
```

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

When using the build scripts, the options are as follows:

**(1)**: `build.bat [debug|release] <doc> <ex>`

- Specifying `debug` or `release` builds the source code.
- Specifying `doc` also builds the documentation (requires Doxygen and Graphviz).
- Specifying `ex` also builds the examples.

**(2)**: `build.bat doc`

- Specifying only `doc` builds the documentation without building the source code.

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
