set(TEST_SOURCES
    # Testing for iges_string_utils.h
    test_iges_string_utils_basic.cpp
    test_iges_string_utils_line_assertion.cpp
    test_iges_string_utils_global_section.cpp
    test_iges_string_utils_serialization.cpp

    # Testing for iges_binary_reader.h
    test_iges_binary_reader.cpp

    # Testing for path_encoding.h
    test_path_encoding.cpp
)

add_executable(test_utils ${TEST_SOURCES})
target_link_libraries(test_utils PRIVATE gtest gtest_main igesio_utils)
# 全角パスのテスト用ヘルパー (tests/unicode_test_path.h) を参照する
target_include_directories(test_utils PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_test(NAME test_utils COMMAND test_utils)
