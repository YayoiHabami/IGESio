# Build the examples
set(EXAMPLE_SOURCES
    iges_data_from_scratch.cpp
    intermediate_data_io.cpp
    iges_data_io.cpp
    sample_curves.cpp
    sample_surfaces.cpp
    special_swept_surfaces.cpp

    geometric_properties.cpp
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
    install(TARGETS ${EXAMPLE_NAME} RUNTIME DESTINATION . COMPONENT samples)
endforeach()

# Files distributed with the sample executables (`samples` component)
install(DIRECTORY data/ DESTINATION data COMPONENT samples)
install(FILES
    ${PROJECT_SOURCE_DIR}/LICENSE
    ${PROJECT_SOURCE_DIR}/THIRD_PARTY_LICENSES.md
    DESTINATION .
    COMPONENT samples
)

# Add subdirectory
add_subdirectory(gui)

message(STATUS "Examples built successfully.")
