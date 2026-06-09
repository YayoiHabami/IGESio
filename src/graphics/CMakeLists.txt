# Graphics module
add_library(igesio_graphics STATIC
    core/camera.cpp
    core/texture.cpp
    core/i_entity_graphics.cpp
    core/open_gl.cpp
    core/ray.cpp
    core/surface_edge_buffer.cpp

    curves/i_curve_graphics.cpp          # general curves
    curves/circular_arc_graphics.cpp     # 100
    curves/composite_curve_graphics.cpp  # 102
    curves/conic_arc_graphics.cpp        # 104
    curves/copious_data_graphics.cpp     # 106
    curves/line_graphics.cpp             # 110
    curves/point_graphics.cpp            # 116
    curves/rational_b_spline_curve_graphics.cpp        # 126
    curves/curve_on_a_parametric_surface_graphics.cpp  # 142

    surfaces/surface_mesh.cpp                          # general surface mesh
    surfaces/i_surface_graphics.cpp                    # general surfaces
    surfaces/rational_b_spline_surface_graphics.cpp    # 128
    surfaces/restricted_surface_graphics.cpp           # 143/144/108(bounded)

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
    # Fetch stb (stb_image.h, stb_image_write.h) for image I/O.
    # 再現性のため、masterではなくコミットを固定して取得する。
    set(STB_COMMIT 31c1ad37456438565541f4919958214b6e762fb4)
    set(STB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/_deps/stb")
    foreach(STB_HEADER stb_image.h stb_image_write.h)
        set(STB_OUTPUT_PATH "${STB_OUTPUT_DIR}/${STB_HEADER}")
        if (NOT EXISTS "${STB_OUTPUT_PATH}")
            file(DOWNLOAD
                "https://raw.githubusercontent.com/nothings/stb/${STB_COMMIT}/${STB_HEADER}"
                "${STB_OUTPUT_PATH}"
                STATUS STB_DOWNLOAD_STATUS
            )
            # 失敗時は空ファイルが残り、次回configure時のEXISTSチェックを
            # すり抜けてしまうため、削除してエラーにする
            list(GET STB_DOWNLOAD_STATUS 0 STB_DOWNLOAD_CODE)
            if (NOT STB_DOWNLOAD_CODE EQUAL 0)
                file(REMOVE "${STB_OUTPUT_PATH}")
                list(GET STB_DOWNLOAD_STATUS 1 STB_DOWNLOAD_ERROR)
                message(FATAL_ERROR
                    "Failed to download ${STB_HEADER}: ${STB_DOWNLOAD_ERROR}")
            endif()
        endif()
    endforeach()

    # Install stb headers to the include directory (top-level builds only;
    # FetchContent等で取り込まれた場合に利用側のinstallへ混入させない)
    if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
        install(FILES
            "${STB_OUTPUT_DIR}/stb_image.h"
            "${STB_OUTPUT_DIR}/stb_image_write.h"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/stb
        )
    endif()

    target_include_directories(igesio_graphics PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/stb>
        $<BUILD_INTERFACE:${STB_OUTPUT_DIR}> # Keep this for local builds
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
    PRIVATE
        # gladは公開ヘッダに出さない (core/open_gl.cppのみが内部で使用).
        # 静的ライブラリのため、リンク用アーカイブは利用側へ自動で伝播する.
        glad_gl_core_43
)

# Create an alias for the graphics library
add_library(IGESio::graphics ALIAS igesio_graphics)

# Set target properties for export
set_target_properties(igesio_graphics PROPERTIES
    EXPORT_NAME graphics
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
