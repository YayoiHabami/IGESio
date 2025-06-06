# utils library
add_library(igesio_utils STATIC
    iges_string_utils.cpp
    iges_binary_reader.cpp
)

# Set the source and include directories
target_include_directories(igesio_utils PUBLIC
    ${SRC_DIR}
    ${INCLUDE_DIR}
)

target_link_libraries(igesio_utils PUBLIC IGESio::common)

# Create an alias for the library
add_library(IGESio::utils ALIAS igesio_utils)
