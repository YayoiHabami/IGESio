/**
 * @file tests/entities/test_geometry_revision.cpp
 * @brief ジオメトリリビジョン (GeometryRevision/MarkGeometryModified) のテスト
 * @author Yayoi Habami
 * @date 2026-06-06
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象 (Scene Reconcile フェーズ2):
 *   - IEntityIdentifier::GeometryRevision() / EntityBase実装 (初期値・単調増加)
 *   - 形状に影響するmutatorのバンプ配線 (クラス別網羅):
 *     RationalBSplineCurve / RationalBSplineSurface / SurfaceOfRevolution /
 *     RuledSurface / TabulatedCylinder / TrimmedSurface / CompositeCurve /
 *     CurveOnAParametricSurface / Point / TransformationMatrix
 *   - 失敗経路 (throw・false返却) での不変
 *   - CurveView / SurfaceView の転送override
 *
 * NOTE: バンプ回数は検証しない (増加/不変のみ検証する). レンダラとの契約は
 *       「変化の検知」であり、回数は実装詳細のため (例: 委譲mutatorは複数回
 *       バンプしうる).
 *
 * TODO: Point::SetSubfigureはISubfigureDefinitionの具象実装が存在しないため
 *       未カバー (実装追加時にテストを追加すること).
 * TODO: 読込時参照解決 (SetUnresolvedReference系) が非バンプであることは
 *       仕様だが、内部経路のためテストでは固定しない.
 * TODO: 共有TransformationMatrix編集による参照元graphicsの再同期は
 *       graphics層 (フェーズ3) の同期キーテストの管轄.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/point.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"
#include "igesio/entities/surfaces/ruled_surface.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/entities/surfaces/tabulated_cylinder.h"
#include "igesio/entities/surfaces/trimmed_surface.h"
#include "igesio/entities/transformations/transformation_matrix.h"
#include "igesio/entities/views/curve_view.h"
#include "igesio/entities/views/surface_view.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::RationalBSplineCurve;
using i_ent::RationalBSplineSurface;



/**
 * テスト用インスタンスのファクトリ関数
 */

/// @brief {x0,y0,z0, x1,y1,z1, ...} から Matrix3Xd を構築するヘルパー
/// @param coords 制御点座標の列 (x, y, z の順)
igesio::Matrix3Xd MakeCPs(std::initializer_list<double> coords) {
    const int n = static_cast<int>(coords.size()) / 3;
    igesio::Matrix3Xd cp(3, n);
    auto it = coords.begin();
    for (int i = 0; i < n; ++i) {
        cp(0, i) = *it++;
        cp(1, i) = *it++;
        cp(2, i) = *it++;
    }
    return cp;
}

/// @brief degree=2 Bezier曲線 P0=(0,0,0), P1=(1,1,0), P2=(2,0,0)
std::shared_ptr<RationalBSplineCurve> MakeTestCurve() {
    return i_ent::MakeBezierCurve(MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0}));
}

/// @brief 双一次NURBS平面 (z=0, ドメイン[0,1]²)
/// @note 呼び出しごとに別IDの新規エンティティが生成される
std::shared_ptr<RationalBSplineSurface> MakePlane() {
    const std::vector<std::vector<Vector3d>> grid{
        {{0, 0, 0}, {0, 1, 0}},
        {{1, 0, 0}, {1, 1, 0}}};
    return i_ent::MakeBezierSurface(grid);
}

/// @brief 指定曲面上の矩形境界 (Type 142; C(t)は自動生成)
std::shared_ptr<i_ent::CurveOnAParametricSurface> MakeRectBoundaryOn(
        const std::shared_ptr<i_ent::ISurface>& surface,
        const double lo = 0.2, const double hi = 0.8) {
    auto loop = i_ent::MakeLinearPath(
            std::vector<Vector2d>{{lo, lo}, {hi, lo}, {hi, hi}, {lo, hi}},
            true);
    // 戻り値ペアの第2要素 (自動生成されたC(t)) は本テストでは使用しない
    return i_ent::MakeCurveOnAParametricSurface(surface, loop).first;
}



/**
 * バンプ検証ヘルパー
 */

/// @brief mutator呼び出し毎のリビジョン増加をチェーン検証するヘルパー
/// @note Check()は前回値からの増加を検証し、基準値を更新する.
///       Unchanged()は前回値からの不変を検証する.
class RevisionChecker {
 public:
    /// @brief コンストラクタ
    /// @param entity 監視対象のエンティティ
    explicit RevisionChecker(const i_ent::IEntityIdentifier& entity)
            : entity_(entity), last_(entity.GeometryRevision()) {}

    /// @brief 前回値から増加していることを検証し、基準値を更新する
    /// @param what 失敗時に表示する操作名
    void Check(const char* what) {
        EXPECT_GT(entity_.GeometryRevision(), last_) << what;
        last_ = entity_.GeometryRevision();
    }

    /// @brief 前回値から変化していないことを検証する
    /// @param what 失敗時に表示する操作名
    void Unchanged(const char* what) const {
        EXPECT_EQ(entity_.GeometryRevision(), last_) << what;
    }

 private:
    /// @brief 監視対象のエンティティ
    const i_ent::IEntityIdentifier& entity_;
    /// @brief 前回検証時のリビジョン値
    uint64_t last_;
};

}  // namespace



/**
 * 基本セマンティクス
 */

// 新規エンティティの初期値は1以上 (既定実装0=「形状を持たない」と区別される)
TEST(GeometryRevisionTest, InitialValueIsPositive) {
    EXPECT_GE(MakeTestCurve()->GeometryRevision(), 1u);
    EXPECT_GE(MakePlane()->GeometryRevision(), 1u);
    EXPECT_GE(std::make_shared<i_ent::Point>(Vector3d{1, 2, 3})
                      ->GeometryRevision(), 1u);
}

// const操作 (評価・範囲・BBox取得) ではリビジョンが変化しない
TEST(GeometryRevisionTest, ConstQueriesDoNotChange) {
    auto curve = MakeTestCurve();
    RevisionChecker rev(*curve);

    (void)curve->TryGetDefinedDerivatives(0.5, 1);
    (void)curve->GetParameterRange();
    (void)curve->GetDefinedBoundingBox();
    (void)curve->IsClosed();
    rev.Unchanged("const queries");
}



/**
 * クラス別バンプ網羅 (形状setter)
 */

// RationalBSplineCurve: 全形状mutatorでバンプされる
TEST(GeometryRevisionTest, RationalBSplineCurve_AllShapeSettersBump) {
    auto curve = MakeTestCurve();
    RevisionChecker rev(*curve);

    curve->SetControlPointAt(1, {1, 2, 0});
    rev.Check("SetControlPointAt");
    curve->SetControlPoints(MakeCPs({0, 0, 0, 1, 3, 0, 2, 0, 0}));
    rev.Check("SetControlPoints");
    curve->SetWeightAt(1, 2.0);
    rev.Check("SetWeightAt");
    curve->SetWeights({1.0, 1.5, 1.0});
    rev.Check("SetWeights");
    curve->SetKnots({0, 0, 0, 1, 1, 1});
    rev.Check("SetKnots");
    curve->SetParameterRange({0.0, 0.5});
    rev.Check("SetParameterRange");
    curve->SetPeriodic(true);
    rev.Check("SetPeriodic");
    EXPECT_TRUE(curve->SetCurveType(
            i_ent::RationalBSplineCurveType::kCircularArc));
    rev.Check("SetCurveType");
    curve->SetData(2, {0, 0, 0, 1, 1, 1}, {},
                   MakeCPs({0, 0, 0, 1, 1, 1, 2, 0, 0}));
    rev.Check("SetData");
}

// RationalBSplineSurface: 全形状mutatorでバンプされる
TEST(GeometryRevisionTest, RationalBSplineSurface_AllShapeSettersBump) {
    auto surf = MakePlane();
    RevisionChecker rev(*surf);

    surf->SetControlPointAt(0, 0, {0, 0, 0.5});
    rev.Check("SetControlPointAt");
    surf->SetControlPoints(MakeCPs({0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0.5}));
    rev.Check("SetControlPoints");
    surf->SetWeightAt(0, 0, 2.0);
    rev.Check("SetWeightAt");
    surf->SetWeights(igesio::MatrixXd::Constant(2, 2, 1.5));
    rev.Check("SetWeights");
    surf->SetUKnots({0, 0, 1, 1});
    rev.Check("SetUKnots");
    surf->SetVKnots({0, 0, 1, 1});
    rev.Check("SetVKnots");
    surf->SetParameterRange({0.0, 0.5, 0.0, 0.5});
    rev.Check("SetParameterRange");
    surf->SetUPeriodic(true);
    rev.Check("SetUPeriodic");
    surf->SetVPeriodic(true);
    rev.Check("SetVPeriodic");
    EXPECT_TRUE(surf->SetSurfaceType(
            i_ent::RationalBSplineSurfaceType::kPlane));
    rev.Check("SetSurfaceType");
    surf->SetData({1, 1}, {0, 0, 1, 1}, {0, 0, 1, 1}, {},
                  {{{0, 0, 0}, {0, 1, 0}}, {{1, 0, 0}, {1, 1, 0}}});
    rev.Check("SetData");
}

// SurfaceOfRevolution: 全形状mutatorでバンプされる
TEST(GeometryRevisionTest, SurfaceOfRevolution_AllShapeSettersBump) {
    auto axis = i_ent::MakeLine({0, 0, 0}, {0, 0, 1});
    auto generatrix = i_ent::MakeLine({1, 0, 0}, {1, 0, 1});
    auto sor = i_ent::MakeSurfaceOfRevolution(axis, generatrix);
    RevisionChecker rev(*sor);

    sor->SetAxis(i_ent::MakeLine({0, 0, 0}, {0, 0, 2}));
    rev.Check("SetAxis");
    sor->SetGeneratrix(i_ent::MakeLine({2, 0, 0}, {2, 0, 1}));
    rev.Check("SetGeneratrix");
    sor->SetAngleRange(0.0, 1.5);
    rev.Check("SetAngleRange");
}

// RuledSurface: 全形状mutatorでバンプされる
TEST(GeometryRevisionTest, RuledSurface_AllShapeSettersBump) {
    auto rs = i_ent::MakeRuledSurface(
            i_ent::MakeLine({0, 0, 0}, {1, 0, 0}),
            i_ent::MakeLine({0, 1, 0}, {1, 1, 0}));
    RevisionChecker rev(*rs);

    rs->SetCurve1(i_ent::MakeLine({0, 0, 1}, {1, 0, 1}));
    rev.Check("SetCurve1");
    rs->SetCurve2(i_ent::MakeLine({0, 1, 1}, {1, 1, 1}));
    rev.Check("SetCurve2");
    rs->SetIsReversed(true);
    rev.Check("SetIsReversed");
    rs->SetIsDevelopable(true);
    rev.Check("SetIsDevelopable");
    rs->SetSurfaceForm(i_ent::RuledSurfaceForm::kEqualArcLength);
    rev.Check("SetSurfaceForm");
}

// TabulatedCylinder: 全形状mutatorでバンプされる
TEST(GeometryRevisionTest, TabulatedCylinder_AllShapeSettersBump) {
    auto tab = i_ent::MakeTabulatedCylinder(
            i_ent::MakeLine({0, 0, 0}, {1, 0, 0}), Vector3d{0, 0, 1});
    RevisionChecker rev(*tab);

    tab->SetDirectrix(i_ent::MakeLine({0, 0, 0}, {0, 1, 0}));
    rev.Check("SetDirectrix");
    tab->SetLocationVector({0, 1, 1});
    rev.Check("SetLocationVector");
    tab->SetDirection({0, 0, 1}, 2.0);
    rev.Check("SetDirection");
}

// TrimmedSurface: 全境界編集mutatorでバンプされる
TEST(GeometryRevisionTest, TrimmedSurface_AllBoundaryEditsBump) {
    auto ts = i_ent::MakeTrimmedSurface(MakePlane());
    RevisionChecker rev(*ts);

    // 境界が無い状態なら曲面の差し替えが可能
    auto surf = MakePlane();
    ts->SetSurface(surf);
    rev.Check("SetSurface");
    ts->SetOuterBoundary(MakeRectBoundaryOn(surf, 0.1, 0.9));
    rev.Check("SetOuterBoundary");
    ts->AddInnerBoundary(MakeRectBoundaryOn(surf, 0.3, 0.5));
    rev.Check("AddInnerBoundary");
    ts->SetInnerBoundaryAt(0, MakeRectBoundaryOn(surf, 0.6, 0.8));
    rev.Check("SetInnerBoundaryAt");
    ts->RemoveInnerBoundaryAt(0);
    rev.Check("RemoveInnerBoundaryAt");
    ts->AddInnerBoundary(MakeRectBoundaryOn(surf, 0.3, 0.5));
    rev.Check("AddInnerBoundary (2)");
    ts->ClearInnerBoundaries();
    rev.Check("ClearInnerBoundaries");
    ts->SetOuterBoundary(nullptr);  // N1=0: Dの境界を使用する経路
    rev.Check("SetOuterBoundary (nullptr)");
}

// CompositeCurve: 全編集mutatorでバンプされる (ReplaceCurves委譲経由も含む)
TEST(GeometryRevisionTest, CompositeCurve_AllEditsBump) {
    auto cc = i_ent::MakeCompositeCurve(
            {i_ent::MakeLine({0, 0, 0}, {1, 0, 0})});
    RevisionChecker rev(*cc);

    // 連続性制約のため、新曲線の始点は直前曲線の終点と一致させる
    EXPECT_TRUE(cc->AddCurve(i_ent::MakeLine({1, 0, 0}, {1, 1, 0})));
    rev.Check("AddCurve");
    cc->SetCurveAt(1, i_ent::MakeLine({1, 0, 0}, {2, 0, 0}));
    rev.Check("SetCurveAt (ReplaceCurves委譲)");
    cc->RemoveLastCurve();
    rev.Check("RemoveLastCurve (ReplaceCurves委譲)");
    cc->ReplaceCurves(0, 0, {i_ent::MakeLine({0, 0, 0}, {1, 1, 1})});
    rev.Check("ReplaceCurves");
    cc->ClearCurves();
    rev.Check("ClearCurves");
}

// CurveOnAParametricSurface: 全mutatorでバンプされる
TEST(GeometryRevisionTest, CurveOnSurface_AllSettersBump) {
    auto surf = MakePlane();
    auto cops = MakeRectBoundaryOn(surf, 0.2, 0.8);
    RevisionChecker rev(*cops);

    EXPECT_TRUE(cops->SetCreationType(
            i_ent::CurveCreationType::kProjection));
    rev.Check("SetCreationType");
    EXPECT_TRUE(cops->SetPreferredRepresentation(
            i_ent::PreferredRepresentation::kC));
    rev.Check("SetPreferredRepresentation");
    cops->SetSurface(MakePlane());
    rev.Check("SetSurface");
    // SetBaseCurveはprotectedのため、公開経路のSetCurves (内部でSetBaseCurveを
    // 呼ぶ) 経由でカバーする. C(t)は自動生成される
    auto loop2 = i_ent::MakeLinearPath(
            std::vector<Vector2d>{{0.3, 0.3}, {0.7, 0.3}, {0.7, 0.7},
                                  {0.3, 0.7}}, true);
    cops->SetCurves(loop2, nullptr);
    rev.Check("SetCurves");
}

// Point: 位置設定でバンプされる
TEST(GeometryRevisionTest, Point_SetDefinedPositionBumps) {
    auto pt = std::make_shared<i_ent::Point>(Vector3d{1, 2, 3});
    RevisionChecker rev(*pt);

    pt->SetDefinedPosition({4, 5, 6});
    rev.Check("SetDefinedPosition");
}



/**
 * TransformationMatrix (参照・種類)
 */

// SetReference (設定・解除) とSetMatrixTypeでバンプされる
TEST(GeometryRevisionTest, TransformationMatrix_SetReferenceAndTypeBump) {
    auto tm_a = i_ent::MakeTranslation({1, 0, 0});
    auto tm_b = i_ent::MakeTranslation({0, 1, 0});
    RevisionChecker rev(*tm_a);

    // 具象型ポインタはconst/非constオーバーロードが曖昧になるため明示キャスト
    EXPECT_TRUE(tm_a->SetReference(
            std::static_pointer_cast<i_ent::ITransformation>(tm_b)));
    rev.Check("SetReference");
    EXPECT_TRUE(tm_a->SetReference(
            std::shared_ptr<i_ent::ITransformation>(nullptr)));
    rev.Check("SetReference (解除)");
    EXPECT_TRUE(tm_a->SetMatrixType(i_ent::MatrixType::kCartesianOffset));
    rev.Check("SetMatrixType");
}



/**
 * 失敗経路で不変
 */

// 検証エラー (throw) ・無効値 (false) の経路ではバンプされない
TEST(GeometryRevisionTest, RationalBSplineCurve_NoBumpOnFailedSetters) {
    auto curve = MakeTestCurve();
    RevisionChecker rev(*curve);

    // サイズ不一致のノット (throw)
    EXPECT_THROW(curve->SetKnots({0.0, 1.0}), igesio::EntityValueError);
    rev.Unchanged("SetKnots (size mismatch)");
    // 非正の重み (throw)
    EXPECT_THROW(curve->SetWeightAt(0, -1.0), igesio::EntityValueError);
    rev.Unchanged("SetWeightAt (non-positive)");
    // 無効な曲線種類 (false)
    EXPECT_FALSE(curve->SetCurveType(
            static_cast<i_ent::RationalBSplineCurveType>(7)));
    rev.Unchanged("SetCurveType (invalid)");
}

// 循環参照で拒否されたSetReferenceではバンプされない
TEST(GeometryRevisionTest,
     TransformationMatrix_NoBumpWhenSetReferenceCreatesCycle) {
    auto tm_a = i_ent::MakeTranslation({1, 0, 0});
    auto tm_b = i_ent::MakeTranslation({0, 1, 0});
    // 具象型ポインタはconst/非constオーバーロードが曖昧になるため明示キャスト
    ASSERT_TRUE(tm_a->SetReference(
            std::static_pointer_cast<i_ent::ITransformation>(tm_b)));  // a -> b

    RevisionChecker rev(*tm_b);
    // b -> a は循環で拒否される
    EXPECT_FALSE(tm_b->SetReference(
            std::static_pointer_cast<i_ent::ITransformation>(tm_a)));
    rev.Unchanged("SetReference (cycle)");
}



/**
 * View転送
 */

// CurveView: 元曲線の形状編集がビューのリビジョンに反映される
TEST(GeometryRevisionTest, CurveView_ForwardsSourceRevision) {
    auto curve = MakeTestCurve();
    auto view = std::make_shared<i_ent::CurveView>(curve);
    const auto before = view->GeometryRevision();
    EXPECT_EQ(before, curve->GeometryRevision());

    curve->SetControlPointAt(0, {0, 0, 1});
    EXPECT_GT(view->GeometryRevision(), before);
    EXPECT_EQ(view->GeometryRevision(), curve->GeometryRevision());
}

// SurfaceView: 元曲面の形状編集がビューのリビジョンに反映される
TEST(GeometryRevisionTest, SurfaceView_ForwardsSourceRevision) {
    auto surf = MakePlane();
    auto view = std::make_shared<i_ent::SurfaceView>(surf);
    const auto before = view->GeometryRevision();
    EXPECT_EQ(before, surf->GeometryRevision());

    surf->SetControlPointAt(0, 0, {0, 0, 1});
    EXPECT_GT(view->GeometryRevision(), before);
    EXPECT_EQ(view->GeometryRevision(), surf->GeometryRevision());
}
