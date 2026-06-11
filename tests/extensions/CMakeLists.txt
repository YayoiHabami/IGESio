set(TEST_SOURCES "")

if(IGESIO_ENABLE_STL_EXTENSION)
    list(APPEND TEST_SOURCES
        test_stl_io.cpp
    )
endif()

add_executable(test_extensions ${TEST_SOURCES})
target_link_libraries(test_extensions PRIVATE gtest gtest_main igesio)

add_test(NAME test_extensions COMMAND test_extensions)
