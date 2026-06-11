set(TEST_SOURCES
    test_entity_type.cpp

    # Directory Entry Section
    de/test_raw_entity_de.cpp
    de/test_raw_entity_de_is_valid.cpp
    de/test_de_color.cpp
    de/test_de_level.cpp
    de/test_de_line_font_pattern.cpp

    # Parameter Data Section
    test_entity_parameter_data.cpp

    # Entity Base
    test_entity_base.cpp
    test_pointer_container.cpp
    test_geometry_revision.cpp

    # Factory
    test_factory.cpp

    # Non-IGES entities
    meshes/test_mesh_entity.cpp

    # Curves
    curves/test_i_curve.cpp
    curves/test_circular_arc.cpp
    curves/test_conic_arc.cpp
    curves/test_copious_data.cpp
    curves/test_curve_on_a_parametric_surface.cpp
    curves/test_line.cpp
    curves/test_point.cpp
    curves/test_algorithms.cpp
    curves/test_nurbs_approximation_algorithms.cpp
    curves/test_composite_curve.cpp
    curves/test_composite_curve_edit.cpp
    curves/test_linear_path.cpp
    curves/test_parametric_spline_curve.cpp
    curves/test_rational_b_spline_curve.cpp
    curves/test_rational_b_spline_curve_edit.cpp
    curves/test_signed_curvature.cpp
    curves/test_curve_line_intersection.cpp
    curves/algorithms/test_extremal_polygon.cpp
    curves/algorithms/test_polygonal_approximation.cpp

    # Structures
    structures/test_color_definition.cpp

    # Transformations
    transformations/test_transformation_matrix.cpp

    # Surfaces
    surfaces/test_i_surface.cpp
    surfaces/test_plane.cpp
    surfaces/test_rational_b_spline_surface.cpp
    surfaces/test_rational_b_spline_surface_edit.cpp
    surfaces/test_ruled_surface.cpp
    surfaces/test_surface_of_revolution.cpp
    surfaces/test_tabulated_cylinder.cpp
    surfaces/test_trimmed_surface.cpp
    surfaces/test_trimmed_surface_edit.cpp
    surfaces/test_surface_line_intersection.cpp
    surfaces/test_curve_surface_inversion.cpp
    surfaces/test_restricted_surface_mesh.cpp
    surfaces/test_surface_boundary_edges.cpp

    # Views
    views/test_curve_view.cpp
    views/test_surface_view.cpp
)

add_executable(test_entities ${TEST_SOURCES})
target_link_libraries(test_entities PRIVATE gtest gtest_main igesio)

add_test(NAME test_entities COMMAND test_entities)
