add_subdirectory(common)
add_subdirectory(utils)
add_subdirectory(numerics)
add_subdirectory(entities)
add_subdirectory(models)



set(TEST_SOURCES
    test_reader.cpp
    test_writer.cpp
)

add_executable(test_igesio ${TEST_SOURCES})
target_link_libraries(test_igesio PRIVATE
    gtest
    gtest_main
    igesio
)

# Register the test with CTest
add_test(NAME test_igesio COMMAND test_igesio)
