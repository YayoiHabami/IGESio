/**
 * @file numerics/meshes/triangle_mesh.h
 * @brief インデックス付き三角形メッシュ
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note メッシュ用データ構造は、IGESのエンティティや描画、ファイル入出力（STL等）
 *       とは独立して定義する. アルゴリズムは同階層のalgorithms.hで定義する.
 */
#ifndef IGESIO_NUMERICS_MESHES_TRIANGLE_MESH_H_
#define IGESIO_NUMERICS_MESHES_TRIANGLE_MESH_H_

#include <cstdint>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"



namespace igesio::numerics {

/// @brief 面グループ (三角形範囲の名前付き区分)
/// @note OBJのg/o (グループ) とusemtl (マテリアル参照) に対応する.
///       マテリアルの実体 (色・テクスチャパス等) はメッシュでは保持せず、
///       名前参照のみを持つ (実体は拡張側の別クラスが扱う)
struct MeshGroup {
    /// @brief グループ名
    std::string name;
    /// @brief マテリアル名 (参照のみ. 空=未指定)
    std::string material_name;
    /// @brief このグループの先頭三角形のインデックス
    std::uint32_t first_triangle = 0;
    /// @brief このグループの三角形数
    std::uint32_t triangle_count = 0;
};

/// @brief インデックス付き三角形メッシュ
/// @tparam Scalar スカラー型（float/double）
/// @note 以下の形式でデータを保持する.
///       (i) 三角形のみ（多角形は読み込み側で先頭頂点を共有する三角形に分割する）.
///       (i) 位置・法線・UVを共通の1つのインデックスで参照する
///          （頂点ごとに位置+法線+UVの組. OBJのように要素ごとに別のインデックスを
///            持つ形式では、読み込み側で(v, vt, vn)の組ごとに頂点をまとめて変換する）.
///       (i) SoAレイアウトであり、頂点位置・法線・UVを別々の行列で保持する.
///           法線とUVの指定は任意（未指定の場合は列数ゼロの行列とする）.
///       (i) 基本的にはScalar=doubleとし、
///           GPUステージングやSTLバイナリとの変換ではScalar=floatとする
template <typename Scalar>
struct TriangleMeshT {
    /// @brief 3×N行列の別名
    using Matrix3X = Eigen::Matrix<Scalar, 3, Eigen::Dynamic>;
    /// @brief 2×N行列の別名
    using Matrix2X = Eigen::Matrix<Scalar, 2, Eigen::Dynamic>;

    /// @brief 頂点位置（3×N; 各列が1頂点）
    Matrix3X positions;
    /// @brief 頂点法線（3×N; 法線を指定しない場合は列数ゼロ）
    Matrix3X normals;
    /// @brief テクスチャ座標（2×N; UVを指定しない場合は列数ゼロ）
    Matrix2X uvs;
    /// @brief 三角形インデックス（3要素で1つの三角形; positionsの列番号）
    std::vector<std::uint32_t> indices;
    /// @brief 面グループ（空ならグループなし）
    std::vector<MeshGroup> groups;

    /// @brief 頂点法線を持つか
    bool HasNormals() const { return normals.cols() > 0; }
    /// @brief テクスチャ座標を持つか
    bool HasUVs() const { return uvs.cols() > 0; }
    /// @brief 頂点数を取得する
    std::size_t VertexCount() const {
        return static_cast<std::size_t>(positions.cols());
    }
    /// @brief 三角形の数を取得する
    std::size_t TriangleCount() const { return indices.size() / 3; }
};

/// @brief 倍精度の三角形メッシュ（既定）
using TriangleMeshd = TriangleMeshT<double>;
/// @brief 単精度の三角形メッシュ（GPUステージング・STLバイナリとの変換用）
using TriangleMeshf = TriangleMeshT<float>;

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESHES_TRIANGLE_MESH_H_
