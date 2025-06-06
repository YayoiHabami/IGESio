# entities library
add_library(igesio_entities STATIC
    entity_type.cpp
    directory_entry_param.cpp
    entity_parameter_data.cpp
)

# Set the source and include directories
target_include_directories(igesio_entities PUBLIC
    ${SRC_DIR}
    ${INCLUDE_DIR}
)

target_link_libraries(igesio_entities PUBLIC
    IGESio::common
    IGESio::utils
)

# Create an alias for the library
add_library(IGESio::entities ALIAS igesio_entities)
