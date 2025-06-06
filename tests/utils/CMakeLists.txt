set(TEST_SOURCES
    # Testing for iges_string_utils.h
    test_iges_string_utils_basic.cpp
    test_iges_string_utils_line_assertion.cpp
    test_iges_string_utils_global_section.cpp
    test_iges_string_utils_serialization.cpp

    # Testing for iges_binary_reader.h
    test_iges_binary_reader.cpp
)

add_executable(test_utils ${TEST_SOURCES})
target_link_libraries(test_utils PRIVATE gtest gtest_main igesio_utils)

add_test(NAME test_utils COMMAND test_utils)
