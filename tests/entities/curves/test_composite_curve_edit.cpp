/**
 * @file tests/entities/curves/test_composite_curve_edit.cpp
 * @brief CompositeCurve (Type 102) の構築・編集・パラメータ写像APIのテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [ファクトリ関数]
 *     MakeCompositeCurve(curves)
 *   [構成要素の操作]
 *     AddCurve(curve)            (連続性強制の追加分; 幾何挙動は既存ファイル)
 *     ReplaceCurves(first, last, new_curves)
 *     SetCurveAt(index, curve)
 *     RemoveFirstCurve(), RemoveLastCurve(), ClearCurves()
 *   [パラメータ写像・接合診断]
 *     GetCurveBreakParameters()
 *     TryGetCurveIndexAtParameter(t)
 *     TryGetGlobalParameter(index, t_local)
 *     GetJunctionGaps()
 *
 * 連続性許容差: max(kGeometryTolerance, 1e-5×scale)。本ファイルの境界精度テスト
 * はscale≈100 (許容差≈1e-3) の代表寸法で行う。
 *
 * TODO:
 *   - 無限長曲線 (Line Form 1/2等) との接合チェックスキップ・パラメータ長0扱い
 *     は未検証 (無限長エンティティの構築手段が未整備のため、未解決参照による
 *     スキップ検証のみ実施)
 *   - 変換行列を持つ構成曲線での連続性判定 (モデル空間端点) は未検証
 *   - 許容差下限 (kGeometryTolerance床、scale≈0近傍) の境界は未検証
 *     (代表スケールの境界のみ検証する方針)
 */
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::CompositeCurve;
using i_ent::ICurve;
using i_ent::Line;
using i_ent::LinearPath;
using i_ent::RationalBSplineCurve;
using i_ent::SubordinateEntitySwitch;
constexpr double kPi = igesio::kPi;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief 連続性許容差 (scale≈100で≈1e-3) を確実に下回る接合隙間
constexpr double kGapBelowTol = 0.9e-3;
/// @brief 連続性許容差 (scale≈100で≈1e-3) を確実に超える接合隙間
/// @note 隙間を加えた点のノルムも≈100のため許容差は≈1.00001e-3に留まり、
///       1.1e-3は確実に超過する
constexpr double kGapAboveTol = 1.1e-3;
/// @brief 位置・スケールに依存せずあらゆる連続性許容差を超える大きな隙間
constexpr double kHugeGap = 1.0;



/**
 * テスト用インスタンスのファクトリ関数
 */

/// @brief 3直線の連続チェーンと、その構成曲線へのハンドル
struct Chain3 {
    /// @brief 構成曲線 (それぞれパラメータ範囲[0,1])
    std::shared_ptr<Line> l0, l1, l2;
    /// @brief 複合曲線 (全体範囲[0,3], 接合点 t=1, t=2)
    std::shared_ptr<CompositeCurve> cc;
};

/// @brief scale≈100の3直線連続チェーンを作成する
/// L0: (0,0,0)→(100,0,0), L1: (100,0,0)→(100,100,0), L2: (100,100,0)→(0,100,0)
/// 連続性許容差は接合点のノルム≈100〜141に対し≈1e-3〜1.4e-3
Chain3 MakeChain3Lines() {
    Chain3 c;
    c.l0 = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});
    c.l1 = i_ent::MakeLine(
        Vector3d{100.0, 0.0, 0.0}, Vector3d{100.0, 100.0, 0.0});
    c.l2 = i_ent::MakeLine(
        Vector3d{100.0, 100.0, 0.0}, Vector3d{0.0, 100.0, 0.0});
    c.cc = i_ent::MakeCompositeCurve({c.l0, c.l1, c.l2});
    return c;
}

/// @brief Chain3のL1区間 ((100,0,0)→(100,100,0)) を置き換える2直線ブリッジ
/// (100,0,0)→(150,50,0)→(100,100,0); 両端はL0終点・L2始点と一致
std::vector<std::shared_ptr<ICurve>> MakeBridge2Lines() {
    return {i_ent::MakeLine(
                Vector3d{100.0, 0.0, 0.0}, Vector3d{150.0, 50.0, 0.0}),
            i_ent::MakeLine(
                Vector3d{150.0, 50.0, 0.0}, Vector3d{100.0, 100.0, 0.0})};
}

/// @brief 始点=終点=(100,0,0)の閉ループLinearPath
/// (100,0)→(120,0)→(120,20)→(100,0) 非ループフラグだが頂点列として閉じている
std::shared_ptr<LinearPath> MakeClosedLoopPath() {
    return i_ent::MakeLinearPath(
        std::vector<Vector2d>{{100.0, 0.0}, {120.0, 0.0},
                              {120.0, 20.0}, {100.0, 0.0}}, false);
}

/// @brief 参照未解決のCompositeCurve (2曲線分のIDのみ保持しポインタ未設定)
/// @note 読み込み経路 (PDコンストラクタ) で構築し、参照解決を行わない。
///       接合チェック不能時のスキップ・nullptr返却・パラメータ長0扱いの検証用
std::shared_ptr<CompositeCurve> MakeUnresolved2Curves() {
    const auto l0 = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});
    const auto l1 = i_ent::MakeLine(
        Vector3d{100.0, 0.0, 0.0}, Vector3d{100.0, 100.0, 0.0});
    igesio::pointer2ID de2id;
    de2id.emplace(1u, l0->GetID());
    de2id.emplace(3u, l1->GetID());
    const auto param = igesio::IGESParameterVector{2, 1, 3};
    return std::make_shared<CompositeCurve>(
        i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kCompositeCurve),
        param, de2id);
}

/// @brief 四分円弧 + 2頂点LinearPathからなるCompositeCurve
/// Arc: 中心(0,0), r=1, (1,0)→(0,1) CCW, range=[0,π/2]
/// Path: (0,1,0)→(0,2,0) 非ループ, range=[0,1]
/// 全体範囲 [0, π/2+1], 接合点 t=π/2
std::shared_ptr<CompositeCurve> MakeArcPathQuarter() {
    return i_ent::MakeCompositeCurve({
        i_ent::MakeCircularArc(
            Vector2d{0.0, 0.0},   // 中心
            Vector2d{1.0, 0.0},   // 始点 (角度 0)
            Vector2d{0.0, 1.0}),  // 終点 (角度 π/2, CCW)
        i_ent::MakeLinearPath(
            std::vector<Vector2d>{{0.0, 1.0}, {0.0, 2.0}}, false)});
}

/// @brief 2頂点LinearPath + degree-1 NURBS (パラメータ範囲[2,5]) のCompositeCurve
/// Path: (0,0,0)→(1,0,0) 非ループ, range=[0,1]
/// NURBS: degree=1, K=2 (3制御点), range=[2,5]
///        P0=(1,0,0), P1=(1,1,0), P2=(1,2,0)
/// 全体範囲 [0,4], 接合点 t=1.0
/// @note local_start=2.0の非ゼロ開始パラメータを持つサブカーブの
///       写像計算を検証するためのフィクスチャ
std::shared_ptr<CompositeCurve> MakePathNurbsNonZeroRange() {
    // degree-1 NURBS: K=2, M=1, ノット=[2,2,3.5,5,5], range=[2,5]
    // ノットベクトルサイズ = K+M+2 = 5, 重みサイズ = K+1 = 3
    const auto param = igesio::IGESParameterVector{
        2,                          // K (制御点数 - 1)
        1,                          // M (次数)
        false, false, true, false,  // PROP1-4 (非周期, 非閉, 多項式, 非平面外)
        2.0, 2.0, 3.5, 5.0, 5.0,    // ノットベクトル (K+M+2=5 個)
        1.0, 1.0, 1.0,              // 重み (K+1=3 個)
        1.0, 0.0, 0.0,              // P0 = (1,0,0)
        1.0, 1.0, 0.0,              // P1 = (1,1,0)
        1.0, 2.0, 0.0,              // P2 = (1,2,0)
        2.0, 5.0,                   // V(0)=2, V(1)=5
        0.0, 0.0, 1.0               // 法線ベクトル
    };
    return i_ent::MakeCompositeCurve({
        i_ent::MakeLinearPath(
            std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}}, false),
        std::make_shared<RationalBSplineCurve>(param)});
}

}  // namespace



/**
 * MakeCompositeCurve のテスト
 */

// 連続な3曲線: 数・格納順・パラメータ範囲・子のスイッチ設定を確認
TEST(MakeCompositeCurveTest, ThreeContiguousCurves) {
    auto c = MakeChain3Lines();

    ASSERT_EQ(c.cc->GetCurveCount(), 3u);
    EXPECT_EQ(c.cc->GetCurveAt(0), c.l0);
    EXPECT_EQ(c.cc->GetCurveAt(1), c.l1);
    EXPECT_EQ(c.cc->GetCurveAt(2), c.l2);

    // パラメータ範囲は各構成曲線のパラメータ長の合計
    const auto range = c.cc->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 3.0, kTol);

    // 各曲線のSubordinateEntitySwitchはkPhysicallyDependentに設定される
    EXPECT_EQ(c.l0->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(c.l1->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(c.l2->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

// 境界: 1曲線のみ (接合なし→連続性チェックなし)
TEST(MakeCompositeCurveTest, SingleCurve) {
    const auto line = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});

    const auto cc = i_ent::MakeCompositeCurve({line});

    ASSERT_EQ(cc->GetCurveCount(), 1u);
    EXPECT_NEAR(cc->GetParameterRange()[1], 1.0, kTol);
}

// 波括弧リスト記法での呼び出しが通る
TEST(MakeCompositeCurveTest, AcceptsBraceInitList) {
    const auto cc = i_ent::MakeCompositeCurve({
        i_ent::MakeLine(
            Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0}),
        i_ent::MakeLine(
            Vector3d{100.0, 0.0, 0.0}, Vector3d{100.0, 100.0, 0.0})});

    EXPECT_EQ(cc->GetCurveCount(), 2u);
}

// エラー: 空リスト
TEST(MakeCompositeCurveTest, ThrowsInvalidArgumentWhenEmpty) {
    EXPECT_THROW(i_ent::MakeCompositeCurve({}), std::invalid_argument);
}

// エラー: nullptr要素
TEST(MakeCompositeCurveTest, ThrowsInvalidArgumentWhenContainsNull) {
    const auto line = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});

    EXPECT_THROW(i_ent::MakeCompositeCurve({line, nullptr}),
                 std::invalid_argument);
}

// エラー: 接合隙間が許容差 (≈1e-3 @ scale100) を超える
TEST(MakeCompositeCurveTest, ThrowsEntityValueErrorWhenDiscontinuous) {
    const auto l0 = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});
    const auto l1 = i_ent::MakeLine(
        Vector3d{100.0 + kGapAboveTol, 0.0, 0.0}, Vector3d{200.0, 0.0, 0.0});

    EXPECT_THROW(i_ent::MakeCompositeCurve({l0, l1}),
                 igesio::EntityValueError);
}

// 境界精度: 許容差直下の隙間は許容される
TEST(MakeCompositeCurveTest, GapJustBelowTolerance_NoThrow) {
    const auto l0 = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});
    const auto l1 = i_ent::MakeLine(
        Vector3d{100.0 + kGapBelowTol, 0.0, 0.0}, Vector3d{200.0, 0.0, 0.0});

    EXPECT_NO_THROW(i_ent::MakeCompositeCurve({l0, l1}));
}

// 失敗時は入力曲線に副作用 (スイッチ変更) を残さない (事前検証の確認)
TEST(MakeCompositeCurveTest, NoSideEffectOnThrow) {
    const auto l0 = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});
    const auto l1 = i_ent::MakeLine(
        Vector3d{100.0 + kHugeGap, 0.0, 0.0}, Vector3d{200.0, 0.0, 0.0});
    const auto sw0 = l0->GetSubordinateEntitySwitch();
    const auto sw1 = l1->GetSubordinateEntitySwitch();

    EXPECT_THROW(i_ent::MakeCompositeCurve({l0, l1}),
                 igesio::EntityValueError);

    EXPECT_EQ(l0->GetSubordinateEntitySwitch(), sw0);
    EXPECT_EQ(l1->GetSubordinateEntitySwitch(), sw1);
}



/**
 * AddCurve の連続性強制のテスト
 * (幾何挙動・スイッチ設定は既存のtest_composite_curve.cppでカバー)
 */

// 連続な追加は成功する (1本目は接合がないため常にチェックなし)
TEST(CompositeCurveAddCurveTest, Contiguous_ReturnsTrue) {
    const auto cc = std::make_shared<CompositeCurve>();

    EXPECT_TRUE(cc->AddCurve(i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0})));
    EXPECT_TRUE(cc->AddCurve(i_ent::MakeLine(
        Vector3d{100.0, 0.0, 0.0}, Vector3d{100.0, 100.0, 0.0})));
    EXPECT_EQ(cc->GetCurveCount(), 2u);
}

// nullptrはfalseを返す (既存挙動の維持; 例外は投げない)
TEST(CompositeCurveAddCurveTest, Null_ReturnsFalse) {
    auto c = MakeChain3Lines();

    EXPECT_FALSE(c.cc->AddCurve(nullptr));
    EXPECT_EQ(c.cc->GetCurveCount(), 3u);
}

// エラー: 接合隙間が許容差を超える
TEST(CompositeCurveAddCurveTest, ThrowsEntityValueErrorWhenGapExceedsTolerance) {
    const auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0}));

    EXPECT_THROW(cc->AddCurve(i_ent::MakeLine(
        Vector3d{100.0 + kGapAboveTol, 0.0, 0.0}, Vector3d{200.0, 0.0, 0.0})),
        igesio::EntityValueError);
    // 失敗時は追加されない
    EXPECT_EQ(cc->GetCurveCount(), 1u);
}

// 境界精度: 許容差直下の隙間は許容される
TEST(CompositeCurveAddCurveTest, GapJustBelowTolerance_NoThrow) {
    const auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0}));

    EXPECT_NO_THROW(cc->AddCurve(i_ent::MakeLine(
        Vector3d{100.0 + kGapBelowTol, 0.0, 0.0}, Vector3d{200.0, 0.0, 0.0})));
    EXPECT_EQ(cc->GetCurveCount(), 2u);
}

// 直前の曲線が未解決参照 (終点取得不能) の場合はチェック不能として許容
TEST(CompositeCurveAddCurveTest, AfterUnresolvedCurve_NoThrow) {
    const auto cc = MakeUnresolved2Curves();

    // どこに置いてもチェックされない (直前の終点が取得できないため)
    EXPECT_NO_THROW(cc->AddCurve(i_ent::MakeLine(
        Vector3d{500.0, 0.0, 0.0}, Vector3d{600.0, 0.0, 0.0})));
    EXPECT_EQ(cc->GetCurveCount(), 3u);
}



/**
 * ReplaceCurves のテスト
 */

// 中間1要素を端点一致の円弧1本で置換
TEST(CompositeCurveReplaceCurvesTest, MiddleWithOne_EndpointsMatched) {
    auto c = MakeChain3Lines();
    // L1と同じ端点を持つ半円弧: 中心(100,50), (100,0)→(100,100) CCW
    const auto arc = i_ent::MakeCircularArc(
        Vector2d{100.0, 50.0}, Vector2d{100.0, 0.0}, Vector2d{100.0, 100.0});

    const auto removed = c.cc->ReplaceCurves(1, 1, {arc});

    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0], c.l1);
    ASSERT_EQ(c.cc->GetCurveCount(), 3u);
    EXPECT_EQ(c.cc->GetCurveAt(0), c.l0);
    EXPECT_EQ(c.cc->GetCurveAt(1), arc);
    EXPECT_EQ(c.cc->GetCurveAt(2), c.l2);
    // パラメータ範囲の再計算: 1 + π (半円弧) + 1
    EXPECT_NEAR(c.cc->GetParameterRange()[1], 2.0 + kPi, kTol);
}

// 中間1要素を2要素で置換 (置換前後で個数が異なるケース)
TEST(CompositeCurveReplaceCurvesTest, MiddleWithTwo) {
    auto c = MakeChain3Lines();
    const auto bridge = MakeBridge2Lines();

    const auto removed = c.cc->ReplaceCurves(1, 1, bridge);

    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0], c.l1);
    ASSERT_EQ(c.cc->GetCurveCount(), 4u);
    EXPECT_EQ(c.cc->GetCurveAt(0), c.l0);
    EXPECT_EQ(c.cc->GetCurveAt(1), bridge[0]);
    EXPECT_EQ(c.cc->GetCurveAt(2), bridge[1]);
    EXPECT_EQ(c.cc->GetCurveAt(3), c.l2);
    EXPECT_NEAR(c.cc->GetParameterRange()[1], 4.0, kTol);
}

// 範囲[0,1]を1要素で置換 (縮約)
TEST(CompositeCurveReplaceCurvesTest, RangeWithOne) {
    auto c = MakeChain3Lines();
    // L0始点(0,0,0)とL2始点(100,100,0)を直接結ぶ対角線
    const auto diag = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 100.0, 0.0});

    const auto removed = c.cc->ReplaceCurves(0, 1, {diag});

    ASSERT_EQ(removed.size(), 2u);
    EXPECT_EQ(removed[0], c.l0);
    EXPECT_EQ(removed[1], c.l1);
    ASSERT_EQ(c.cc->GetCurveCount(), 2u);
    EXPECT_EQ(c.cc->GetCurveAt(0), diag);
    EXPECT_EQ(c.cc->GetCurveAt(1), c.l2);
}

// 空のnew_curves: 先頭削除 (左隣接なし→境界チェックなし)
TEST(CompositeCurveReplaceCurvesTest, EmptyNewCurves_RemovesHead) {
    auto c = MakeChain3Lines();

    const auto removed = c.cc->ReplaceCurves(0, 0, {});

    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0], c.l0);
    ASSERT_EQ(c.cc->GetCurveCount(), 2u);
    EXPECT_EQ(c.cc->GetCurveAt(0), c.l1);
}

// 縮退: 始点=終点の閉ループ曲線の中間削除は前後が接続するため成功する
TEST(CompositeCurveReplaceCurvesTest, EmptyNewCurves_MiddleLoopCurve) {
    const auto l0 = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});
    const auto loop = MakeClosedLoopPath();  // 始点=終点=(100,0,0)
    const auto l2 = i_ent::MakeLine(
        Vector3d{100.0, 0.0, 0.0}, Vector3d{100.0, 100.0, 0.0});
    const auto cc = i_ent::MakeCompositeCurve({l0, loop, l2});

    const auto removed = cc->ReplaceCurves(1, 1, {});

    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0], loop);
    EXPECT_EQ(cc->GetCurveCount(), 2u);
}

// 境界: 全範囲の削除で空になる (接合チェック対象なし)
TEST(CompositeCurveReplaceCurvesTest, FullRange_Empties) {
    auto c = MakeChain3Lines();

    const auto removed = c.cc->ReplaceCurves(0, 2, {});

    EXPECT_EQ(removed.size(), 3u);
    EXPECT_EQ(c.cc->GetCurveCount(), 0u);
}

// エラー: first > last
TEST(CompositeCurveReplaceCurvesTest, ThrowsInvalidArgumentWhenFirstExceedsLast) {
    auto c = MakeChain3Lines();

    EXPECT_THROW(c.cc->ReplaceCurves(2, 1, {}), std::invalid_argument);
}

// エラー: lastが範囲外 (境界両側: last=size-1は有効, last=sizeで例外)
TEST(CompositeCurveReplaceCurvesTest, ThrowsOutOfRangeWhenLastTooLarge) {
    auto c = MakeChain3Lines();

    EXPECT_THROW(c.cc->ReplaceCurves(0, 3, {}), std::out_of_range);
    EXPECT_NO_THROW(c.cc->ReplaceCurves(2, 2, {}));  // 末尾削除として有効
}

// エラー: new_curvesにnullptrが含まれる
TEST(CompositeCurveReplaceCurvesTest, ThrowsInvalidArgumentWhenContainsNull) {
    auto c = MakeChain3Lines();

    EXPECT_THROW(c.cc->ReplaceCurves(1, 1, {nullptr}), std::invalid_argument);
}

// エラー: 左境界 (前隣接の終点と置換列先頭の始点) の接合違反
TEST(CompositeCurveReplaceCurvesTest, ThrowsEntityValueErrorOnLeftBoundaryGap) {
    auto c = MakeChain3Lines();
    // 始点がL0終点(100,0,0)からkHugeGapずれた直線
    const auto bad = i_ent::MakeLine(
        Vector3d{100.0, kHugeGap, 0.0}, Vector3d{100.0, 100.0, 0.0});

    EXPECT_THROW(c.cc->ReplaceCurves(1, 1, {bad}), igesio::EntityValueError);
}

// エラー: 右境界 (置換列末尾の終点と後隣接の始点) の接合違反
TEST(CompositeCurveReplaceCurvesTest, ThrowsEntityValueErrorOnRightBoundaryGap) {
    auto c = MakeChain3Lines();
    // 終点がL2始点(100,100,0)からkHugeGapずれた直線
    const auto bad = i_ent::MakeLine(
        Vector3d{100.0, 0.0, 0.0}, Vector3d{100.0, 100.0 - kHugeGap, 0.0});

    EXPECT_THROW(c.cc->ReplaceCurves(1, 1, {bad}), igesio::EntityValueError);
}

// エラー: 置換列内部の接合違反
TEST(CompositeCurveReplaceCurvesTest, ThrowsEntityValueErrorOnInternalGap) {
    auto c = MakeChain3Lines();
    const auto a = i_ent::MakeLine(
        Vector3d{100.0, 0.0, 0.0}, Vector3d{150.0, 50.0, 0.0});
    const auto b = i_ent::MakeLine(
        Vector3d{150.0 + kHugeGap, 50.0, 0.0}, Vector3d{100.0, 100.0, 0.0});

    EXPECT_THROW(c.cc->ReplaceCurves(1, 1, {a, b}), igesio::EntityValueError);
}

// 強い例外保証: 失敗後は構成・新曲線のスイッチとも完全に不変
TEST(CompositeCurveReplaceCurvesTest, StrongGuarantee_StateUnchangedOnThrow) {
    auto c = MakeChain3Lines();
    const auto a = i_ent::MakeLine(
        Vector3d{100.0, 0.0, 0.0}, Vector3d{150.0, 50.0, 0.0});
    const auto b = i_ent::MakeLine(
        Vector3d{150.0 + kHugeGap, 50.0, 0.0}, Vector3d{100.0, 100.0, 0.0});
    const auto sw_a = a->GetSubordinateEntitySwitch();
    const auto sw_b = b->GetSubordinateEntitySwitch();

    EXPECT_THROW(c.cc->ReplaceCurves(1, 1, {a, b}), igesio::EntityValueError);

    // 構成は変更されていない
    ASSERT_EQ(c.cc->GetCurveCount(), 3u);
    EXPECT_EQ(c.cc->GetCurveAt(0), c.l0);
    EXPECT_EQ(c.cc->GetCurveAt(1), c.l1);
    EXPECT_EQ(c.cc->GetCurveAt(2), c.l2);
    // 新曲線のスイッチも変更されていない
    EXPECT_EQ(a->GetSubordinateEntitySwitch(), sw_a);
    EXPECT_EQ(b->GetSubordinateEntitySwitch(), sw_b);
}

// 挿入側のスイッチはkPhysicallyDependentに設定され、取り外し側は変更されない
TEST(CompositeCurveReplaceCurvesTest, SetsSubordinateSwitch_KeepsRemovedSwitch) {
    auto c = MakeChain3Lines();
    const auto arc = i_ent::MakeCircularArc(
        Vector2d{100.0, 50.0}, Vector2d{100.0, 0.0}, Vector2d{100.0, 100.0});

    c.cc->ReplaceCurves(1, 1, {arc});

    EXPECT_EQ(arc->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
    // 取り外されたl1のスイッチは戻されない (他からの参照有無を知り得ないため)
    EXPECT_EQ(c.l1->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

// 未解決参照の要素は取り外し時nullptrとして返る (接合チェックもスキップ)
TEST(CompositeCurveReplaceCurvesTest, UnresolvedEntry_RemovedAsNull) {
    const auto cc = MakeUnresolved2Curves();

    const auto removed = cc->ReplaceCurves(0, 0, {});

    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed[0], nullptr);
    EXPECT_EQ(cc->GetCurveCount(), 1u);
}



/**
 * SetCurveAt のテスト
 */

// 端点一致の置換: 旧曲線を返し、数は不変
TEST(CompositeCurveSetCurveAtTest, ReplacesAndReturnsOld) {
    auto c = MakeChain3Lines();
    const auto arc = i_ent::MakeCircularArc(
        Vector2d{100.0, 50.0}, Vector2d{100.0, 0.0}, Vector2d{100.0, 100.0});

    const auto old = c.cc->SetCurveAt(1, arc);

    EXPECT_EQ(old, c.l1);
    ASSERT_EQ(c.cc->GetCurveCount(), 3u);
    EXPECT_EQ(c.cc->GetCurveAt(1), arc);
}

// エラー: nullptr
TEST(CompositeCurveSetCurveAtTest, ThrowsInvalidArgumentWhenNull) {
    auto c = MakeChain3Lines();

    EXPECT_THROW(c.cc->SetCurveAt(1, nullptr), std::invalid_argument);
}

// エラー: indexが範囲外 (境界両側: index=size-1は有効, index=sizeで例外)
TEST(CompositeCurveSetCurveAtTest, ThrowsOutOfRangeWhenIndexTooLarge) {
    auto c = MakeChain3Lines();
    // 末尾置換用: L1終点(100,100,0)に接続し、右隣接はないため終点は自由
    const auto tail = i_ent::MakeLine(
        Vector3d{100.0, 100.0, 0.0}, Vector3d{300.0, 100.0, 0.0});

    EXPECT_THROW(c.cc->SetCurveAt(3, tail), std::out_of_range);
    EXPECT_NO_THROW(c.cc->SetCurveAt(2, tail));
}

// エラー: 前後と接続しない曲線への置換
TEST(CompositeCurveSetCurveAtTest, ThrowsEntityValueErrorWhenDiscontinuous) {
    auto c = MakeChain3Lines();
    const auto bad = i_ent::MakeLine(
        Vector3d{100.0, kHugeGap, 0.0}, Vector3d{100.0, 100.0, 0.0});

    EXPECT_THROW(c.cc->SetCurveAt(1, bad), igesio::EntityValueError);
}



/**
 * RemoveFirstCurve / RemoveLastCurve のテスト
 */

// 先頭削除: 取り外した曲線を返し、範囲が縮小する
TEST(CompositeCurveRemoveCurveTest, RemoveFirst_ReturnsHeadCurve) {
    auto c = MakeChain3Lines();

    const auto removed = c.cc->RemoveFirstCurve();

    EXPECT_EQ(removed, c.l0);
    ASSERT_EQ(c.cc->GetCurveCount(), 2u);
    EXPECT_EQ(c.cc->GetCurveAt(0), c.l1);
    EXPECT_NEAR(c.cc->GetParameterRange()[1], 2.0, kTol);
}

// 末尾削除: 取り外した曲線を返し、範囲が縮小する
TEST(CompositeCurveRemoveCurveTest, RemoveLast_ReturnsTailCurve) {
    auto c = MakeChain3Lines();

    const auto removed = c.cc->RemoveLastCurve();

    EXPECT_EQ(removed, c.l2);
    ASSERT_EQ(c.cc->GetCurveCount(), 2u);
    EXPECT_EQ(c.cc->GetCurveAt(1), c.l1);
    EXPECT_NEAR(c.cc->GetParameterRange()[1], 2.0, kTol);
}

// 境界: 1曲線構成からの削除で空になる
TEST(CompositeCurveRemoveCurveTest, SingleCurve_RemoveEmptiesComposite) {
    const auto line = i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0});
    const auto cc = i_ent::MakeCompositeCurve({line});

    const auto removed = cc->RemoveFirstCurve();

    EXPECT_EQ(removed, line);
    EXPECT_EQ(cc->GetCurveCount(), 0u);
}

// エラー: 空の複合曲線からの削除
TEST(CompositeCurveRemoveCurveTest, ThrowsOutOfRangeWhenEmpty) {
    const auto cc = std::make_shared<CompositeCurve>();

    EXPECT_THROW(cc->RemoveFirstCurve(), std::out_of_range);
    EXPECT_THROW(cc->RemoveLastCurve(), std::out_of_range);
}

// 繰り返し削除: 全削除後はout_of_range
TEST(CompositeCurveRemoveCurveTest, RepeatedRemoval_UntilEmptyThenThrows) {
    auto c = MakeChain3Lines();

    EXPECT_NO_THROW(c.cc->RemoveLastCurve());
    EXPECT_NO_THROW(c.cc->RemoveLastCurve());
    EXPECT_NO_THROW(c.cc->RemoveLastCurve());
    EXPECT_EQ(c.cc->GetCurveCount(), 0u);
    EXPECT_THROW(c.cc->RemoveLastCurve(), std::out_of_range);
}



/**
 * ClearCurves のテスト
 */

// 全削除: 数0・パラメータ範囲{0,0}
TEST(CompositeCurveClearCurvesTest, EmptiesComposite) {
    auto c = MakeChain3Lines();

    c.cc->ClearCurves();

    EXPECT_EQ(c.cc->GetCurveCount(), 0u);
    const auto range = c.cc->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 0.0, kTol);
}

// 空の複合曲線に対しても投げない
TEST(CompositeCurveClearCurvesTest, NoThrowOnEmpty) {
    const auto cc = std::make_shared<CompositeCurve>();

    EXPECT_NO_THROW(cc->ClearCurves());
    EXPECT_NO_THROW(cc->ClearCurves());  // 再実行も安全
}



/**
 * GetCurveBreakParameters のテスト
 */

// 弧+パス: {0, π/2, π/2+1}
TEST(CompositeCurveBreakParametersTest, ArcPath) {
    const auto cc = MakeArcPathQuarter();

    const auto breaks = cc->GetCurveBreakParameters();

    ASSERT_EQ(breaks.size(), 3u);
    EXPECT_NEAR(breaks[0], 0.0, kTol);
    EXPECT_NEAR(breaks[1], kPi / 2.0, kTol);
    EXPECT_NEAR(breaks[2], kPi / 2.0 + 1.0, kTol);
}

// 境界: 空の複合曲線は{0}のみ (サイズn+1=1)
TEST(CompositeCurveBreakParametersTest, Empty_SingleZero) {
    const auto cc = std::make_shared<CompositeCurve>();

    const auto breaks = cc->GetCurveBreakParameters();

    ASSERT_EQ(breaks.size(), 1u);
    EXPECT_NEAR(breaks[0], 0.0, kTol);
}

// 非ゼロ開始パラメータ (NURBS range=[2,5]): パラメータ長3が累積される
TEST(CompositeCurveBreakParametersTest, NonZeroRangeStart) {
    const auto cc = MakePathNurbsNonZeroRange();

    const auto breaks = cc->GetCurveBreakParameters();

    ASSERT_EQ(breaks.size(), 3u);
    EXPECT_NEAR(breaks[0], 0.0, kTol);
    EXPECT_NEAR(breaks[1], 1.0, kTol);
    EXPECT_NEAR(breaks[2], 4.0, kTol);
}

// 未解決参照の曲線はパラメータ長0として扱われる (サイズn+1は維持)
TEST(CompositeCurveBreakParametersTest, Unresolved_ZeroLength) {
    const auto cc = MakeUnresolved2Curves();

    const auto breaks = cc->GetCurveBreakParameters();

    ASSERT_EQ(breaks.size(), 3u);
    EXPECT_NEAR(breaks[0], 0.0, kTol);
    EXPECT_NEAR(breaks[1], 0.0, kTol);
    EXPECT_NEAR(breaks[2], 0.0, kTol);
}



/**
 * TryGetCurveIndexAtParameter のテスト
 */

// 各構成曲線の内部点が正しい(index, t_local)に解決される
TEST(CompositeCurveIndexAtParameterTest, InteriorPoints) {
    auto c = MakeChain3Lines();  // 各Line range=[0,1], 全体[0,3]

    const auto r0 = c.cc->TryGetCurveIndexAtParameter(0.5);
    const auto r1 = c.cc->TryGetCurveIndexAtParameter(1.5);
    const auto r2 = c.cc->TryGetCurveIndexAtParameter(2.5);

    ASSERT_TRUE(r0 && r1 && r2);
    EXPECT_EQ(r0->first, 0u);
    EXPECT_NEAR(r0->second, 0.5, kTol);
    EXPECT_EQ(r1->first, 1u);
    EXPECT_NEAR(r1->second, 0.5, kTol);
    EXPECT_EQ(r2->first, 2u);
    EXPECT_NEAR(r2->second, 0.5, kTol);
}

// 非ゼロ開始パラメータ: ローカル値はrange[0]起点で計算される
TEST(CompositeCurveIndexAtParameterTest, NonZeroRangeStart) {
    const auto cc = MakePathNurbsNonZeroRange();  // NURBS range=[2,5]

    // グローバルt=2.0 → NURBS上のローカル t = 2 + (2.0-1.0) = 3.0
    const auto result = cc->TryGetCurveIndexAtParameter(2.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, 1u);
    EXPECT_NEAR(result->second, 3.0, kTol);
}

// 接合点はその直前の曲線 (区間判定の先勝ち) に解決される (現仕様の明文化)
TEST(CompositeCurveIndexAtParameterTest, JunctionBelongsToPrecedingCurve) {
    auto c = MakeChain3Lines();

    const auto result = c.cc->TryGetCurveIndexAtParameter(1.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, 0u);
    EXPECT_NEAR(result->second, 1.0, kTol);  // 曲線0の終端ローカル値
}

// 境界: 全体範囲の両端は有効
TEST(CompositeCurveIndexAtParameterTest, RangeEnds) {
    auto c = MakeChain3Lines();

    const auto start = c.cc->TryGetCurveIndexAtParameter(0.0);
    const auto end = c.cc->TryGetCurveIndexAtParameter(3.0);

    ASSERT_TRUE(start.has_value());
    EXPECT_EQ(start->first, 0u);
    EXPECT_NEAR(start->second, 0.0, kTol);
    ASSERT_TRUE(end.has_value());
    EXPECT_EQ(end->first, 2u);
    EXPECT_NEAR(end->second, 1.0, kTol);
}

// 範囲外はnullopt
TEST(CompositeCurveIndexAtParameterTest, OutOfRange_Nullopt) {
    auto c = MakeChain3Lines();

    EXPECT_FALSE(c.cc->TryGetCurveIndexAtParameter(-1e-6).has_value());
    EXPECT_FALSE(c.cc->TryGetCurveIndexAtParameter(3.0 + 1e-6).has_value());
}



/**
 * TryGetGlobalParameter のテスト
 */

// 代表: 各構成曲線のローカル値がグローバル値へ正しく写像される
TEST(CompositeCurveGlobalParameterTest, Representative) {
    auto c = MakeChain3Lines();

    const auto result = c.cc->TryGetGlobalParameter(1, 0.7);

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(*result, 1.7, kTol);
}

// 非ゼロ開始パラメータ: NURBS (range=[2,5]) の t_local=3.5 → グローバル2.5
TEST(CompositeCurveGlobalParameterTest, NonZeroRangeStart) {
    const auto cc = MakePathNurbsNonZeroRange();

    const auto result = cc->TryGetGlobalParameter(1, 3.5);

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(*result, 2.5, kTol);
}

// 往復性: TryGetCurveIndexAtParameterとの相互逆写像
TEST(CompositeCurveGlobalParameterTest, RoundTripWithIndexAtParameter) {
    auto c = MakeChain3Lines();
    const double t_global = 1.7;

    const auto index_local = c.cc->TryGetCurveIndexAtParameter(t_global);
    ASSERT_TRUE(index_local.has_value());
    const auto recovered = c.cc->TryGetGlobalParameter(
        index_local->first, index_local->second);

    ASSERT_TRUE(recovered.has_value());
    EXPECT_NEAR(*recovered, t_global, kTol);
}

// t_localが当該曲線の範囲外の場合はnullopt (境界両側: 範囲端は有効)
TEST(CompositeCurveGlobalParameterTest, LocalOutOfRange_Nullopt) {
    auto c = MakeChain3Lines();  // 各Line range=[0,1]

    EXPECT_TRUE(c.cc->TryGetGlobalParameter(1, 0.0).has_value());
    EXPECT_TRUE(c.cc->TryGetGlobalParameter(1, 1.0).has_value());
    EXPECT_FALSE(c.cc->TryGetGlobalParameter(1, -1e-6).has_value());
    EXPECT_FALSE(c.cc->TryGetGlobalParameter(1, 1.0 + 1e-6).has_value());
}

// エラー: indexが範囲外 (境界両側: index=size-1は有効, index=sizeで例外)
TEST(CompositeCurveGlobalParameterTest, ThrowsOutOfRangeWhenIndexTooLarge) {
    auto c = MakeChain3Lines();

    EXPECT_THROW(c.cc->TryGetGlobalParameter(3, 0.5), std::out_of_range);
    EXPECT_NO_THROW(c.cc->TryGetGlobalParameter(2, 0.5));
}

// 未解決参照の曲線はnullopt
TEST(CompositeCurveGlobalParameterTest, Unresolved_Nullopt) {
    const auto cc = MakeUnresolved2Curves();

    EXPECT_FALSE(cc->TryGetGlobalParameter(0, 0.5).has_value());
}



/**
 * GetJunctionGaps のテスト
 */

// 連続チェーン: 全接合点の隙間≈0
TEST(CompositeCurveJunctionGapsTest, ContiguousChain_NearZero) {
    auto c = MakeChain3Lines();

    const auto gaps = c.cc->GetJunctionGaps();

    ASSERT_EQ(gaps.size(), 2u);
    ASSERT_TRUE(gaps[0].has_value());
    ASSERT_TRUE(gaps[1].has_value());
    EXPECT_NEAR(*gaps[0], 0.0, kTol);
    EXPECT_NEAR(*gaps[1], 0.0, kTol);
}

// 許容内の隙間 (AddCurveで合法構築) が定量的に報告される
TEST(CompositeCurveJunctionGapsTest, SmallGap_ReportsValue) {
    const auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0}));
    cc->AddCurve(i_ent::MakeLine(
        Vector3d{100.0 + kGapBelowTol, 0.0, 0.0}, Vector3d{200.0, 0.0, 0.0}));

    const auto gaps = cc->GetJunctionGaps();

    ASSERT_EQ(gaps.size(), 1u);
    ASSERT_TRUE(gaps[0].has_value());
    EXPECT_NEAR(*gaps[0], kGapBelowTol, kTol);
}

// 境界: 曲線が2つ未満の場合は空
TEST(CompositeCurveJunctionGapsTest, FewerThanTwoCurves_Empty) {
    const auto empty_cc = std::make_shared<CompositeCurve>();
    EXPECT_TRUE(empty_cc->GetJunctionGaps().empty());

    const auto single = i_ent::MakeCompositeCurve({i_ent::MakeLine(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{100.0, 0.0, 0.0})});
    EXPECT_TRUE(single->GetJunctionGaps().empty());
}

// 端点が取得できない接合点 (未解決参照) はnullopt
TEST(CompositeCurveJunctionGapsTest, UnresolvedJunction_Nullopt) {
    const auto cc = MakeUnresolved2Curves();

    const auto gaps = cc->GetJunctionGaps();

    ASSERT_EQ(gaps.size(), 1u);
    EXPECT_FALSE(gaps[0].has_value());
}
