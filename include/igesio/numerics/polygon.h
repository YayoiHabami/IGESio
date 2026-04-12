/**
 * @file numerics/polygon.h
 * @brief 多角形データ構造の定義
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_POLYGON_H_
#define IGESIO_NUMERICS_POLYGON_H_

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"



namespace igesio::numerics {

/// @brief 多角形の頂点と曲線パラメータを保持するデータ構造
/// @note 閉曲線 C(t) の外包/内包多角形を表現するために使用する.
struct PolygonData {
    /// @brief 多角形の頂点座標 (3次元)
    std::vector<Vector3d> vertices;

    /// @brief 各頂点が曲線C(t)上にあるか否か
    /// @note verticesと同じ長さであること
    std::vector<bool> on_curve;

    /// @brief 各頂点の曲線パラメータ値
    /// @note verticesと同じ長さであること.
    ///       対応するon_curveがfalseの場合は0.0を格納する.
    ///       on_curveがtrueの頂点については昇順で並んでいることが期待される.
    std::vector<double> curve_params;

    /// @brief 頂点数を返す
    /// @return verticesのサイズ
    int Count() const noexcept;

    /// @brief 指定された辺が含まれる曲線パラメータ範囲の境界頂点インデックスを返す
    ///
    /// 辺のインデックスiは、頂点iから頂点 (i+1) % M への辺を表す.
    /// 両端点のうちon_curveがfalseの頂点は制御点とみなし、
    /// その頂点を挟むon_curveがtrueの頂点までパラメータ範囲を拡張する.
    ///
    /// @param edge_index 辺のインデックス (0 以上 Count()-1 未満)
    /// @return (i_start, i_end): いずれも on_curve=true の頂点を指すインデックス.
    ///         パラメータ範囲をまたぐ場合 (曲線末尾側→先頭側) は i_start > i_end となる.
    /// @throws std::out_of_range edge_indexが範囲外の場合
    /// @throws std::invalid_argument 全頂点が on_curve=false の場合
    std::pair<int, int> GetCurveParamIndex(int edge_index) const;
};

/// @brief 閉曲線C(t)の内包・外包・近似多角形を保持するデータ構造
struct CurveContainmentPolygons {
    /// @brief 外包多角形
    PolygonData circumscribed;
    /// @brief 内包多角形
    PolygonData inscribed;
    /// @brief 近似多角形 (全頂点 on_curve=true)
    PolygonData approximate;
};

/// @brief 閉曲線C(u)に対して、点が曲線内部に存在するかを判定する
///
/// CurveContainmentPolygons に格納された外包多角形・内包多角形・近似多角形を
/// 利用して内外判定を行う. 内外判定は Non-Zero Winding 則を使用する.
///
/// @param point 判定する点 (x, y 成分のみ使用)
/// @param polygons 閉曲線C(u)の内包・外包・近似多角形
/// @return 点が曲線C(u)の内部に存在する場合 true
bool IsPointInPolygon(
    const Vector3d& point,
    const CurveContainmentPolygons& polygons);

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_POLYGON_H_
