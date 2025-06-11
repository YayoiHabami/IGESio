# utils library
add_library(igesio_utils STATIC
    iges_string_utils.cpp
    iges_binary_reader.cpp
)

# Set the source and include directories
target_include_directories(igesio_utils PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(igesio_utils PUBLIC IGESio::common)

# Create an alias for the library
add_library(IGESio::utils ALIAS igesio_utils)

# Set target properties for export
set_target_properties(igesio_utils PROPERTIES
    EXPORT_NAME utils
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
