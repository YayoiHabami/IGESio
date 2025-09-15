# models library
add_library(igesio_models STATIC
    global_param.cpp

    iges_data.cpp
)

# Set the source and include directories
target_include_directories(igesio_models PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(igesio_models PUBLIC
    IGESio::common
    IGESio::utils
    IGESio::entities
)

# Create an alias for the models library
add_library(IGESio::models ALIAS igesio_models)

# Set target properties for export
set_target_properties(igesio_models PROPERTIES
    EXPORT_NAME models
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
