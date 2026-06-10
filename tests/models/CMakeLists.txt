set(TEST_SOURCES
    test_global_param.cpp

    test_assembly.cpp
    test_assembly_coords.cpp
    test_initial_tree.cpp
    test_flatten.cpp
    test_non_iges_entity.cpp

    test_selection_set.cpp
    test_scene.cpp
)

add_executable(test_models ${TEST_SOURCES})
# AssemblyテストはReadIges(reader)やviews等を引くためフルigesioへリンクする
target_link_libraries(test_models PRIVATE gtest gtest_main igesio)

add_test(NAME test_models COMMAND test_models)
