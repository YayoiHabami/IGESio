/**
 * @file entities/curves/algorithms/polygonal_approximation.h
 * @brief 曲線の折れ線近似アルゴリズム (内部実装)
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 * @note 本ファイルは内部実装用であり、公開APIには含めない.
 */
#ifndef SRC_ENTITIES_CURVES_ALGORITHMS_POLYGONAL_APPROXIMATION_H_
#define SRC_ENTITIES_CURVES_ALGORITHMS_POLYGONAL_APPROXIMATION_H_

#include <memory>

#include "igesio/numerics/polygon.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

/// @brief 曲線を折れ線近似した近似多角形を生成する
///
/// 内包多角形・外包多角形の全頂点 (on_curve=trueのもの) を固定点として含み、
/// 各固定点間の弧を「辺と弧の中点との距離 <= eps」を満たすまで反復二分する.
/// inscribed・circumscribedが両方空の場合は、一般曲線の折れ線化相当の動作となる.
///
/// @param curve 対象の曲線 (開曲線・閉曲線どちらも可)
/// @param inscribed 内包多角形 (空でもよい)
/// @param circumscribed 外包多角形 (空でもよい)
/// @param eps 許容距離 (辺と弧の中点との点-直線距離)
/// @param max_depth 反復スタックの最大深度
/// @return 近似多角形 (全頂点 on_curve=true)
/// @note inscribed, circumscribedの両者に{}を渡した場合、curveは開曲線であっても
///       正しく処理され、単にcurveを許容距離epsを満たすように離散化した折れ線が返る.
numerics::PolygonData ComputeApproximatePolygon(
    const std::shared_ptr<const ICurve>& curve,
    const numerics::PolygonData& inscribed,
    const numerics::PolygonData& circumscribed,
    double eps,
    int max_depth = 1000);

}  // namespace igesio::entities

#endif  // SRC_ENTITIES_CURVES_ALGORITHMS_POLYGONAL_APPROXIMATION_H_
