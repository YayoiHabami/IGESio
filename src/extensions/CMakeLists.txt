# Extensions module (外部フォーマット入出力等のオプション機能)
# 有効化されたフォーマットのソースのみを追加する
set(EXTENSIONS_SOURCES "")

if(IGESIO_ENABLE_STL_EXTENSION)
    list(APPEND EXTENSIONS_SOURCES
        stl/stl_io.cpp
    )
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

# Create an alias for the extensions library
add_library(IGESio::extensions ALIAS igesio_extensions)

# Set target properties for export
set_target_properties(igesio_extensions PROPERTIES
    EXPORT_NAME extensions
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
