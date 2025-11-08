# entities library
add_library(igesio_entities STATIC
    # General
    entity_type.cpp

    # Interfaces
    interfaces/i_geometry.cpp
    interfaces/i_curve.cpp
    interfaces/i_surface.cpp

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

    # Entity Base class
    entity_base.cpp
    structures/unsupported_entity.cpp

    # Entities
    structures/null_entity.cpp                 # 000
    curves/circular_arc.cpp                    # 100
    curves/composite_curve.cpp                 # 102
    curves/conic_arc.cpp                       # 104
    curves/copious_data_base.cpp               # 106
    curves/copious_data.cpp                    # 106
    curves/linear_path.cpp                     # 106

    curves/line.cpp                            # 110
    curves/parametric_spline_curve.cpp         # 112
    curves/point.cpp                           # 116
    surfaces/ruled_surface.cpp                 # 118
    surfaces/surface_of_revolution.cpp         # 120
    surfaces/tabulated_cylinder.cpp            # 122
    transformations/transformation_matrix.cpp  # 124
    curves/rational_b_spline_curve.cpp         # 126
    surfaces/rational_b_spline_surface.cpp     # 128

    structures/color_definition.cpp            # 314

    # Algorithms
    curves/algorithms.cpp

    # Factory
    factory.cpp
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
    IGESio::numerics
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
