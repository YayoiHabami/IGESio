# CMakeLists.txt for IGESio project
#
# Options:
#   - IGESIO_BUILD_DOCS: Build the documentation (default: OFF)
#   - IGESIO_BUILD_EXAMPLE: Build the examples (default: OFF)
#   - IGESIO_BUILD_GUI: Build GUI (with GLFW and ImGui) as examples (default: OFF)
#     - if IGESIO_BUILD_GUI is ON, IGESIO_ENABLE_GRAPHICS is also set to ON
#     - if IGESIO_BUILD_GUI is ON, IGESIO_ENABLE_TEXTURE_IO is also set to ON
#     - this option requires python environment and jinja2 installed
#   - IGESIO_BUILD_TESTING: Build the tests (default: OFF)
#   - IGESIO_ENABLE_COVERAGE: Enable coverage reporting (default: OFF)
#   - IGESIO_ENABLE_EIGEN: Enable Eigen support (default: ON)
#       - Eigen is currently required for the project, so this option cannot be set to OFF.
#       - This option is provided for future flexibility in case we want to make Eigen optional later on, but for now it must be ON.
#   - IGESIO_ENABLE_GRAPHICS: Enable OpenGL (glad) support (default: OFF)
#   - IGESIO_ENABLE_TEXTURE_IO: Enable texture I/O support (default: OFF)
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

project(IGESio VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable coverage
option(IGESIO_ENABLE_COVERAGE "Enable coverage reporting" OFF)

# Coverage configuration
if(IGESIO_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        add_compile_options(--coverage)
        add_link_options(--coverage)
        message(STATUS "Coverage reporting enabled")
    else()
        message(WARNING "Coverage reporting is only supported with GCC")
    endif()
endif()

# Set the build type if not specified
if(NOT CMAKE_BUILD_TYPE)
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



# Option to enable Eigen support
option(IGESIO_ENABLE_EIGEN "Enable Eigen support" ON)
# Currently, Eigen is required for the project
if (NOT IGESIO_ENABLE_EIGEN)
    message(FATAL_ERROR "Eigen support is currently required for IGESio. Please enable IGESIO_ENABLE_EIGEN option.")
else()
  # Fetch Eigen if enabled
  include(FetchContent)

  # Disable Eigen's internal testing and documentation to speed up the build process
  set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(EIGEN_BUILD_DOC     OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTING       OFF CACHE BOOL "" FORCE)

  # Fetch Eigen
  FetchContent_Declare(
    Eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/5.0.1/eigen-5.0.1.zip
    ${DOWNLOAD_EXTRACT_TIMESTAMP_OPTION}
  )
  FetchContent_MakeAvailable(Eigen)

  # Define macro for C++ code
  add_compile_definitions(IGESIO_ENABLE_EIGEN)

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
  FetchContent_Declare(
    Boost
    URL https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.gz
    ${DOWNLOAD_EXTRACT_TIMESTAMP_OPTION}
  )
  FetchContent_MakeAvailable(Boost)
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

# Fetch glad if OpenGL support is enabled
if(IGESIO_ENABLE_GRAPHICS)
  include(FetchContent)

  # Find OpenGL
  find_package(OpenGL REQUIRED)

  # Fetch glad
  FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.4
  )
  FetchContent_MakeAvailable(glad)
  set(GLAD_GENERATOR_DIR "${glad_BINARY_DIR}")

  # Add glad library
  add_subdirectory(${glad_SOURCE_DIR}/cmake ${glad_BINARY_DIR})
  glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)
  # Overwrite the INTERFACE_INCLUDE_DIRECTORIES property for glad
  # to use generator expressions, making it relocatable for both build and install.
  set_target_properties(glad_gl_core_33 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/gladsources/glad_gl_core_33/include>;$<INSTALL_INTERFACE:include>"
  )
  message(STATUS "OpenGL (glad) support enabled")
else()
  message(STATUS "OpenGL (glad) support is disabled")
endif()

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
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

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
        # Install glad library and its headers
        install(TARGETS glad_gl_core_33
            EXPORT IGESioTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
        install(DIRECTORY ${glad_BINARY_DIR}/include/
            DESTINATION include
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
