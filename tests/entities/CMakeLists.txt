set(TEST_SOURCES
    test_entity_type.cpp

    # Directory Entry Section
    de/test_raw_entity_de.cpp
    de/test_raw_entity_de_is_valid.cpp
    de/test_de_color.cpp

    # Parameter Data Section
    test_entity_parameter_data.cpp

    # Entity Base
    test_entity_base.cpp

    # Curves
    curves/test_circular_arc.cpp
)

add_executable(test_entities ${TEST_SOURCES})
target_link_libraries(test_entities PRIVATE gtest gtest_main igesio)

add_test(NAME test_entities COMMAND test_entities)
