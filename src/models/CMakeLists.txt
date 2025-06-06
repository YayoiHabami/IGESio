# models library
add_library(igesio_models STATIC
    global_param.cpp
)

# Set the source and include directories
target_include_directories(igesio_models PUBLIC
    ${SRC_DIR}
    ${INCLUDE_DIR}
)

target_link_libraries(igesio_models PUBLIC
    IGESio::common
    IGESio::utils
    IGESio::entities
)

# Create an alias for the models library
add_library(IGESio::models ALIAS igesio_models)
