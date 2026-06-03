/**
 * @file entities/surfaces/algorithms/restricted_surface_mesh.h
 * @brief 制限付き曲面 (Type 143/144 等) の三角形メッシュ生成
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 *
 * @details
 * IRestrictedSurface の具象クラスに依存せず、以下のAPIのみを使用する:
 *   - TryGetDerivatives(u, v, 1) : 点座標S(u,v)と偏導関数Su, Svの取得
 *                                  (トリム領域外ではnulloptを返す)
 *   - IsInDomain(u, v)           : トリム後の有効ドメイン内外判定 (交差点探索用)
 *   - GetOuterDomainPolygon() / GetInnerDomainPolygons()
 *                                : 境界の包含多角形 (細分すべきセルのマーク用)
 *
 * ### アルゴリズム概要
 * 基底base_div x base_divグリッドを四分木のルートとし、トリム境界が通過する
 * ルートのみを最大深さmax_depthまで細分する (境界駆動の制限付き四分木)。
 * 隣接ルートの深さ差を1以下に均すこと(2:1バランス)で各辺のハンギングノードを
 * 高々1個に抑え、ハンギングノードを周回に含めることでクラックフリーな
 * 三角分割を得る。各葉ではMarching-Squaresと同様に有効コーナーと境界交差点で
 * CCW多角形を構成しファン三角分割する。
 *
 * 全頂点・交差点を「仮想最細格子」(base_div * 2^max_depth 解像度)の整数座標で
 * キー化して重複排除するため、隣接する葉は境界カットを含め必ず同一頂点を共有する
 * (クラックフリーの保証)。境界が通らない領域は base_div の粗さで分割されるため、
 * 細い形状・欠け角の脱落が起きる境界近傍にのみ細分コストを払う。
 */
#ifndef IGESIO_ENTITIES_SURFACES_ALGORITHMS_RESTRICTED_SURFACE_MESH_H_
#define IGESIO_ENTITIES_SURFACES_ALGORITHMS_RESTRICTED_SURFACE_MESH_H_

#include <cstdint>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_restricted_surface.h"



namespace igesio::entities {

/// @brief 制限付き曲面の三角形メッシュ (頂点属性 + インデックス)
struct RestrictedSurfaceMesh {
    /// @brief 頂点属性(8 x N)。各列が {x, y, z, nx, ny, nz, tu, tv}
    /// @note tu, tvはパラメータ範囲を [0,1] に正規化したテクスチャ座標
    MatrixXf vertices;
    /// @brief 三角形インデックス (3個で1三角形を構成)
    std::vector<std::uint32_t> indices;
};

/// @brief TessellateRestrictedSurface のテッセレーション制御パラメータ
struct RestrictedSurfaceMeshParams {
    /// @brief 基底グリッド分割数 (u,v各方向)
    /// @note トリム境界が通らない領域はこの粗さで三角分割される
    int base_div = 32;
    /// @brief 境界セルを細分する四分木の最大深さ
    /// @note 局所的に実効分割数が base_div * 2^max_depth まで上がる
    ///       (既定では 32 * 4 = 128 相当)。細い形状・欠け角の脱落を救済する
    int max_depth = 2;
};

/// @brief 制限付き曲面 (Type 143/144等) を三角形メッシュへテッセレーションする
///
/// 境界駆動の制限付き四分木により、トリム境界近傍を適応的に細分しつつ、
/// 境界が通らない領域はbase_divの一様グリッドで分割する。出力頂点は
/// 基底曲面のモデル空間 (M_base適用済み) の座標・法線と、パラメータ範囲を
/// [0,1] に正規化したテクスチャ座標を持つ。
///
/// @param surface テッセレーション対象の制限付き曲面
/// @param params  テッセレーション制御パラメータ
/// @return 三角形メッシュ (頂点属性とインデックス)
/// @note 境界曲線が未解決・退化等で包含多角形が構築できない場合、当該境界の
///       細分はスキップされ、基底グリッドのMarching-Squaresにフォールバックする
RestrictedSurfaceMesh TessellateRestrictedSurface(
        const IRestrictedSurface& surface,
        const RestrictedSurfaceMeshParams& params = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_ALGORITHMS_RESTRICTED_SURFACE_MESH_H_
