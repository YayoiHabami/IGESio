add_subdirectory(common)
add_subdirectory(utils)
add_subdirectory(numerics)
add_subdirectory(entities)
add_subdirectory(models)

# graphicsテストはOpenGL有効時のみ (モックでGL文脈なしに動作する)
if(IGESIO_ENABLE_GRAPHICS)
    add_subdirectory(graphics)
endif()

# extensionsテストは対応する拡張が有効な場合のみ
if(IGESIO_ENABLE_STL_EXTENSION)
    add_subdirectory(extensions)
endif()



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
