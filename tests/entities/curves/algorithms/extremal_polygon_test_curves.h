/**
 * @file tests/entities/curves/algorithms/extremal_polygon_test_curves.h
 * @brief extremal_polygon アルゴリズム検証用のテスト曲線フィクスチャ
 * @author Yayoi Habami
 * @copyright 2026 Yayoi Habami
 * @note 本ヘッダは生成物である。M16_solidworks_s144_flatten_part1.IGS の
 *       パラメータ空間トリミングループ (142 の BPTR) を逐語抽出したものであり、
 *       テストは元の .IGS ファイルに依存しない。全ループは z=0 平面上の閉曲線で、
 *       参照法線は (0,0,1)。各構成曲線は 126 (RationalBSplineCurve)。
 */
#ifndef IGESIO_TESTS_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_TEST_CURVES_H_
#define IGESIO_TESTS_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_TEST_CURVES_H_

#include <memory>

#include "igesio/common/iges_parameter_vector.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"



namespace igesio::tests {

using igesio::entities::CompositeCurve;
using igesio::entities::RationalBSplineCurve;

/// @brief 曲線アーチの帯ループ1 (2次NURBS×2 + 1次NURBS×2)。z=0平面上の非凸閉ループ。
///        102,4,5,11,15,19 (DE 23) のパラメータ空間トリミングループ。
inline std::shared_ptr<CompositeCurve> MakeLoopArchBand1() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            16, 2,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (20)
            0., 0., 0., 0.125, 0.125, 0.25, 0.25, 0.375, 0.375,
            0.5, 0.5, 0.625, 0.625, 0.75, 0.75, 0.875, 0.875, 1.,
            1., 1.,
            // 重み (17)
            1., 1., 1., 1., 1., 1., 1., 1., 1.,
            1., 1., 1., 1., 1., 1., 1., 1.,
            // 制御点 (17 点)
            0., 0.005928854, 0.,
            0.05817466, 0.069902082, 0.,
            0.118120371, 0.123799921, 0.,
            0.178066082, 0.17769776, 0.,
            0.239475926, 0.221244257, 0.,
            0.300885771, 0.264790755, 0.,
            0.363445334, 0.297762954, 0.,
            0.426004897, 0.330735154, 0.,
            0.489393877, 0.35296424, 0.,
            0.552782857, 0.375193327, 0.,
            0.616676707, 0.386565489, 0.,
            0.680570558, 0.397937651, 0.,
            0.744642145, 0.398394663, 0.,
            0.808713732, 0.398851675, 0.,
            0.872635014, 0.388391198, 0.,
            0.936556296, 0.377930721, 0.,
            1., 0.356606312, 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            1., 0.356606312, 0.,
            1., 0.952263687, 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            16, 2,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (20)
            0., 0., 0., 0.125, 0.125, 0.25, 0.25, 0.375, 0.375,
            0.5, 0.5, 0.625, 0.625, 0.75, 0.75, 0.875, 0.875, 1.,
            1., 1.,
            // 重み (17)
            1., 1., 1., 1., 1., 1., 1., 1., 1.,
            1., 1., 1., 1., 1., 1., 1., 1.,
            // 制御点 (17 点)
            1., 0.952263687, 0.,
            0.936556296, 0.973588096, 0.,
            0.872635014, 0.984048573, 0.,
            0.808713732, 0.99450905, 0.,
            0.744642145, 0.994052038, 0.,
            0.680570558, 0.993595026, 0.,
            0.616676707, 0.982222864, 0.,
            0.552782857, 0.970850702, 0.,
            0.489393877, 0.948621615, 0.,
            0.426004897, 0.926392529, 0.,
            0.363445334, 0.893420329, 0.,
            0.300885771, 0.86044813, 0.,
            0.239475926, 0.816901632, 0.,
            0.178066082, 0.773355135, 0.,
            0.118120371, 0.719457296, 0.,
            0.05817466, 0.665559457, 0.,
            0., 0.601586229, 0.,
            0., 1.,  // V(0), V(1)
            0., 0., -1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            0., 0.601586229, 0.,
            0., 0.005928854, 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    return cc;
}

/// @brief 曲線アーチの帯ループ2 (2次NURBS×2 + 1次NURBS×2)。ループ1とほぼ鏡像。
///        102,4,33,39,43,47 (DE 51) のパラメータ空間トリミングループ。
inline std::shared_ptr<CompositeCurve> MakeLoopArchBand2() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            16, 2,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (20)
            0., 0., 0., 0.125, 0.125, 0.25, 0.25, 0.375, 0.375,
            0.5, 0.5, 0.625, 0.625, 0.75, 0.75, 0.875, 0.875, 1.,
            1., 1.,
            // 重み (17)
            1., 1., 1., 1., 1., 1., 1., 1., 1.,
            1., 1., 1., 1., 1., 1., 1., 1.,
            // 制御点 (17 点)
            1., 0.601586229, 0.,
            0.94182534, 0.665559457, 0.,
            0.881879629, 0.719457296, 0.,
            0.821933918, 0.773355135, 0.,
            0.760524074, 0.816901632, 0.,
            0.699114229, 0.86044813, 0.,
            0.636554666, 0.893420329, 0.,
            0.573995103, 0.926392529, 0.,
            0.510606123, 0.948621615, 0.,
            0.447217143, 0.970850702, 0.,
            0.383323293, 0.982222864, 0.,
            0.319429442, 0.993595026, 0.,
            0.255357855, 0.994052038, 0.,
            0.191286268, 0.99450905, 0.,
            0.127364986, 0.984048573, 0.,
            0.063443704, 0.973588096, 0.,
            0., 0.952263687, 0.,
            0., 1.,  // V(0), V(1)
            0., 0., -1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            0., 0.952263687, 0.,
            0., 0.356606312, 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            16, 2,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (20)
            0., 0., 0., 0.125, 0.125, 0.25, 0.25, 0.375, 0.375,
            0.5, 0.5, 0.625, 0.625, 0.75, 0.75, 0.875, 0.875, 1.,
            1., 1.,
            // 重み (17)
            1., 1., 1., 1., 1., 1., 1., 1., 1.,
            1., 1., 1., 1., 1., 1., 1., 1.,
            // 制御点 (17 点)
            0., 0.356606312, 0.,
            0.063443704, 0.377930721, 0.,
            0.127364986, 0.388391198, 0.,
            0.191286268, 0.398851675, 0.,
            0.255357855, 0.398394663, 0.,
            0.319429442, 0.397937651, 0.,
            0.383323293, 0.386565489, 0.,
            0.447217143, 0.375193327, 0.,
            0.510606123, 0.35296424, 0.,
            0.573995103, 0.330735154, 0.,
            0.636554666, 0.297762954, 0.,
            0.699114229, 0.264790755, 0.,
            0.760524074, 0.221244257, 0.,
            0.821933918, 0.17769776, 0.,
            0.881879629, 0.123799921, 0.,
            0.94182534, 0.069902082, 0.,
            1., 0.005928854, 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            1., 0.005928854, 0.,
            1., 0.601586229, 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    return cc;
}

/// @brief 矩形ループ1 (1次NURBS×4)。全辺直線・角4つの退化ケース。
///        102,4,61,65,69,73 (DE 77) のパラメータ空間トリミングループ。
inline std::shared_ptr<CompositeCurve> MakeLoopRect1() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            1., 0., 0.,
            1., 1., 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            1., 1., 0.,
            0., 1., 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            0., 1., 0.,
            0., 0., 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            0., 0., 0.,
            1., 0., 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    return cc;
}

/// @brief 矩形ループ2 (1次NURBS×4, 単位矩形)。全辺直線・角4つの退化ケース。
///        102,4,87,93,97,101 (DE 105) のパラメータ空間トリミングループ。
inline std::shared_ptr<CompositeCurve> MakeLoopRect2() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            0., 0., 0.,
            1., 0., 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            1., 0., 0.,
            1., 1., 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            1., 1., 0.,
            0., 1., 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,  // K (制御点数-1), M (次数)
            true, false, true, false,  // PROP1-4 (平面/閉/多項式/周期)
            // ノットベクトル (4)
            0., 0., 1., 1.,
            // 重み (2)
            1., 1.,
            // 制御点 (2 点)
            0., 1., 0.,
            0., 0., 0.,
            0., 1.,  // V(0), V(1)
            0., 0., 1.  // 定義平面の法線
        }));
    return cc;
}

}  // namespace igesio::tests

#endif  // IGESIO_TESTS_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_TEST_CURVES_H_
