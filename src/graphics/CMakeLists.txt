# Graphics module
add_library(igesio_graphics STATIC
    core/i_entity_graphics.cpp
)

# Set the source and include directories
target_include_directories(igesio_graphics PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Public dependencies
target_link_libraries(igesio_graphics
    PUBLIC
        IGESio::common
        IGESio::utils
        IGESio::entities
        IGESio::models
        glad_gl_core_33
)

# Create an alias for the graphics library
add_library(IGESio::graphics ALIAS igesio_graphics)

# Set target properties for export
set_target_properties(igesio_graphics PROPERTIES
    EXPORT_NAME graphics
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
