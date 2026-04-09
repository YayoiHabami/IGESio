# Graphics module
add_library(igesio_graphics STATIC
    core/camera.cpp
    core/texture.cpp
    core/i_entity_graphics.cpp

    curves/i_curve_graphics.cpp          # general curves
    curves/circular_arc_graphics.cpp     # 100
    curves/composite_curve_graphics.cpp  # 102
    curves/conic_arc_graphics.cpp        # 104
    curves/copious_data_graphics.cpp     # 106
    curves/line_graphics.cpp             # 110
    curves/point_graphics.cpp            # 116
    curves/rational_b_spline_curve_graphics.cpp        # 126
    curves/curve_on_a_parametric_surface_graphics.cpp  # 142

    surfaces/i_surface_graphics.cpp                    # general surfaces
    surfaces/rational_b_spline_surface_graphics.cpp    # 128

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

if (IGESIO_ENABLE_TEXTURE_IO)
    # Fetch stb (stb_image.h, stb_image_write.h) for image I/O
    set(STB_LOAD_URL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h)
    set(STB_LOAD_OUTPUT_PATH "${CMAKE_BINARY_DIR}/_deps/stb/stb_image.h")
    set(STB_WRITE_URL https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h)
    set(STB_WRITE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/_deps/stb/stb_image_write.h")
    # Show download path
    if (NOT EXISTS ${STB_LOAD_OUTPUT_PATH})
        file(DOWNLOAD ${STB_LOAD_URL} ${STB_LOAD_OUTPUT_PATH} SHOW_PROGRESS)
    endif()
    if (NOT EXISTS ${STB_WRITE_OUTPUT_PATH})
        file(DOWNLOAD ${STB_WRITE_URL} ${STB_WRITE_OUTPUT_PATH} SHOW_PROGRESS)
    endif()

    # Install stb_image.h to the include directory
    install(FILES ${STB_WRITE_OUTPUT_PATH}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/stb
    )

    target_include_directories(igesio_graphics PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/stb>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/_deps/stb> # Keep this for local builds
    )

    # Define macro to enable texture loading
    target_compile_definitions(igesio_graphics PUBLIC IGESIO_ENABLE_TEXTURE_IO)
endif()

# Public dependencies
target_link_libraries(igesio_graphics
    PUBLIC
        IGESio::common
        IGESio::numerics
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
