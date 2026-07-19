# ---------------------------------------------------------------
# シェーダーソースのビルド時埋め込み
# ---------------------------------------------------------------
# 埋め込むGLSLソースを shaders/ からの相対パス (glsl/...) で列挙する。
# ここに列挙した相対パス文字列がそのまま実行時の参照キーとなる
# (例: ShaderCode("glsl/curves/general_curve.vert")、
#      GLSL中の #include "glsl/common/nurbs_surface_prop.glsl")。
# パス参照される全ステージファイルと共通スニペットを漏れなく列挙すること。
set(GRAPHICS_SHADER_SOURCES
    glsl/common/nurbs_curve_prop.glsl
    glsl/common/nurbs_surface_prop.glsl
    glsl/curves/100_circular_arc.vert
    glsl/curves/100_circular_arc.tesc
    glsl/curves/100_circular_arc.tese
    glsl/curves/100_circular_arc.frag
    glsl/curves/104_form0_ellipse.vert
    glsl/curves/104_form0_ellipse.frag
    glsl/curves/116_point.vert
    glsl/curves/116_point.frag
    glsl/curves/126_nurbs_curve.vert
    glsl/curves/126_nurbs_curve.tesc
    glsl/curves/126_nurbs_curve.tese
    glsl/curves/126_nurbs_curve.frag
    glsl/curves/general_curve.vert
    glsl/curves/general_curve.frag
    glsl/curves/wide_line.geom
    glsl/surfaces/general_surface.vert
    glsl/surfaces/general_surface.frag
    glsl/surfaces/128_nurbs_surface.vert
    glsl/surfaces/128_nurbs_surface.tesc
    glsl/surfaces/128_nurbs_surface.tese
    glsl/surfaces/128_nurbs_surface.frag
)

# シェーダーの基準ディレクトリ、生成先.cpp、依存関係指定用の絶対パス群を求める
set(_graphics_shader_root ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(_graphics_shader_generated
    ${CMAKE_CURRENT_BINARY_DIR}/generated/shader_sources_generated.cpp)
set(_graphics_shader_abs_paths "")
foreach(_shader IN LISTS GRAPHICS_SHADER_SOURCES)
    list(APPEND _graphics_shader_abs_paths ${_graphics_shader_root}/${_shader})
endforeach()

# 上記シェーダーを文字列定数として埋め込む.cppを生成する
# (いずれかのシェーダー、または生成スクリプトが変化した場合に再生成する)
add_custom_command(
    OUTPUT ${_graphics_shader_generated}
    COMMAND ${CMAKE_COMMAND}
        -DSHADER_ROOT=${_graphics_shader_root}
        -DOUTPUT_FILE=${_graphics_shader_generated}
        "-DSHADER_KEYS=${GRAPHICS_SHADER_SOURCES}"
        -P ${PROJECT_SOURCE_DIR}/cmake/embed_shaders.cmake
    DEPENDS
        ${_graphics_shader_abs_paths}
        ${PROJECT_SOURCE_DIR}/cmake/embed_shaders.cmake
    COMMENT "Embedding graphics shader sources into shader_sources_generated.cpp"
    VERBATIM
)
set_source_files_properties(${_graphics_shader_generated}
    PROPERTIES GENERATED TRUE)

# Graphics module
add_library(igesio_graphics STATIC
    ${_graphics_shader_generated}

    core/camera.cpp
    core/texture.cpp
    core/i_entity_graphics.cpp
    core/open_gl.cpp
    core/pick_registry.cpp
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

    meshes/triangle_mesh_graphics.cpp                  # MeshEntity (non-IGES)

    factory.cpp
    graphics_registry.cpp
    shader_registry.cpp

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
        igesio_glad_gl_core_43
)

# Create an alias for the graphics library
add_library(IGESio::graphics ALIAS igesio_graphics)

# Set target properties for export
set_target_properties(igesio_graphics PROPERTIES
    EXPORT_NAME graphics
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
