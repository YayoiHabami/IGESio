# Graphics module
add_library(igesio_graphics STATIC
    core/camera.cpp
    core/i_entity_graphics.cpp
    curves/i_curve_graphics.cpp

    curves/circular_arc_graphics.cpp     # 100
    curves/composite_curve_graphics.cpp  # 102
    curves/conic_arc_graphics.cpp        # 104
    curves/copious_data_graphics.cpp     # 106
    curves/line_graphics.cpp             # 110
    curves/rational_b_spline_curve_graphics.cpp      # 126
    surfaces/rational_b_spline_surface_graphics.cpp  # 128

    factory.cpp

    renderer.cpp
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
