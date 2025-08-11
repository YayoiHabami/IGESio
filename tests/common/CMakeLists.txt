set(TEST_SOURCES
    test_versions.cpp
    test_matrix.cpp
    test_id_generator.cpp
    test_iges_metadata.cpp
    test_serialization_FromIgesXxx.cpp
    test_serialization_ToIgesXxx.cpp
    test_iges_parameter_vector.cpp
    test_tolerance.cpp
    test_validation_result.cpp
)

add_executable(test_common ${TEST_SOURCES})
target_link_libraries(test_common PRIVATE gtest gtest_main igesio_common)

add_test(NAME test_common COMMAND test_common)