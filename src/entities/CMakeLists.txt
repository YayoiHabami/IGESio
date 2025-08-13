# entities library
add_library(igesio_entities STATIC
    # General
    entity_type.cpp

    # Directory Entry section related
    de/de_structure.cpp
    de/de_line_font_pattern.cpp
    de/de_level.cpp
    de/de_view.cpp
    de/de_transformation_matrix.cpp
    de/de_label_display_associativity.cpp
    de/de_color.cpp
    de/raw_entity_de.cpp

    # Parameter Data section related
    pd.cpp
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
