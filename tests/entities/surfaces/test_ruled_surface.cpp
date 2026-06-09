/**
 * @file tests/entities/surfaces/test_ruled_surface.cpp
 * @brief RuledSurface (Type 118) の構築・編集APIのテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [ファクトリ関数]
 *     MakeRuledSurface(curve1, curve2, is_reversed, is_developable)
 *   [要素の変更・取得]
 *     GetSurfaceForm(), SetSurfaceForm(form)
 *     SetCurve1(curve), SetCurve2(curve) (既存APIの基本動作の補完)
 *   [コンストラクタ]
 *     RuledSurface(curve1, curve2, ...) のnullptr検出 (回帰)
 *
 * TODO:
 *   - 評価系 (TryGetDefinedDerivatives, GetDefinedBoundingBox等) の網羅は
 *     test_i_surface.cppの責務 (本ファイルではコーナー点の対応のみ検証)
 *   - GetMainPDParameters()経由の書き出しはprotectedのため対象外
 *     (test_igesio側の責務)
 *   - 規格上参照可能なPoint (Type 116) を片側に用いる縮退ケース (錐面) は
 *     PointがICurveを継承していないため未対応 (ruled_surface.hのTODO参照)
 *   - Form 0 (等相対弧長) の弧長再パラメータ化評価は機能未実装のため対象外
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/surfaces/ruled_surface.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector3d;
using i_ent::RuledSurface;
using i_ent::RuledSurfaceForm;
using i_ent::SubordinateEntitySwitch;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;



/**
 * 検証ヘルパー・フィクスチャ
 */

/// @brief Vector3dの成分一致を検証する
void ExpectVectorNear(const Vector3d& actual, const Vector3d& expected,
                      const double tol = kTol) {
    EXPECT_NEAR(actual.x(), expected.x(), tol);
    EXPECT_NEAR(actual.y(), expected.y(), tol);
    EXPECT_NEAR(actual.z(), expected.z(), tol);
}

/// @brief 曲面上の点S(u, v)を評価する (TryGetDefinedDerivativesの0階)
Vector3d EvalSurface(const i_ent::ISurface& surface,
                     const double u, const double v) {
    auto deriv = surface.TryGetDefinedDerivatives(u, v, 0);
    EXPECT_TRUE(deriv.has_value());
    if (!deriv.has_value()) return Vector3d::Zero();
    return deriv.value()(0, 0);
}

/// @brief C1: X軸に沿う線分 (0,0,0)→(10,0,0)
std::shared_ptr<i_ent::Line> MakeCurve1() {
    return i_ent::MakeLine(Vector3d(0.0, 0.0, 0.0), Vector3d(10.0, 0.0, 0.0));
}

/// @brief C2: C1をY方向に+5だけ平行移動した線分 (0,5,0)→(10,5,0)
std::shared_ptr<i_ent::Line> MakeCurve2() {
    return i_ent::MakeLine(Vector3d(0.0, 5.0, 0.0), Vector3d(10.0, 5.0, 0.0));
}

/// @brief 角を1つ持つ折れ線を作成する (xy平面内のV字)
/// @param y_offset 全頂点のy座標オフセット (C1/C2を分離するため)
/// @param apex_x 中央頂点のx座標 (非対称化に使用; 既定は対称な0)
std::shared_ptr<i_ent::LinearPath> MakeCorneredPath(
        const double y_offset, const double apex_x = 0.0) {
    return i_ent::MakeLinearPath(std::vector<Vector3d>{
        Vector3d{-4., y_offset, 0.}, Vector3d{apex_x, y_offset + 3., 0.},
        Vector3d{4., y_offset, 0.}});
}

/// @brief 折れ目におけるパラメータ列を順不同で比較する
void ExpectCreasesNear(std::vector<double> actual,
                       std::vector<double> expected, const double tol = kTol) {
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
        EXPECT_NEAR(actual[i], expected[i], tol);
    }
}

}  // namespace



/**
 * ファクトリ関数: 正常系
 */

TEST(RuledSurfaceFactoryTest, MakeRuledSurface_StoredValues) {
    const auto c1 = MakeCurve1();
    const auto c2 = MakeCurve2();
    const auto surface = i_ent::MakeRuledSurface(c1, c2);

    EXPECT_EQ(surface->GetCurve1()->GetID(), c1->GetID());
    EXPECT_EQ(surface->GetCurve2()->GetID(), c2->GetID());
    // フラグのデフォルトはともにfalse
    EXPECT_FALSE(surface->IsReversed());
    EXPECT_FALSE(surface->IsDevelopable());
}

TEST(RuledSurfaceFactoryTest, MakeRuledSurface_FlagsStored) {
    const auto surface = i_ent::MakeRuledSurface(
            MakeCurve1(), MakeCurve2(), true, true);
    EXPECT_TRUE(surface->IsReversed());
    EXPECT_TRUE(surface->IsDevelopable());
}

TEST(RuledSurfaceFactoryTest, MakeRuledSurface_FormNumberIsEqualParameters) {
    // 与えられたパラメータ化をそのまま用いるため、Form 1が設定される
    const auto surface = i_ent::MakeRuledSurface(MakeCurve1(), MakeCurve2());
    EXPECT_EQ(surface->GetSurfaceForm(), RuledSurfaceForm::kEqualParameters);
    EXPECT_EQ(surface->GetFormNumber(), 1);
}

TEST(RuledSurfaceFactoryTest, MakeRuledSurface_SetsCurvesPhysicallyDependent) {
    const auto c1 = MakeCurve1();
    const auto c2 = MakeCurve2();
    const auto surface = i_ent::MakeRuledSurface(c1, c2);

    EXPECT_EQ(c1->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(c2->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

TEST(RuledSurfaceFactoryTest, MakeRuledSurface_ValidationSucceeds) {
    const auto surface = i_ent::MakeRuledSurface(MakeCurve1(), MakeCurve2());
    EXPECT_TRUE(surface->Validate().is_valid);
}

// DIRFLG=0: 始点同士・終点同士が結ばれる
TEST(RuledSurfaceFactoryTest, MakeRuledSurface_CornerPoints) {
    const auto surface = i_ent::MakeRuledSurface(MakeCurve1(), MakeCurve2());

    ExpectVectorNear(EvalSurface(*surface, 0.0, 0.0), Vector3d(0.0, 0.0, 0.0));
    ExpectVectorNear(EvalSurface(*surface, 1.0, 0.0), Vector3d(10.0, 0.0, 0.0));
    ExpectVectorNear(EvalSurface(*surface, 0.0, 1.0), Vector3d(0.0, 5.0, 0.0));
    ExpectVectorNear(EvalSurface(*surface, 1.0, 1.0), Vector3d(10.0, 5.0, 0.0));
}

// DIRFLG=1: 一方の始点と他方の終点が結ばれる
TEST(RuledSurfaceFactoryTest, MakeRuledSurface_Reversed_CornerPoints) {
    const auto surface = i_ent::MakeRuledSurface(
            MakeCurve1(), MakeCurve2(), true);

    ExpectVectorNear(EvalSurface(*surface, 0.0, 0.0), Vector3d(0.0, 0.0, 0.0));
    ExpectVectorNear(EvalSurface(*surface, 0.0, 1.0), Vector3d(10.0, 5.0, 0.0));
    ExpectVectorNear(EvalSurface(*surface, 1.0, 1.0), Vector3d(0.0, 5.0, 0.0));
}



/**
 * ファクトリ関数: 異常系
 */

TEST(RuledSurfaceFactoryTest,
     MakeRuledSurface_ThrowsInvalidArgumentWhenCurve1IsNull) {
    EXPECT_THROW(i_ent::MakeRuledSurface(nullptr, MakeCurve2()),
                 std::invalid_argument);
}

TEST(RuledSurfaceFactoryTest,
     MakeRuledSurface_ThrowsInvalidArgumentWhenCurve2IsNull) {
    EXPECT_THROW(i_ent::MakeRuledSurface(MakeCurve1(), nullptr),
                 std::invalid_argument);
}

TEST(RuledSurfaceFactoryTest,
     MakeRuledSurface_ThrowsEntityValueErrorWhenSameEntity) {
    const auto c1 = MakeCurve1();
    EXPECT_THROW(i_ent::MakeRuledSurface(c1, c1), igesio::EntityValueError);
    // 境界の内側: 幾何的に同一でも別エンティティであれば許容される
    EXPECT_NO_THROW(i_ent::MakeRuledSurface(MakeCurve1(), MakeCurve1()));
}



/**
 * フォーム番号アクセサ
 */

// PDコンストラクタ (RawEntityDE::ByDefault) はForm 0で生成される
TEST(RuledSurfaceFormTest, GetSurfaceForm_DefaultConstructedIsEqualArcLength) {
    const auto c1 = MakeCurve1();
    const auto c2 = MakeCurve2();
    const RuledSurface surface(
            igesio::IGESParameterVector{c1->GetID(), c2->GetID(), false, false});

    EXPECT_EQ(surface.GetSurfaceForm(), RuledSurfaceForm::kEqualArcLength);
    EXPECT_EQ(surface.GetFormNumber(), 0);
}

TEST(RuledSurfaceFormTest, SetSurfaceForm_UpdatesFormNumber) {
    const auto surface = i_ent::MakeRuledSurface(MakeCurve1(), MakeCurve2());

    surface->SetSurfaceForm(RuledSurfaceForm::kEqualArcLength);
    EXPECT_EQ(surface->GetFormNumber(), 0);
    EXPECT_EQ(surface->GetSurfaceForm(), RuledSurfaceForm::kEqualArcLength);

    surface->SetSurfaceForm(RuledSurfaceForm::kEqualParameters);
    EXPECT_EQ(surface->GetFormNumber(), 1);
    EXPECT_EQ(surface->GetSurfaceForm(), RuledSurfaceForm::kEqualParameters);
}



/**
 * コンストラクタ: nullptr検出の回帰テスト
 * (委譲コンストラクタ引数内での逆参照によりUBだった経路)
 */

TEST(RuledSurfaceConstructorTest,
     Constructor_ThrowsInvalidArgumentWhenCurve1IsNull) {
    EXPECT_THROW(RuledSurface(nullptr, MakeCurve2()), std::invalid_argument);
}

TEST(RuledSurfaceConstructorTest,
     Constructor_ThrowsInvalidArgumentWhenCurve2IsNull) {
    EXPECT_THROW(RuledSurface(MakeCurve1(), nullptr), std::invalid_argument);
}



/**
 * セッター (既存APIの基本動作の補完)
 */

TEST(RuledSurfaceSetterTest, SetCurve1_ReplacesCurveAndChildIDs) {
    const auto c2 = MakeCurve2();
    const auto surface = i_ent::MakeRuledSurface(MakeCurve1(), c2);

    const auto c3 = i_ent::MakeLine(Vector3d(0.0, 0.0, 7.0),
                                    Vector3d(10.0, 0.0, 7.0));
    surface->SetCurve1(c3);
    EXPECT_EQ(surface->GetCurve1()->GetID(), c3->GetID());

    const auto ids = surface->GetChildIDs();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], c3->GetID());
    EXPECT_EQ(ids[1], c2->GetID());
}

TEST(RuledSurfaceSetterTest, SetCurve1_ThrowsInvalidArgumentWhenNull) {
    const auto surface = i_ent::MakeRuledSurface(MakeCurve1(), MakeCurve2());
    EXPECT_THROW(surface->SetCurve1(nullptr), std::invalid_argument);
}

TEST(RuledSurfaceSetterTest, SetCurve2_ThrowsInvalidArgumentWhenNull) {
    const auto surface = i_ent::MakeRuledSurface(MakeCurve1(), MakeCurve2());
    EXPECT_THROW(surface->SetCurve2(nullptr), std::invalid_argument);
}

// セッターは同一エンティティの設定を許容し、Validate()が検出する
// (現状仕様の固定化; ファクトリは構築時にEntityValueErrorとする)
TEST(RuledSurfaceSetterTest, Validate_ErrorWhenSameCurveBothSides) {
    const auto c1 = MakeCurve1();
    const auto surface = i_ent::MakeRuledSurface(c1, MakeCurve2());

    EXPECT_NO_THROW(surface->SetCurve2(c1));
    EXPECT_FALSE(surface->Validate().is_valid);
}



/**
 * GetUCreaseParameters() のテスト
 *
 * C1の角は u=(t-t0)/(t1-t0)、C2の角は DIRFLG に応じて
 * 非反転 u=(s-s0)/(s1-s0) / 反転 u=(s1-s)/(s1-s0) へ写像し、和集合を返す.
 */

// C1のみ角を持つ場合、C1の角を u へ写像して返す (対称V字 -> u=0.5)
TEST(RuledSurfaceCreaseTest, GetUCreaseParameters_MapsCurve1Corner) {
    auto c1 = MakeCorneredPath(0.0);  // 対称V字
    auto surface = i_ent::MakeRuledSurface(c1, MakeCurve2());

    const auto creases = surface->GetUCreaseParameters();
    ASSERT_EQ(creases.size(), 1u);
    EXPECT_NEAR(creases[0], 0.5, kTol);
}

// 両曲線が角を持つ場合、両方を u へ写像した和集合を返す
TEST(RuledSurfaceCreaseTest, GetUCreaseParameters_UnionsBothCurveCorners) {
    auto c1 = MakeCorneredPath(0.0);          // 対称 -> u=0.5
    auto c2 = MakeCorneredPath(5.0, 1.0);     // 非対称 -> u≠0.5
    auto surface = i_ent::MakeRuledSurface(c1, c2);

    const auto r1 = c1->GetParameterRange();
    const auto r2 = c2->GetParameterRange();
    const double u1 = (c1->GetCornerParams()[0] - r1[0]) / (r1[1] - r1[0]);
    const double u2 = (c2->GetCornerParams()[0] - r2[0]) / (r2[1] - r2[0]);
    ExpectCreasesNear(surface->GetUCreaseParameters(), {u1, u2});
}

// DIRFLG反転時、C2の角は終端からの u=(s1-s)/(s1-s0) へ写像される
TEST(RuledSurfaceCreaseTest, GetUCreaseParameters_ReversedMapsCurve2FromEnd) {
    auto c1 = MakeCurve1();                // 直線 (角なし)
    auto c2 = MakeCorneredPath(5.0, 1.0);  // 非対称な角

    auto fwd = i_ent::MakeRuledSurface(c1, c2, /*is_reversed=*/false);
    auto rev = i_ent::MakeRuledSurface(c1, c2, /*is_reversed=*/true);

    const auto r2 = c2->GetParameterRange();
    const double sc = c2->GetCornerParams()[0];
    const double u_fwd = (sc - r2[0]) / (r2[1] - r2[0]);
    const double u_rev = (r2[1] - sc) / (r2[1] - r2[0]);

    ASSERT_EQ(fwd->GetUCreaseParameters().size(), 1u);
    ASSERT_EQ(rev->GetUCreaseParameters().size(), 1u);
    EXPECT_NEAR(fwd->GetUCreaseParameters()[0], u_fwd, kTol);
    EXPECT_NEAR(rev->GetUCreaseParameters()[0], u_rev, kTol);
    // 非対称な角のため、反転で値が変わることを確認 (反転写像の検証)
    EXPECT_GT(std::abs(u_fwd - u_rev), 1e-3);
}

// 両曲線とも滑らか (直線) の場合は空
TEST(RuledSurfaceCreaseTest, GetUCreaseParameters_EmptyWhenBothSmooth) {
    auto surface = i_ent::MakeRuledSurface(MakeCurve1(), MakeCurve2());
    EXPECT_TRUE(surface->GetUCreaseParameters().empty());
}
