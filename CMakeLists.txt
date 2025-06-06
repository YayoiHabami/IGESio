cmake_minimum_required(VERSION 3.14)

# Set CMP0135 policy to control timestamp handling for FetchContent archive extraction
# when CMake version 3.24 or later is available
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
    set(DOWNLOAD_EXTRACT_TIMESTAMP_OPTION DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
else()
    set(DOWNLOAD_EXTRACT_TIMESTAMP_OPTION "")
endif()

project(IGESio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# Generate version header file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/igesio/common/versions.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/igesio/common/versions.h"
    @ONLY
)

# Variables for source and include directories
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Set include directories
include_directories(${INCLUDE_DIR})

# Option to enable Eigen support
option(IGESIO_ENABLE_EIGEN "Enable Eigen support" OFF)

# Fetch Eigen if enabled
if(IGESIO_ENABLE_EIGEN)
  include(FetchContent)

  # Fetch Eigen
  FetchContent_Declare(
    Eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip
    ${DOWNLOAD_EXTRACT_TIMESTAMP_OPTION}
  )
  FetchContent_MakeAvailable(Eigen)

  # Define macro for C++ code
  add_compile_definitions(IGESIO_ENABLE_EIGEN)

  message(STATUS "Eigen support enabled")
endif()

# Add subdirectory for source files
add_subdirectory(src)



# Option to build tests
option(IGESIO_BUILD_TESTING "Build the tests" OFF)

# Add the tests subdirectory if IGESIO_BUILD_TESTING is ON
if(IGESIO_BUILD_TESTING)
  include(FetchContent)

  # Fetch Google Test
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
    ${DOWNLOAD_EXTRACT_TIMESTAMP_OPTION}
  )
  FetchContent_MakeAvailable(googletest)

  # Google Test configuration
  enable_testing()

  add_subdirectory(tests)
endif()



# Documentation generation
option(IGESIO_BUILD_DOCS "Build the documentation" ON)
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
