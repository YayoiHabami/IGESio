# Build the examples with GUI support
set(GUI_EXAMPLE_SOURCES
    curves_viewer.cpp
)



# Fetch GLFW and ImGui if GUI support is enabled
if(IGESIO_BUILD_GUI)
    include(FetchContent)

    # Fetch GLFW
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    # Fetch ImGui
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.1
    )

    FetchContent_MakeAvailable(glfw imgui)

    # ImGui source files
    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    message(STATUS "GUI support enabled with GLFW and ImGui")
else()
    # If IGESIO_BUILD_GUI is not ON, skip the GUI-related code
    return()
endif()

# Build each example as an executable
foreach(EXAMPLE_SOURCE ${GUI_EXAMPLE_SOURCES})
    message(STATUS "Processing example: ${EXAMPLE_SOURCE}")
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
    add_executable(${EXAMPLE_NAME}
        ${EXAMPLE_SOURCE}
        iges_viewer_gui.cpp   # Include the GUI viewer source
        ${IMGUI_SOURCES}      # Include ImGui sources
    )

    target_link_options(${EXAMPLE_NAME} PRIVATE
        "-mconsole"
    )

    target_include_directories(${EXAMPLE_NAME} PRIVATE
        ${INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(${EXAMPLE_NAME} PRIVATE
        igesio
        glfw
        glad_gl_core_33
        OpenGL::GL
    )
endforeach()
