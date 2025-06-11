# entities library
add_library(igesio_entities STATIC
    entity_type.cpp
    directory_entry_param.cpp
    entity_parameter_data.cpp
)

# Set the source and include directories
target_include_directories(igesio_entities PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(igesio_entities PUBLIC
    IGESio::common
    IGESio::utils
)

# Create an alias for the library
add_library(IGESio::entities ALIAS igesio_entities)

# Set target properties for export
set_target_properties(igesio_entities PROPERTIES
    EXPORT_NAME entities
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
