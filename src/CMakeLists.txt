# サブディレクトリの追加
add_subdirectory(common)
add_subdirectory(utils)
add_subdirectory(entities)
add_subdirectory(models)

# Create the igesio library
add_library(igesio STATIC
    reader.cpp
    writer.cpp
)

# Specify the source and include directories
target_include_directories(igesio PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(igesio PUBLIC
    IGESio::common
    IGESio::utils
    IGESio::entities
    IGESio::models
)

# Create an alias for the igesio library
add_library(IGESio::IGESio ALIAS igesio)

# Set target properties for export
set_target_properties(igesio PROPERTIES
    EXPORT_NAME IGESio
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
