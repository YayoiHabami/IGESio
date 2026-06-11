/**
 * @file numerics/mesh/triangle_mesh.h
 * @brief インデックス付き三角形メッシュ (純粋データ)
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note IGESエンティティ・描画・ファイル入出力 (STL等) から独立した、
 *       メッシュの正準データ構造. アルゴリズムは同階層のalgorithms.hに
 *       関数として置く (関数指向).
 */
#ifndef IGESIO_NUMERICS_MESH_TRIANGLE_MESH_H_
#define IGESIO_NUMERICS_MESH_TRIANGLE_MESH_H_

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

/// @brief インデックス付き三角形メッシュ (純粋データ)
/// @tparam Scalar スカラー型 (float / double)
/// @note 設計上の選択:
///       - 三角形限定 (多角形は読み込み側でファン分割する)
///       - 単一インデックス空間 (1頂点 = 位置+法線+UVの組. OBJの多重インデックスは
///         読み込み側でコーナー重複排除により正規化する)
///       - SoAレイアウト+オプションチャンネル (法線・UVは列数0で「なし」を表す)
///       - スカラー型はテンプレート (解析・オーサリング=double、
///         GPUステージング・STLバイナリ=float)
template <typename Scalar>
struct TriangleMeshT {
    /// @brief 3×N行列の別名
    using Matrix3X = Eigen::Matrix<Scalar, 3, Eigen::Dynamic>;
    /// @brief 2×N行列の別名
    using Matrix2X = Eigen::Matrix<Scalar, 2, Eigen::Dynamic>;

    /// @brief 頂点位置 (3×N; 各列が1頂点)
    Matrix3X positions;
    /// @brief 頂点法線 (3×N or 列数0=法線なし)
    Matrix3X normals;
    /// @brief テクスチャ座標 (2×N or 列数0=UVなし)
    Matrix2X uvs;
    /// @brief 三角形インデックス (3要素で1三角形; positionsの列番号)
    std::vector<std::uint32_t> indices;
    /// @brief 面グループ (空ならグループなし)
    std::vector<MeshGroup> groups;

    /// @brief 頂点法線を持つか
    bool HasNormals() const { return normals.cols() > 0; }
    /// @brief テクスチャ座標を持つか
    bool HasUVs() const { return uvs.cols() > 0; }
    /// @brief 頂点数を取得する
    std::size_t VertexCount() const {
        return static_cast<std::size_t>(positions.cols());
    }
    /// @brief 三角形数を取得する
    std::size_t TriangleCount() const { return indices.size() / 3; }
};

/// @brief 倍精度の三角形メッシュ (解析・オーサリングの既定)
using TriangleMeshd = TriangleMeshT<double>;
/// @brief 単精度の三角形メッシュ (GPUステージング・STLバイナリ)
using TriangleMeshf = TriangleMeshT<float>;

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESH_TRIANGLE_MESH_H_
