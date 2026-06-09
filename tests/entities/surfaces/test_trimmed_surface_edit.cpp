/**
 * @file tests/entities/surfaces/test_trimmed_surface_edit.cpp
 * @brief TrimmedSurface (Type 144) の構築・編集・整合性検査APIのテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [ファクトリ関数]
 *     MakeTrimmedSurface(surface, outer, inner_boundaries)
 *   [同一曲面整合性の強制 (144のPTSと境界142のPTSの一致)]
 *     型付きコンストラクタ / SetSurface / SetOuterBoundary / AddInnerBoundary
 *     (曲面参照が未解決の境界はチェック不能としてスキップ)
 *   [構成要素の操作]
 *     SetInnerBoundaryAt / RemoveInnerBoundaryAt (ポインタ返却) /
 *     ClearInnerBoundaries
 *   [ValidatePD]
 *     参照曲面不一致のkWarning報告 (読み込み経路に寛容)
 *
 * NOTE: 「異なる曲面」はMakePlane()を2回呼ぶことで作成する (同一幾何・別ID)。
 *       整合性検査がID比較であることの検証を兼ねる。
 *
 * TODO: 境界の幾何的妥当性 (閉性・外側境界内への内側境界の包含) は
 *       検査対象外のため未カバー (設計時の見送り判断)。
 * TODO: ドメイン判定・導関数等のIRestrictedSurface挙動は
 *       test_trimmed_surface.cppでカバー。
 */
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/surfaces/trimmed_surface.h"
#include "./surfaces_for_testing.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_test = igesio::tests;
using igesio::Vector2d;
using i_ent::CurveOnAParametricSurface;
using i_ent::SubordinateEntitySwitch;
using i_ent::TrimmedSurface;

/// @brief 基底NURBS平面 D=[0,1]² を作成する
/// @note 呼び出しごとに別IDの新規エンティティが生成されるため、
///       2回呼べば「同一幾何・別ID」の異なる曲面が得られる
std::shared_ptr<i_ent::ISurface> MakePlane() {
    return i_test::CreateRationalBSplineSurfaces()[0].surface;
}

/// @brief z=0平面上の軸平行矩形ループを作成する
std::shared_ptr<i_ent::ICurve> MakeUvRectLoop(
        const double umin, const double vmin,
        const double umax, const double vmax) {
    return i_ent::MakeLinearPath(
        std::vector<Vector2d>{
            {umin, vmin}, {umax, vmin}, {umax, vmax}, {umin, vmax}},
        true);
}

/// @brief UV曲線を境界 (Type142) に変換する (C(t)は自動生成)
std::shared_ptr<CurveOnAParametricSurface> MakeBoundary142(
        const std::shared_ptr<i_ent::ISurface>& surface,
        const std::shared_ptr<i_ent::ICurve>& uv_loop) {
    auto [c142, _] = i_ent::MakeCurveOnAParametricSurface(surface, uv_loop);
    return c142;
}

/// @brief 標準的な外側境界 (矩形 [0.2,0.8]²) を指定曲面上に作成する
std::shared_ptr<CurveOnAParametricSurface> MakeOuterOn(
        const std::shared_ptr<i_ent::ISurface>& surface) {
    return MakeBoundary142(surface, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
}

/// @brief 円形の穴 (内側境界) を指定曲面上に作成する
std::shared_ptr<CurveOnAParametricSurface> MakeHoleOn(
        const std::shared_ptr<i_ent::ISurface>& surface,
        const Vector2d& center = {0.5, 0.5}, const double radius = 0.1) {
    return MakeBoundary142(surface, i_ent::MakeCircle(center, radius));
}

/// @brief 曲面参照が未解決のType 142を作成する
/// @note 各参照はIDのみ保持しポインタ未設定。GetSurface()はReferenceErrorを
///       投げる状態であり、整合性チェックのスキップ検証に用いる
std::shared_ptr<CurveOnAParametricSurface> MakeUnresolvedBoundary142() {
    const auto surf = MakePlane();
    const auto loop_b = MakeUvRectLoop(0.3, 0.3, 0.7, 0.7);
    const auto loop_c = MakeUvRectLoop(0.3, 0.3, 0.7, 0.7);
    igesio::pointer2ID de2id;
    de2id.emplace(1u, surf->GetID());
    de2id.emplace(3u, loop_b->GetID());
    de2id.emplace(5u, loop_c->GetID());
    // PD: {CRTN, SPTR, BPTR, CPTR, PREF}
    const auto param = igesio::IGESParameterVector{0, 1, 3, 5, 0};
    return std::make_shared<CurveOnAParametricSurface>(
        i_ent::RawEntityDE::ByDefault(
            i_ent::EntityType::kCurveOnAParametricSurface),
        param, de2id);
}

/// @brief 読み込み経路 (PDコンストラクタ+参照解決) でTrimmedSurfaceを構築する
/// @param surface トリム対象の曲面
/// @param outer 外側境界 (nullptrの場合はN1=0)
/// @param inners 内側境界のリスト
/// @param resolve trueの場合、全参照を解決する。falseの場合はIDのみ保持
/// @note 読み込み経路は同一曲面整合性を検証しない (ValidatePDのkWarningのみ)
std::shared_ptr<TrimmedSurface> MakeTrimmedViaReadPath(
        const std::shared_ptr<i_ent::ISurface>& surface,
        const std::shared_ptr<CurveOnAParametricSurface>& outer,
        const std::vector<std::shared_ptr<CurveOnAParametricSurface>>& inners,
        const bool resolve = true) {
    igesio::pointer2ID de2id;
    igesio::IGESParameterVector pd;
    de2id.emplace(1u, surface->GetID());
    pd.push_back(1);                               // PTS
    pd.push_back(outer ? 1 : 0);                   // N1
    pd.push_back(static_cast<int>(inners.size()));  // N2
    if (outer) {
        de2id.emplace(3u, outer->GetID());
        pd.push_back(3);                           // PTO
    } else {
        pd.push_back(0);                           // PTO (ヌルポインタ)
    }
    unsigned int de = 5;
    for (const auto& inner : inners) {
        de2id.emplace(de, inner->GetID());
        pd.push_back(static_cast<int>(de));        // PTI(i)
        de += 2;
    }

    auto ts = std::make_shared<TrimmedSurface>(
        i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kTrimmedSurface),
        pd, de2id);
    if (resolve) {
        ts->SetUnresolvedReference(
                std::dynamic_pointer_cast<i_ent::EntityBase>(surface));
        if (outer) ts->SetUnresolvedReference(outer);
        for (const auto& inner : inners) ts->SetUnresolvedReference(inner);
    }
    return ts;
}

/// @brief 検証結果にkWarningが含まれるかを確認する
bool HasWarning(const igesio::ValidationResult& result) {
    for (const auto& e : result.errors) {
        if (e.severity == igesio::ValidationSeverity::kWarning) return true;
    }
    return false;
}

}  // namespace



/**
 * MakeTrimmedSurface のテスト
 */

// 曲面のみ: N1=0 (Dの境界を使用)、内側境界なし
TEST(MakeTrimmedSurfaceTest, SurfaceOnly_N1Zero) {
    const auto plane = MakePlane();

    const auto ts = i_ent::MakeTrimmedSurface(plane);

    EXPECT_TRUE(ts->IsOuterBoundaryOfD());
    EXPECT_EQ(ts->GetInnerBoundaryCount(), 0u);
    EXPECT_EQ(ts->GetSurface(), plane);
    // 曲面のSubordinateEntitySwitchはkPhysicallyDependentに設定される
    EXPECT_EQ(std::dynamic_pointer_cast<i_ent::EntityBase>(plane)
                      ->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

// 外側境界付き: N1=1
TEST(MakeTrimmedSurfaceTest, WithOuterBoundary) {
    const auto plane = MakePlane();
    const auto outer = MakeOuterOn(plane);

    const auto ts = i_ent::MakeTrimmedSurface(plane, outer);

    EXPECT_FALSE(ts->IsOuterBoundaryOfD());
    EXPECT_EQ(ts->GetOuterBoundary(), outer);
    EXPECT_EQ(outer->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

// 内側境界付き: 数・格納順・スイッチ設定
TEST(MakeTrimmedSurfaceTest, WithInnerBoundaries) {
    const auto plane = MakePlane();
    const auto outer = MakeOuterOn(plane);
    const auto hole1 = MakeHoleOn(plane, {0.35, 0.5});
    const auto hole2 = MakeHoleOn(plane, {0.65, 0.5});

    const auto ts = i_ent::MakeTrimmedSurface(plane, outer, {hole1, hole2});

    ASSERT_EQ(ts->GetInnerBoundaryCount(), 2u);
    EXPECT_EQ(ts->GetInnerBoundaryAt(0), hole1);
    EXPECT_EQ(ts->GetInnerBoundaryAt(1), hole2);
    EXPECT_EQ(hole1->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(hole2->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

// 縮退: N1=0 (outer=nullptr) + 内側境界あり (仕様上有効な組み合わせ)
TEST(MakeTrimmedSurfaceTest, OuterNullWithInners) {
    const auto plane = MakePlane();
    const auto hole = MakeHoleOn(plane);

    const auto ts = i_ent::MakeTrimmedSurface(plane, nullptr, {hole});

    EXPECT_TRUE(ts->IsOuterBoundaryOfD());
    EXPECT_EQ(ts->GetInnerBoundaryCount(), 1u);
}

// エラー: surfaceがnullptr
TEST(MakeTrimmedSurfaceTest, ThrowsInvalidArgumentWhenSurfaceNull) {
    EXPECT_THROW(i_ent::MakeTrimmedSurface(nullptr), std::invalid_argument);
}

// エラー: inner_boundariesにnullptrが含まれる
TEST(MakeTrimmedSurfaceTest, ThrowsInvalidArgumentWhenInnerContainsNull) {
    const auto plane = MakePlane();
    const auto hole = MakeHoleOn(plane);

    EXPECT_THROW(i_ent::MakeTrimmedSurface(plane, nullptr, {hole, nullptr}),
                 std::invalid_argument);
}

// エラー: outerが異なる曲面上に定義されている
TEST(MakeTrimmedSurfaceTest, ThrowsEntityValueErrorWhenOuterOnDifferentSurface) {
    const auto plane = MakePlane();
    const auto other_plane = MakePlane();  // 同一幾何・別ID
    const auto outer = MakeOuterOn(other_plane);

    EXPECT_THROW(i_ent::MakeTrimmedSurface(plane, outer),
                 igesio::EntityValueError);
}

// エラー: 内側境界が異なる曲面上に定義されている
TEST(MakeTrimmedSurfaceTest, ThrowsEntityValueErrorWhenInnerOnDifferentSurface) {
    const auto plane = MakePlane();
    const auto other_plane = MakePlane();
    const auto good_hole = MakeHoleOn(plane, {0.35, 0.5});
    const auto bad_hole = MakeHoleOn(other_plane, {0.65, 0.5});

    EXPECT_THROW(
        i_ent::MakeTrimmedSurface(plane, nullptr, {good_hole, bad_hole}),
        igesio::EntityValueError);
}

// 失敗時は入力エンティティに副作用 (スイッチ変更) を残さない (事前検証の確認)
TEST(MakeTrimmedSurfaceTest, NoSideEffectOnThrow) {
    const auto plane = MakePlane();
    const auto other_plane = MakePlane();
    const auto good_hole = MakeHoleOn(plane, {0.35, 0.5});
    const auto bad_hole = MakeHoleOn(other_plane, {0.65, 0.5});
    const auto sw_plane = std::dynamic_pointer_cast<i_ent::EntityBase>(plane)
                                  ->GetSubordinateEntitySwitch();
    const auto sw_good = good_hole->GetSubordinateEntitySwitch();

    EXPECT_THROW(
        i_ent::MakeTrimmedSurface(plane, nullptr, {good_hole, bad_hole}),
        igesio::EntityValueError);

    EXPECT_EQ(std::dynamic_pointer_cast<i_ent::EntityBase>(plane)
                      ->GetSubordinateEntitySwitch(), sw_plane);
    EXPECT_EQ(good_hole->GetSubordinateEntitySwitch(), sw_good);
}



/**
 * 同一曲面整合性 (編集API) のテスト
 */

// エラー: 異なる曲面上の内側境界の追加は拒否され、数も変わらない
TEST(TrimmedSurfaceSurfaceConsistencyTest,
     AddInnerBoundary_ThrowsEntityValueErrorWhenDifferentSurface) {
    const auto plane = MakePlane();
    const auto ts = i_ent::MakeTrimmedSurface(plane);
    const auto bad_hole = MakeHoleOn(MakePlane());

    EXPECT_THROW(ts->AddInnerBoundary(bad_hole), igesio::EntityValueError);
    EXPECT_EQ(ts->GetInnerBoundaryCount(), 0u);
}

// 曲面参照が未解決の境界はチェック不能としてスキップし、受理される
TEST(TrimmedSurfaceSurfaceConsistencyTest,
     AddInnerBoundary_UnresolvedSurfaceRef_NoThrow) {
    const auto ts = i_ent::MakeTrimmedSurface(MakePlane());
    const auto unresolved = MakeUnresolvedBoundary142();

    EXPECT_NO_THROW(ts->AddInnerBoundary(unresolved));
    EXPECT_EQ(ts->GetInnerBoundaryCount(), 1u);
}

// エラー: 異なる曲面上の外側境界の設定は拒否され、状態も変わらない
TEST(TrimmedSurfaceSurfaceConsistencyTest,
     SetOuterBoundary_ThrowsEntityValueErrorWhenDifferentSurface) {
    const auto plane = MakePlane();
    const auto ts = i_ent::MakeTrimmedSurface(plane);
    const auto bad_outer = MakeOuterOn(MakePlane());

    EXPECT_THROW(ts->SetOuterBoundary(bad_outer), igesio::EntityValueError);
    // N1=0のまま変更されていない
    EXPECT_TRUE(ts->IsOuterBoundaryOfD());
    EXPECT_EQ(ts->GetOuterBoundary(), nullptr);
}

// エラー: 型付きコンストラクタ経由でも外側境界の曲面不一致は拒否される
TEST(TrimmedSurfaceSurfaceConsistencyTest,
     TypedCtor_ThrowsEntityValueErrorWhenOuterOnDifferentSurface) {
    const auto plane = MakePlane();
    const auto bad_outer = MakeOuterOn(MakePlane());

    EXPECT_THROW(TrimmedSurface(plane, bad_outer), igesio::EntityValueError);
}

// エラー: 境界を保持したままの曲面差し替えは拒否され、曲面は不変
TEST(TrimmedSurfaceSurfaceConsistencyTest,
     SetSurface_ThrowsEntityValueErrorWhenBoundariesMismatch) {
    const auto plane = MakePlane();
    const auto outer = MakeOuterOn(plane);
    const auto hole = MakeHoleOn(plane);
    const auto ts = i_ent::MakeTrimmedSurface(plane, outer, {hole});
    const auto other_plane = MakePlane();

    EXPECT_THROW(ts->SetSurface(other_plane), igesio::EntityValueError);
    EXPECT_EQ(ts->GetSurface(), plane);
}

// 境界をクリアした後は曲面を差し替えられる (ドキュメント化した運用手順)
TEST(TrimmedSurfaceSurfaceConsistencyTest,
     SetSurface_AfterClearingBoundaries_Succeeds) {
    const auto plane = MakePlane();
    const auto outer = MakeOuterOn(plane);
    const auto hole = MakeHoleOn(plane);
    const auto ts = i_ent::MakeTrimmedSurface(plane, outer, {hole});
    const auto other_plane = MakePlane();

    ts->ClearInnerBoundaries();
    ts->SetOuterBoundary(nullptr);  // N1=0へ

    EXPECT_NO_THROW(ts->SetSurface(other_plane));
    EXPECT_EQ(ts->GetSurface(), other_plane);
}

// 読み込み経路の境界未解決状態ではSetSurfaceのチェックはスキップされる
// (型付きコンストラクタ内部のSetSurface→SetOuterBoundary順序の前提の固定)
TEST(TrimmedSurfaceSurfaceConsistencyTest,
     SetSurface_UnresolvedBoundaries_NoThrow) {
    const auto plane = MakePlane();
    const auto outer = MakeOuterOn(plane);
    const auto hole = MakeHoleOn(plane);
    // 参照解決を行わない (境界はIDのみ保持)
    const auto ts = MakeTrimmedViaReadPath(plane, outer, {hole},
                                           /*resolve=*/false);
    const auto other_plane = MakePlane();

    EXPECT_NO_THROW(ts->SetSurface(other_plane));
    EXPECT_EQ(ts->GetSurface(), other_plane);
}



/**
 * SetInnerBoundaryAt のテスト
 */

// 置換: 旧境界を返し、数は不変、新境界が当該indexに入る
TEST(TrimmedSurfaceSetInnerBoundaryAtTest, ReplacesAndReturnsOld) {
    const auto plane = MakePlane();
    const auto hole1 = MakeHoleOn(plane, {0.35, 0.5});
    const auto hole2 = MakeHoleOn(plane, {0.65, 0.5});
    const auto ts = i_ent::MakeTrimmedSurface(plane, nullptr, {hole1});

    const auto old = ts->SetInnerBoundaryAt(0, hole2);

    EXPECT_EQ(old, hole1);
    ASSERT_EQ(ts->GetInnerBoundaryCount(), 1u);
    EXPECT_EQ(ts->GetInnerBoundaryAt(0), hole2);
    EXPECT_EQ(hole2->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

// エラー: nullptr
TEST(TrimmedSurfaceSetInnerBoundaryAtTest, ThrowsInvalidArgumentWhenNull) {
    const auto plane = MakePlane();
    const auto ts = i_ent::MakeTrimmedSurface(plane, nullptr,
                                              {MakeHoleOn(plane)});

    EXPECT_THROW(ts->SetInnerBoundaryAt(0, nullptr), std::invalid_argument);
}

// エラー: indexが範囲外 (境界両側: count-1は有効, countで例外)
TEST(TrimmedSurfaceSetInnerBoundaryAtTest, ThrowsOutOfRangeWhenIndexTooLarge) {
    const auto plane = MakePlane();
    const auto ts = i_ent::MakeTrimmedSurface(plane, nullptr,
                                              {MakeHoleOn(plane, {0.35, 0.5})});
    const auto hole = MakeHoleOn(plane, {0.65, 0.5});

    EXPECT_THROW(ts->SetInnerBoundaryAt(1, hole), std::out_of_range);
    EXPECT_NO_THROW(ts->SetInnerBoundaryAt(0, hole));
}

// エラー: 異なる曲面上の境界への置換は拒否され、旧境界が残る
TEST(TrimmedSurfaceSetInnerBoundaryAtTest,
     ThrowsEntityValueErrorWhenDifferentSurface) {
    const auto plane = MakePlane();
    const auto hole = MakeHoleOn(plane);
    const auto ts = i_ent::MakeTrimmedSurface(plane, nullptr, {hole});
    const auto bad_hole = MakeHoleOn(MakePlane());

    EXPECT_THROW(ts->SetInnerBoundaryAt(0, bad_hole),
                 igesio::EntityValueError);
    EXPECT_EQ(ts->GetInnerBoundaryAt(0), hole);
}



/**
 * RemoveInnerBoundaryAt / ClearInnerBoundaries のテスト
 */

// 削除: 取り外した境界を返し、数が減る
TEST(TrimmedSurfaceRemoveClearTest, Remove_ReturnsRemovedBoundary) {
    const auto plane = MakePlane();
    const auto hole1 = MakeHoleOn(plane, {0.35, 0.5});
    const auto hole2 = MakeHoleOn(plane, {0.65, 0.5});
    const auto ts = i_ent::MakeTrimmedSurface(plane, nullptr, {hole1, hole2});

    const auto removed = ts->RemoveInnerBoundaryAt(0);

    EXPECT_EQ(removed, hole1);
    ASSERT_EQ(ts->GetInnerBoundaryCount(), 1u);
    EXPECT_EQ(ts->GetInnerBoundaryAt(0), hole2);
}

// 取り外した境界のSubordinateEntitySwitchは変更されない (既存方針の固定)
TEST(TrimmedSurfaceRemoveClearTest, Remove_KeepsSubordinateSwitch) {
    const auto plane = MakePlane();
    const auto hole = MakeHoleOn(plane);
    const auto ts = i_ent::MakeTrimmedSurface(plane, nullptr, {hole});

    ts->RemoveInnerBoundaryAt(0);

    EXPECT_EQ(hole->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

// エラー: indexが範囲外 (境界両側: count-1は有効, countで例外)
TEST(TrimmedSurfaceRemoveClearTest, Remove_ThrowsOutOfRangeWhenIndexTooLarge) {
    const auto plane = MakePlane();
    const auto ts = i_ent::MakeTrimmedSurface(plane, nullptr,
                                              {MakeHoleOn(plane)});

    EXPECT_THROW(ts->RemoveInnerBoundaryAt(1), std::out_of_range);
    EXPECT_NO_THROW(ts->RemoveInnerBoundaryAt(0));
    // 空になった後はindex 0も範囲外
    EXPECT_THROW(ts->RemoveInnerBoundaryAt(0), std::out_of_range);
}

// 全削除: 数0になり、空に対する再実行も安全
TEST(TrimmedSurfaceRemoveClearTest, Clear_EmptiesAndIsIdempotent) {
    const auto plane = MakePlane();
    const auto ts = i_ent::MakeTrimmedSurface(
        plane, nullptr, {MakeHoleOn(plane, {0.35, 0.5}),
                         MakeHoleOn(plane, {0.65, 0.5})});

    ts->ClearInnerBoundaries();

    EXPECT_EQ(ts->GetInnerBoundaryCount(), 0u);
    EXPECT_NO_THROW(ts->ClearInnerBoundaries());  // 再実行も安全
}



/**
 * ValidatePD の参照曲面不一致kWarningのテスト (読み込み経路)
 */

// 読み込み経路: 外側境界が異なる曲面を参照 → is_validのままkWarningを報告
TEST(TrimmedSurfaceValidateTest, Validate_WarnsWhenOuterOnDifferentSurface) {
    const auto plane = MakePlane();
    const auto outer_on_other = MakeOuterOn(MakePlane());
    const auto ts = MakeTrimmedViaReadPath(plane, outer_on_other, {});

    const auto result = ts->ValidatePD();

    EXPECT_TRUE(result.is_valid);  // 警告は描画/使用をブロックしない
    EXPECT_TRUE(HasWarning(result));
}

// 読み込み経路: 内側境界が異なる曲面を参照 → is_validのままkWarningを報告
TEST(TrimmedSurfaceValidateTest, Validate_WarnsWhenInnerOnDifferentSurface) {
    const auto plane = MakePlane();
    const auto hole_on_other = MakeHoleOn(MakePlane());
    const auto ts = MakeTrimmedViaReadPath(plane, nullptr, {hole_on_other});

    const auto result = ts->ValidatePD();

    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(HasWarning(result));
}

// 読み込み経路: 参照曲面が整合していれば警告なし
TEST(TrimmedSurfaceValidateTest, Validate_NoWarningWhenSurfacesMatch) {
    const auto plane = MakePlane();
    const auto outer = MakeOuterOn(plane);
    const auto hole = MakeHoleOn(plane);
    const auto ts = MakeTrimmedViaReadPath(plane, outer, {hole});

    const auto result = ts->ValidatePD();

    EXPECT_TRUE(result.is_valid) << result.Message();
    EXPECT_FALSE(HasWarning(result));
}



/**
 * キャッシュ無効化 (新API分) のテスト
 * NOTE: 既存のSet/Add/Remove系のキャッシュ無効化はtest_trimmed_surface.cppで
 *       カバー済み。ここではSetInnerBoundaryAt/ClearInnerBoundariesのみ確認
 */

// 穴の置換後、ドメイン判定が新しい穴の位置を反映する
TEST(TrimmedSurfaceCacheTest, SetInnerBoundaryAt_UpdatesDomainJudgment) {
    const auto plane = MakePlane();
    const auto hole1 = MakeHoleOn(plane, {0.35, 0.5});
    const auto hole2 = MakeHoleOn(plane, {0.65, 0.5});
    const auto ts = i_ent::MakeTrimmedSurface(
        plane, MakeOuterOn(plane), {hole1});

    // キャッシュを構築させてから穴を置換する
    EXPECT_FALSE(ts->IsInDomain(0.35, 0.5));  // 穴1の内部
    EXPECT_TRUE(ts->IsInDomain(0.65, 0.5));

    ts->SetInnerBoundaryAt(0, hole2);

    EXPECT_TRUE(ts->IsInDomain(0.35, 0.5));   // 穴1は除去された
    EXPECT_FALSE(ts->IsInDomain(0.65, 0.5));  // 穴2の内部
}

// 全削除後、旧穴の位置がドメイン内に戻る
TEST(TrimmedSurfaceCacheTest, ClearInnerBoundaries_UpdatesDomainJudgment) {
    const auto plane = MakePlane();
    const auto ts = i_ent::MakeTrimmedSurface(
        plane, MakeOuterOn(plane), {MakeHoleOn(plane, {0.5, 0.5})});

    // キャッシュを構築させてからクリアする
    EXPECT_FALSE(ts->IsInDomain(0.5, 0.5));  // 穴の内部

    ts->ClearInnerBoundaries();

    EXPECT_TRUE(ts->IsInDomain(0.5, 0.5));
}
