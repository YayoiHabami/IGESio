# common library source files
file(GLOB_RECURSE COMMON_SOURCES "*.cpp")

add_library(igesio_common STATIC ${COMMON_SOURCES})

# Set the source and include directories
target_include_directories(igesio_common PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Handle Eigen dependency if enabled
if(IGESIO_ENABLE_EIGEN)
    target_link_libraries(igesio_common PUBLIC Eigen3::Eigen)
    target_compile_definitions(igesio_common PUBLIC IGESIO_ENABLE_EIGEN)
endif()

# Create an alias for the common library
add_library(IGESio::common ALIAS igesio_common)

# Set target properties for export
set_target_properties(igesio_common PROPERTIES
    EXPORT_NAME common
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
