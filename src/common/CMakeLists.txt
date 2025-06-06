# common library for IGESio
add_library(igesio_common STATIC
    versions.cpp
    id_generator.cpp
    iges_metadata.cpp
    serialization.cpp
    iges_parameter_vector.cpp
)

# Set the source and include directories
target_include_directories(igesio_common PUBLIC
    ${SRC_DIR}
    ${INCLUDE_DIR}

    # Generated include directory for version header
    ${CMAKE_BINARY_DIR}/include
)

if (IGESIO_ENABLE_EIGEN)
    # Add Eigen include directories if enabled
    target_link_libraries(igesio_common PUBLIC Eigen3::Eigen)
endif()

# Create an alias for the common library
add_library(IGESio::common ALIAS igesio_common)
