# Extensions module (外部フォーマット入出力等のオプション機能)
# 有効化されたフォーマットのソースのみを追加する
set(EXTENSIONS_SOURCES "")

if(IGESIO_ENABLE_STL_EXTENSION)
    list(APPEND EXTENSIONS_SOURCES
        stl/stl_io.cpp
    )
endif()

if(IGESIO_ENABLE_OBJ_EXTENSION)
    list(APPEND EXTENSIONS_SOURCES
        obj/obj_io.cpp
    )
endif()

if(IGESIO_ENABLE_INSPECTION_EXTENSION)
    # エンティティと橋渡し関数はgraphics非依存 (常に追加)
    list(APPEND EXTENSIONS_SOURCES
        inspection/coordinate_frame_group.cpp
        inspection/instanced_entity.cpp
    )
    # 描画クラス・カスタムシェーダー登録はgraphics有効時のみ
    if(IGESIO_ENABLE_GRAPHICS)
        list(APPEND EXTENSIONS_SOURCES
            inspection/coordinate_frame_group_graphics.cpp
            inspection/instanced_entity_graphics.cpp
        )
    endif()
endif()

if(IGESIO_ENABLE_ANIMATION_EXTENSION)
    # 姿勢適用はAssembly::SetGlobalTransform経由のためGL非依存 (常に追加)
    list(APPEND EXTENSIONS_SOURCES
        animation/animation_clip.cpp
        animation/animation_player.cpp
    )
endif()

if(IGESIO_ENABLE_MACHINES_EXTENSION)
    # TOML入出力・運動学はGL非依存 (常に追加). サブフォルダ単位で列挙する
    list(APPEND EXTENSIONS_SOURCES
        machines/core/diagnostics.cpp
        machines/core/units.cpp
        machines/core/formatting.cpp
        machines/core/rotation.cpp
        machines/machine/machine_definition.cpp
        machines/machine/toml_reading.cpp
        machines/machine/machine_writing.cpp
        machines/machine/machine_io.cpp
    )
    if(MINGW)
        # toml11のテンプレート展開とデバッグ情報でCOFFのセクション数上限 (2^16) を
        # 超えるため、toml11に依存するソースのみ大きなオブジェクト形式を許可する
        set_source_files_properties(
            machines/machine/toml_reading.cpp
            machines/machine/machine_writing.cpp
            machines/machine/machine_io.cpp
            PROPERTIES COMPILE_OPTIONS "-Wa,-mbig-obj"
        )
    endif()
endif()

add_library(igesio_extensions STATIC ${EXTENSIONS_SOURCES})

# Set the source and include directories
target_include_directories(igesio_extensions PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Public dependencies
# 拡張は全モジュールに依存する (将来のSTEP拡張がentities/modelsを要するため)
target_link_libraries(igesio_extensions
    PUBLIC
        IGESio::common
        IGESio::utils
        IGESio::numerics
        IGESio::entities
        IGESio::models
)
if(IGESIO_ENABLE_GRAPHICS)
    # graphics有効時のみ、拡張固有の描画関連ソースをリンク可能にする
    target_link_libraries(igesio_extensions PUBLIC IGESio::graphics)
endif()
if(IGESIO_ENABLE_MACHINES_EXTENSION)
    # toml11はヘッダオンリーで、.cpp内でのみ使用している. BUILD_INTERFACEで囲い、
    # install(EXPORT)がtoml11ターゲットを要求しないようにする (Boost::mathと同型)
    target_link_libraries(igesio_extensions PRIVATE $<BUILD_INTERFACE:toml11::toml11>)
endif()

# Create an alias for the extensions library
add_library(IGESio::extensions ALIAS igesio_extensions)

# Set target properties for export
set_target_properties(igesio_extensions PROPERTIES
    EXPORT_NAME extensions
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
