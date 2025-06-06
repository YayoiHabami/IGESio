set(TEST_SOURCES
    test_entity_type.cpp
    test_directory_entry_param.cpp
    test_entity_parameter_data.cpp
)

add_executable(test_entities ${TEST_SOURCES})
target_link_libraries(test_entities PRIVATE gtest gtest_main igesio_entities)

add_test(NAME test_entities COMMAND test_entities)
