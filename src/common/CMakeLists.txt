# common library source files
add_library(igesio_common STATIC
    versions.cpp
    iges_metadata.cpp
    validation_result.cpp
    id_generator.cpp
    serialization.cpp
    iges_parameter_vector.cpp
)

# Set the source and include directories
target_include_directories(igesio_common PUBLIC
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${IGESIO_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 公開ヘッダがC++17を要求するため、コンパイル要求として利用側へ伝播させる
# (CMAKE_CXX_STANDARDはIGESio自身のビルドにしか効かない)
target_compile_features(igesio_common PUBLIC cxx_std_17)

# Link Threads (std::async/std::thread backend for common/parallel.h).
# PUBLIC so every library that depends on IGESio::common transitively links it.
target_link_libraries(igesio_common PUBLIC Threads::Threads)

# Handle Eigen dependency if enabled
if(IGESIO_ENABLE_EIGEN)
    # BUILD_INTERFACEのみとする: FetchContent由来のEigenはALIAS先の実ターゲット
    # eigenがエクスポートセットに含まれないため、install(EXPORT)に乗せられない。
    # インストール先の利用側へは、IGESioConfig.cmakeがfind_dependency(Eigen3)の
    # 後にIGESio::commonへEigen3::Eigenを追記することで伝播させる。
    target_link_libraries(igesio_common PUBLIC
        $<BUILD_INTERFACE:Eigen3::Eigen>
    )
    target_compile_definitions(igesio_common PUBLIC IGESIO_ENABLE_EIGEN)
    target_include_directories(igesio_common PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endif()

# Create an alias for the common library
add_library(IGESio::common ALIAS igesio_common)

# Set target properties for export
set_target_properties(igesio_common PROPERTIES
    EXPORT_NAME common
    VERSION ${PROJECT_VERSION}
    SOVERSION ${VERSION_MAJOR}
)
