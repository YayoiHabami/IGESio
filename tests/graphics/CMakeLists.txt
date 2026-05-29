set(TEST_SOURCES
    test_renderer_smoke.cpp
    test_selection_pull.cpp
    test_composite_render.cpp
    test_scene_walk.cpp
)

add_executable(test_graphics ${TEST_SOURCES})
# graphicsはIGESIO_ENABLE_GRAPHICS時にigesioへPUBLICリンクされるため、igesioで一式引ける
# (glad等のインクルードパスも推移的に伝播する)
target_link_libraries(test_graphics PRIVATE gtest gtest_main igesio)

# mock_open_gl.h を同ディレクトリから引く
target_include_directories(test_graphics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME test_graphics COMMAND test_graphics)
