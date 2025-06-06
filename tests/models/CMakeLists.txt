set(TEST_SOURCES
    test_global_param.cpp
)

add_executable(test_models ${TEST_SOURCES})
target_link_libraries(test_models PRIVATE gtest gtest_main igesio_models)

add_test(NAME test_models COMMAND test_models)
