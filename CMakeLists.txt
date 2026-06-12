# CMakeLists.txt for IGESio project
#
# Options:
#   - IGESIO_BUILD_DOCS: Build the documentation (default: OFF)
#   - IGESIO_BUILD_EXAMPLE: Build the examples (default: OFF)
#   - IGESIO_BUILD_GUI: Build GUI (with GLFW and ImGui) as examples (default: OFF)
#     - if IGESIO_BUILD_GUI is ON, IGESIO_ENABLE_GRAPHICS is also set to ON
#     - if IGESIO_BUILD_GUI is ON, IGESIO_ENABLE_TEXTURE_IO is also set to ON
#   - IGESIO_BUILD_TESTING: Build the tests (default: OFF)
#   - IGESIO_ENABLE_COVERAGE: Enable coverage reporting (default: OFF)
#   - IGESIO_ENABLE_EIGEN: Enable Eigen support (default: ON)
#       - Eigen is currently required for the project, so this option cannot be set to OFF.
#       - This option is provided for future flexibility in case we want to make Eigen optional later on, but for now it must be ON.
#   - IGESIO_ENABLE_GRAPHICS: Enable OpenGL (glad) support (default: OFF)
#   - IGESIO_ENABLE_TEXTURE_IO: Enable texture I/O support (default: OFF)
#
# Extension options:
#   - IGESIO_ENABLE_ALL_EXTENSIONS: Enable all extensions (default: OFF)
#   - IGESIO_ENABLE_STL_EXTENSION: Enable the STL (stereolithography) file I/O extension (default: OFF)
#   - IGESIO_ENABLE_OBJ_EXTENSION: Enable the OBJ (Wavefront OBJ) file I/O extension (default: OFF)
#   - IGESIO_ENABLE_INSPECTION_EXTENSION: Enable the inspection extension (coordinate frame groups etc.) (default: OFF)
#
# Defined macros:
#   - IGESIO_ENABLE_BOOST: Defined when Boost support is enabled
#   - IGESIO_STL_EXTENSION_ENABLED: Defined when the STL extension is enabled
#   - IGESIO_OBJ_EXTENSION_ENABLED: Defined when the OBJ extension is enabled
#   - IGESIO_INSPECTION_EXTENSION_ENABLED: Defined when the inspection extension is enabled
cmake_minimum_required(VERSION 3.14)

# Set CMP0135 policy to control timestamp handling for FetchContent archive extraction
# when CMake version 3.24 or later is available
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
    set(DOWNLOAD_EXTRACT_TIMESTAMP_OPTION DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
else()
    set(DOWNLOAD_EXTRACT_TIMESTAMP_OPTION "")
endif()

# Get version from Git tag
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
        ERROR_VARIABLE GIT_ERROR
    )

    if(GIT_RESULT EQUAL 0 AND GIT_TAG MATCHES "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)")
        set(VERSION_MAJOR ${CMAKE_MATCH_1})
        set(VERSION_MINOR ${CMAKE_MATCH_2})
        set(VERSION_PATCH ${CMAKE_MATCH_3})
        set(PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
    else()
        # Fallback to default version if no tag or invalid format
        set(VERSION_MAJOR 0)
        set(VERSION_MINOR 1)
        set(VERSION_PATCH 0)
        set(PROJECT_VERSION "0.1.0")
        message(WARNING "No valid Git tag found (${GIT_ERROR}), using default version ${PROJECT_VERSION}")
    endif()
else()
    # Fallback when Git is not available
    set(VERSION_MAJOR 0)
    set(VERSION_MINOR 1)
    set(VERSION_PATCH 0)
    set(PROJECT_VERSION "0.1.0")
    message(WARNING "Git not found, using default version ${PROJECT_VERSION}")
endif()

# C is enabled on demand (enable_language) only when building glad
# (IGESIO_ENABLE_GRAPHICS); everything else is pure C++.
project(IGESio VERSION ${PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Threads (std::thread/std::async backend for igesio/common/parallel.h).
# Links pthread on Linux (GCC/Clang); a no-op on MSVC and macOS.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Option to enable coverage
option(IGESIO_ENABLE_COVERAGE "Enable coverage reporting" OFF)

# Coverage configuration
if(IGESIO_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # -fprofile-update=atomic makes gcov counter increments atomic, which is
        # required because the read pipeline runs instrumented code on multiple
        # threads; the default (single) update mode races and produces corrupted
        # (negative) counts that lcov/geninfo rejects.
        add_compile_options(--coverage -fprofile-update=atomic)
        add_link_options(--coverage)
        message(STATUS "Coverage reporting enabled")
    else()
        message(WARNING "Coverage reporting is only supported with GCC")
    endif()
endif()

# Set the build type if not specified (top-level builds only; when consumed
# via FetchContent/add_subdirectory, follow the consumer's configuration)
if(NOT CMAKE_BUILD_TYPE AND CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# Variables for source and include directories
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(IGESIO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")

# Create the binary include directory
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/igesio/common")

# Generate version header file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/igesio/common/versions.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/igesio/common/versions.h"
    @ONLY
)



# Helpers to temporarily force the standard CTest variable BUILD_TESTING to
# OFF while fetching third-party dependencies, then restore the previous
# state. Forcing it OFF permanently would also disable the tests of any
# project that consumes IGESio via FetchContent.
macro(igesio_disable_build_testing)
    if(DEFINED CACHE{BUILD_TESTING})
        set(_igesio_saved_build_testing "$CACHE{BUILD_TESTING}")
    endif()
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
endmacro()
macro(igesio_restore_build_testing)
    if(DEFINED _igesio_saved_build_testing)
        set(BUILD_TESTING "${_igesio_saved_build_testing}" CACHE BOOL "" FORCE)
        unset(_igesio_saved_build_testing)
    else()
        unset(BUILD_TESTING CACHE)
    endif()
endmacro()

# Option to enable Eigen support
option(IGESIO_ENABLE_EIGEN "Enable Eigen support" ON)
# Currently, Eigen is required for the project
if (NOT IGESIO_ENABLE_EIGEN)
    message(FATAL_ERROR "Eigen support is currently required for IGESio. Please enable IGESIO_ENABLE_EIGEN option.")
else()
  # Prefer a locally installed Eigen; fetch it only as a fallback.
  # NOTE: No minimum version is enforced here because Eigen's config version
  # file rejects different major versions (requiring 3.x would reject 5.x).
  find_package(Eigen3 QUIET CONFIG)
  if (Eigen3_FOUND)
    set(IGESIO_EIGEN_PROVIDER "system")
    message(STATUS "Eigen found locally: ${Eigen3_DIR}")
  else()
    set(IGESIO_EIGEN_PROVIDER "fetched")
    message(STATUS "Eigen not found locally. Fetching via FetchContent...")
    include(FetchContent)

    # Disable Eigen's internal testing and documentation to speed up the build process
    set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(EIGEN_BUILD_DOC     OFF CACHE BOOL "" FORCE)
    igesio_disable_build_testing()

    # Fetch Eigen
    FetchContent_Declare(
      Eigen
      URL https://gitlab.com/libeigen/eigen/-/archive/5.0.1/eigen-5.0.1.zip
      ${DOWNLOAD_EXTRACT_TIMESTAMP_OPTION}
    )
    FetchContent_MakeAvailable(Eigen)
    igesio_restore_build_testing()
  endif()

  message(STATUS "Eigen support enabled")
endif()

# Fetch Boost (Headers only)
find_package(Boost QUIET)
if (NOT Boost_FOUND)
  message(STATUS "Boost not found locally. Fetching via FetchContent...")

  # Use Math library from Boost
  set(BOOST_INCLUDE_LIBRARIES math)
  set(BOOST_ENABLE_MPI     OFF CACHE BOOL "" FORCE)
  set(BOOST_ENABLE_PYTHON  OFF CACHE BOOL "" FORCE)
  set(BOOST_SKIP_INSTALL_RULES ON CACHE BOOL "" FORCE)

  include(FetchContent)
  igesio_disable_build_testing()
  FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.gz
    ${DOWNLOAD_EXTRACT_TIMESTAMP_OPTION}
  )
  FetchContent_MakeAvailable(Boost)
  igesio_restore_build_testing()
  message(STATUS "Boost fetched successfully.")
else()
  message(STATUS "Boost found locally: ${Boost_INCLUDE_DIRS}")

  if(NOT TARGET Boost::headers)
    add_library(Boost::headers INTERFACE IMPORTED)
    set_target_properties(Boost::headers PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
    )
  endif()
  if(NOT TARGET Boost::math)
      add_library(Boost::math INTERFACE IMPORTED)
      set_target_properties(Boost::math PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
      )
  endif()
endif()
add_compile_definitions(IGESIO_ENABLE_BOOST)
message(STATUS "Boost support enabled")



# Option to enable OpenGL (glad) support
option(IGESIO_ENABLE_GRAPHICS "Enable OpenGL (glad) support" OFF)

# Option to enable texture I/O support (stb)
option(IGESIO_ENABLE_TEXTURE_IO "Enable texture I/O support" OFF)

# Options to enable extensions
# Option to enable the STL (stereolithography) file I/O extension
option(IGESIO_ENABLE_STL_EXTENSION "Enable STL file I/O extension" OFF)
# Option to enable the OBJ (Wavefront OBJ) file I/O extension
option(IGESIO_ENABLE_OBJ_EXTENSION "Enable OBJ file I/O extension" OFF)
# Option to enable the inspection extension (coordinate frame groups etc.)
option(IGESIO_ENABLE_INSPECTION_EXTENSION "Enable inspection extension" OFF)
# Option to enable all extensions at once
option(IGESIO_ENABLE_ALL_EXTENSIONS "Enable all extensions" OFF)
if(IGESIO_ENABLE_ALL_EXTENSIONS)
  set(IGESIO_ENABLE_STL_EXTENSION ON)
  set(IGESIO_ENABLE_OBJ_EXTENSION ON)
  set(IGESIO_ENABLE_INSPECTION_EXTENSION ON)
  message(STATUS "All IGESio extensions enabled")
endif()
if(IGESIO_ENABLE_STL_EXTENSION OR IGESIO_ENABLE_OBJ_EXTENSION
    OR IGESIO_ENABLE_INSPECTION_EXTENSION)
  set(IGESIO_EXTENSIONS_ENABLED ON)
else()
  set(IGESIO_EXTENSIONS_ENABLED OFF)
endif()

# Define macros for enabled extensions
if(IGESIO_ENABLE_OBJ_EXTENSION)
  add_compile_definitions(IGESIO_OBJ_EXTENSION_ENABLED)
endif()
if(IGESIO_ENABLE_STL_EXTENSION)
  add_compile_definitions(IGESIO_STL_EXTENSION_ENABLED)
endif()
if(IGESIO_ENABLE_INSPECTION_EXTENSION)
  add_compile_definitions(IGESIO_INSPECTION_EXTENSION_ENABLED)
endif()



# Option to build GUI (with GLFW and ImGui)
option(IGESIO_BUILD_GUI "Build GUI (with GLFW and ImGui)" OFF)
if(IGESIO_BUILD_GUI)
  # Enable OpenGL support as well
  set(IGESIO_ENABLE_GRAPHICS ON)
  message(STATUS "GUI support enabled, enabling OpenGL (glad) support as well")

  # Enable texture I/O support as well
  set(IGESIO_ENABLE_TEXTURE_IO ON)
  message(STATUS "GUI support enabled, enabling texture I/O support as well")
endif()

# Build the pre-generated glad loader if OpenGL support is enabled
if(IGESIO_ENABLE_GRAPHICS)
  # gladはC実装のため、graphics有効時のみC言語を有効化する
  enable_language(C)

  # Find OpenGL
  find_package(OpenGL REQUIRED)

  # Add glad library
  # 生成済みのgladローダ (third_party/glad_gl_core_43) を直接ビルドする. ビルド時生成を
  # 行わないため、Python+jinja2の環境やgladリポジトリのフェッチは不要.
  # 構成を変更する場合の再生成手順はthird_party/glad_gl_core_43/generation_args.txtを参照.
  # MX (multi-context): 関数ポインタはグローバルでなくGladGLContext構造体に持つ.
  # これによりグローバルなglad_glXxxシンボルが消え、利用側との衝突を避けつつ、
  # コンテキスト固有のロード (gladLoadGLContext) をライブラリ内部に封じ込められる.
  # gl:core=4.3: graphicsモジュールはテッセレーション (GL 4.0) とSSBO (GL 4.3)
  # に依存するため、要求バージョンと同じ4.3 coreで生成したローダを使用する.
  # ターゲット名にigesio_接頭辞を付ける: add_subdirectory/FetchContentで本ライブラリを
  # 取り込む利用側が自前の同名gladターゲットを持つ場合の、CMakeターゲット名衝突を避けるため.
  add_library(igesio_glad_gl_core_43 STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad_gl_core_43/src/gl.c"
  )
  # Use generator expressions for the include directories,
  # making the target relocatable for both build and install.
  target_include_directories(igesio_glad_gl_core_43 PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad_gl_core_43/include>"
    "$<INSTALL_INTERFACE:include>"
  )
  # glad付属CMakeのリンク指定と同等 (Linuxでのdlopen用; MSVC等では空)
  target_link_libraries(igesio_glad_gl_core_43 PUBLIC ${CMAKE_DL_LIBS})
  message(STATUS "OpenGL (glad) support enabled")
else()
  message(STATUS "OpenGL (glad) support is disabled")
endif()

# Define CMAKE_INSTALL_* variables before processing the subdirectories:
# the modules' $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> and the stb
# install destination expand the variable at configure time, so it must be
# defined here (otherwise it is empty on the first configure).
include(GNUInstallDirs)

# Add subdirectory for source files
add_subdirectory(src)



# Option to build tests
option(IGESIO_BUILD_TESTING "Build the tests" OFF)

# Add the tests subdirectory if IGESIO_BUILD_TESTING is ON
if(IGESIO_BUILD_TESTING)
  message(STATUS "Building tests...")
  include(FetchContent)

  # Unify Google Test Runtime Library (for WIN32)
  if (WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  endif()

  # Fetch Google Test
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
    ${DOWNLOAD_EXTRACT_TIMESTAMP_OPTION}
  )
  FetchContent_MakeAvailable(googletest)

  # Set Google Test include directories as SYSTEM to suppress warnings from third-party code
  get_target_property(GTEST_INC gtest INTERFACE_INCLUDE_DIRECTORIES)
  if(GTEST_INC)
    set_target_properties(gtest PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${GTEST_INC}"
    )
  endif()
  get_target_property(GTEST_MAIN_INC gtest_main INTERFACE_INCLUDE_DIRECTORIES)
  if(GTEST_MAIN_INC)
    set_target_properties(gtest_main PROPERTIES
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${GTEST_MAIN_INC}"
    )
  endif()
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(gtest PRIVATE -Wno-implicit-int-float-conversion)
    target_compile_options(gtest_main PRIVATE -Wno-implicit-int-float-conversion)
  endif()

  # Google Test configuration
  enable_testing()

  add_subdirectory(tests)
endif()



# Option to build examples
option(IGESIO_BUILD_EXAMPLE "Build the examples" OFF)

if(IGESIO_BUILD_EXAMPLE)
  message(STATUS "Building examples...")
  add_subdirectory(examples)
endif()



# Documentation generation
option(IGESIO_BUILD_DOCS "Build the documentation" OFF)
if (IGESIO_BUILD_DOCS)
  # Find Doxygen
  find_package(Doxygen)

  # Check if Doxygen is found
  if (DOXYGEN_FOUND)
    # Set the input and output directories for Doxygen
    set(DOXYGEN_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")

    # Configure Doxygen
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
      "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      @ONLY
    )

    # Add a custom target to generate documentation
    add_custom_target(doc
      COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
    )
    message(STATUS "Doxygen documentation will be generated in ${DOXYGEN_OUTPUT_DIR}")
  endif()
endif()


# Install targets and configuration files
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(CMakePackageConfigHelpers)

    # Eigen is exposed through IGESio's public headers (matrix.h does
    # #include <Eigen/Core>), so it is an unavoidable PUBLIC dependency of the
    # installed package. A FetchContent-built Eigen is NOT installed alongside
    # IGESio, so a consumer of this install tree must provide their own
    # discoverable Eigen3. Warn so this is not discovered silently.
    if(IGESIO_ENABLE_EIGEN AND IGESIO_EIGEN_PROVIDER STREQUAL "fetched")
        message(WARNING
            "Installing IGESio, but Eigen came from FetchContent and will not be "
            "installed with it. The installed config calls find_dependency(Eigen3) "
            "and the public headers include <Eigen/Core>, so consumers must have "
            "their own Eigen3. Install a system Eigen3 and reconfigure to silence.")
    endif()

    install(TARGETS igesio igesio_common igesio_utils igesio_numerics igesio_entities igesio_models
        EXPORT IGESioTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Install the graphics library if IGESIO_ENABLE_GRAPHICS is ON
    if(IGESIO_ENABLE_GRAPHICS)
        install(TARGETS igesio_graphics
            EXPORT IGESioTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
        # gladアーカイブはエクスポートを維持する (PRIVATE依存として利用側へ自動で
        # 伝播し、利用側は`IGESio::graphics`をリンクするだけでよい).
        # gladヘッダはinstallしない (公開includeにgladを一切出さないため).
        install(TARGETS igesio_glad_gl_core_43
            EXPORT IGESioTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    # Install the extensions library if any extension is enabled
    if(IGESIO_EXTENSIONS_ENABLED)
        install(TARGETS igesio_extensions
            EXPORT IGESioTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/include/igesio/common/versions.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/igesio/common
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/IGESioConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/IGESioConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/IGESio
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/IGESioConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(EXPORT IGESioTargets
        FILE IGESioTargets.cmake
        NAMESPACE IGESio::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/IGESio
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/IGESioConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/IGESioConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/IGESio
    )
endif()
