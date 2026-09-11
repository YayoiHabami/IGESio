set(TEST_SOURCES "")

if(IGESIO_ENABLE_STL_EXTENSION)
    list(APPEND TEST_SOURCES
        test_stl_io.cpp
    )
endif()

if(IGESIO_ENABLE_OBJ_EXTENSION)
    list(APPEND TEST_SOURCES
        test_obj_io.cpp
    )
endif()

if(IGESIO_ENABLE_INSPECTION_EXTENSION)
    list(APPEND TEST_SOURCES
        inspection/test_coordinate_frame_group.cpp
        inspection/test_instanced_entity.cpp
    )
endif()

if(IGESIO_ENABLE_ANIMATION_EXTENSION)
    list(APPEND TEST_SOURCES
        animation/test_animation_clip.cpp
        animation/test_animation_player.cpp
    )
endif()

if(IGESIO_ENABLE_MACHINES_EXTENSION)
    list(APPEND TEST_SOURCES
        machines/core/test_units.cpp
        machines/core/test_formatting.cpp
        machines/core/test_rotation.cpp
        machines/machine/test_machine_io.cpp
        machines/machine/test_machine_writer.cpp
        machines/machine/test_primitives.cpp
        machines/machine/test_axis_values.cpp
        machines/machine/test_machine_model.cpp
        machines/machine/test_forward_kinematics.cpp
        machines/machine/test_inverse_kinematics.cpp
        machines/tools/test_tool_profile.cpp
        machines/tools/test_tool_entities.cpp
    )
endif()

add_executable(test_extensions ${TEST_SOURCES})
target_link_libraries(test_extensions PRIVATE gtest gtest_main igesio)

add_test(NAME test_extensions COMMAND test_extensions)
