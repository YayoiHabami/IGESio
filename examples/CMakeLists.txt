# Build the examples
set(EXAMPLE_SOURCES
    iges_data_from_scratch.cpp
    intermediate_data_io.cpp
    iges_data_io.cpp
    sample_curves.cpp
    sample_surfaces.cpp
)



# Build each example as an executable
foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SOURCE})
    target_include_directories(${EXAMPLE_NAME} PRIVATE
            ${INCLUDE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(${EXAMPLE_NAME} igesio)
endforeach()

# Add subdirectory
add_subdirectory(gui)

message(STATUS "Examples built successfully.")
