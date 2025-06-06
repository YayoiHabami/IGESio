# サブディレクトリの追加
add_subdirectory(common)
add_subdirectory(utils)
add_subdirectory(entities)
add_subdirectory(models)



# Create the igesio library
add_library(igesio STATIC
    reader.cpp
    writer.cpp
)

# Specify the source and include directories
target_include_directories(igesio PUBLIC
    ${SRC_DIR}
    ${INCLUDE_DIR}
)


target_link_libraries(igesio PUBLIC
    IGESio::common
    IGESio::utils
    IGESio::entities
    IGESio::models
)

# Create an alias for the igesio library
add_library(IGESio ALIAS igesio)
