set (TEST_SOURCES
    test_constexpr_math.cpp
    test_integration.cpp
)

add_executable(test_numerics ${TEST_SOURCES})
target_link_libraries(test_numerics PRIVATE gtest gtest_main igesio_numerics)

add_test(NAME test_numerics COMMAND test_numerics)
