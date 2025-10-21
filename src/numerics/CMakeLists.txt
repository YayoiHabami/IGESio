# Numerics module
add_library(igesio_numerics STATIC
    integration.cpp
)

# Set the source and include directories
target_include_directories(igesio_numerics PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(igesio_numerics PUBLIC
    IGESio::common
)

# Create an alias for the library
add_library(IGESio::numerics ALIAS igesio_numerics)

# Set target properties for export
set_target_properties(igesio_numerics PROPERTIES
    EXPORT_NAME numerics
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
