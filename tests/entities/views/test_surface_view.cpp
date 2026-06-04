/**
 * @file tests/entities/views/test_surface_view.cpp
 * @brief entities/views/surface_view.hのテスト
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象 (SurfaceView):
 *   - コンストラクタ (nullガード、ID採番、View畳み込み)
 *   - デストラクタ / ムーブ (int型IDの解放、二重解放回避)
 *   - 委譲: GetSourceID/GetType/GetFormNumber/IsSupported/NDim/IsUClosed/
 *           IsVClosed/GetParameterRange
 *   - 座標系の意味論: TryGetDefined* / TryGet*(継承) /
 *                     TryGetDerivatives(3引数) / GetDefinedBoundingBox /
 *                     GetBoundingBox(継承)
 *
 * TODO: 異常系のうち例外を投げるのはコンストラクタのnullガードのみ。範囲外パラメータは
 *       例外ではなくnulloptを返す仕様のため正常系(退化)として扱う。
 */
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/views/surface_view.h"
#include "./views_for_testing.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Matrix4d;
using igesio::Vector3d;
using igesio::tests::MakeTransform;
using igesio::tests::MockSurface;

/// @brief 浮動小数比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief テストで使用する代表パラメータ (定義域 [0,1]x[0,1] の内点)
constexpr double kU = 0.5;
constexpr double kV = 0.25;

/// @brief 代表的なM_entity (z軸90度回転 + 並進(10,20,30))
inline Matrix4d SampleMEntity() {
    return MakeTransform(igesio::tests::kPiHalf, Vector3d(0, 0, 1),
                         Vector3d(10, 20, 30));
}

}  // namespace



/**
 * コンストラクタ・ライフサイクル
 */

// nullptrのbaseで構築すると例外
TEST(SurfaceViewTest, Constructor_ThrowsInvalidArgumentWhenBaseIsNull) {
    std::shared_ptr<const i_ent::ISurface> null_base;
    EXPECT_THROW({ i_ent::SurfaceView view(null_base); (void)view; },
                 std::invalid_argument);
}

// 有効なbaseでは例外を投げない
TEST(SurfaceViewTest, Constructor_DoesNotThrowWhenBaseIsValid) {
    auto base = std::make_shared<MockSurface>();
    EXPECT_NO_THROW({ i_ent::SurfaceView view(base); (void)view; });
}

// 固有IDはkEntityView型を持つ
TEST(SurfaceViewTest, GetID_IsEntityViewType) {
    auto base = std::make_shared<MockSurface>();
    i_ent::SurfaceView view(base);

    ASSERT_TRUE(view.GetID().IsSet());
    EXPECT_EQ(view.GetID().GetIdentifier()->GetObjectType(),
              igesio::ObjectType::kEntityView);
    EXPECT_NE(view.GetID(), base->GetID());
}

// 同一baseから生成した2つのViewは別個のIDを持つ
TEST(SurfaceViewTest, GetID_IsUniquePerInstance) {
    auto base = std::make_shared<MockSurface>();
    i_ent::SurfaceView v1(base);
    i_ent::SurfaceView v2(base);

    EXPECT_NE(v1.GetID(), v2.GetID());
    EXPECT_NE(v1.GetID().ToInt(), v2.GetID().ToInt());
}

// View畳み込み: 内側Viewへ重ねると元曲面へ繋ぎ直り、変換が合成される
TEST(SurfaceViewTest, Fold_RebindsToOriginalAndComposesPlacement) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    const Matrix4d p_inner = MakeTransform(0.0, Vector3d(0, 0, 1),
                                           Vector3d(1, 0, 0));
    const Matrix4d p_outer = MakeTransform(0.0, Vector3d(0, 0, 1),
                                           Vector3d(0, 5, 0));

    auto inner = std::make_shared<i_ent::SurfaceView>(base, p_inner);
    i_ent::SurfaceView outer(inner, p_outer);

    // (a) GetSourceIDは内側Viewではなく元曲面のID
    EXPECT_EQ(outer.GetSourceID(), base->GetID());

    // (b) サンプル点は合成変換 p_outer·p_inner を1回掛けた値と一致
    const Matrix4d composed = p_outer * p_inner;
    auto expected = base->TryGetPointAt(kU, kV, composed);
    auto actual = outer.TryGetPointAt(kU, kV);
    ASSERT_TRUE(expected.has_value());
    ASSERT_TRUE(actual.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*expected, *actual, kTol))
            << "expected: " << expected->transpose()
            << ", actual: " << actual->transpose();
}

// デストラクタはint型IDを解放する (外部強参照でweak_ptr expiryと切り分け)
TEST(SurfaceViewTest, Destructor_ReleasesIntID) {
    int real_id = igesio::kInvalidIntID;
    igesio::ObjectID kept;  // Identifierを生存させる強参照
    {
        auto base = std::make_shared<MockSurface>();
        i_ent::SurfaceView view(base);
        real_id = view.GetID().ToInt();
        kept = view.GetID();
        EXPECT_TRUE(igesio::IDGenerator::TryGetByIntID(real_id).has_value());
    }
    // keptが生存しているためexpireしない。nulloptはmap.erase(=Release)の証拠
    EXPECT_FALSE(igesio::IDGenerator::TryGetByIntID(real_id).has_value());
}

// ムーブ後: 実IDはムーブ先へ移り、ムーブ元は空ID(=Release(0)無害)になる
TEST(SurfaceViewTest, Move_LeavesSourceEmptyToAvoidDoubleRelease) {
    auto base = std::make_shared<MockSurface>();
    auto v1 = std::make_unique<i_ent::SurfaceView>(base);
    const int real_id = v1->GetID().ToInt();
    const auto src_id = v1->GetSourceID();
    igesio::ObjectID kept = v1->GetID();  // 強参照

    i_ent::SurfaceView v2 = std::move(*v1);
    EXPECT_EQ(v2.GetID().ToInt(), real_id);
    EXPECT_EQ(v2.GetSourceID(), src_id);
    EXPECT_FALSE(v1->GetID().IsSet());
    EXPECT_EQ(v1->GetID().ToInt(), igesio::kInvalidIntID);

    v1.reset();  // ~v1 → Release(0)は無害
    EXPECT_TRUE(igesio::IDGenerator::TryGetByIntID(real_id).has_value());
}

// コピー不可・ムーブ可
TEST(SurfaceViewTest, TypeTraits_IsMoveOnly) {
    static_assert(!std::is_copy_constructible_v<i_ent::SurfaceView>,
                  "SurfaceView must not be copy constructible");
    static_assert(!std::is_copy_assignable_v<i_ent::SurfaceView>,
                  "SurfaceView must not be copy assignable");
    static_assert(std::is_move_constructible_v<i_ent::SurfaceView>,
                  "SurfaceView must be move constructible");
    SUCCEED();
}



/**
 * 委譲 (素通し)
 */

// メタデータはbaseへ委譲される
TEST(SurfaceViewTest, Delegation_ForwardsMetadataToBase) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    i_ent::SurfaceView view(base);

    EXPECT_EQ(view.GetSourceID(), base->GetID());
    EXPECT_EQ(view.GetType(), base->GetType());
    EXPECT_EQ(view.GetFormNumber(), base->GetFormNumber());
    EXPECT_EQ(view.IsSupported(), base->IsSupported());
    EXPECT_EQ(view.NDim(), base->NDim());
    EXPECT_EQ(view.IsUClosed(), base->IsUClosed());
    EXPECT_EQ(view.IsVClosed(), base->IsVClosed());
    EXPECT_EQ(view.GetParameterRange(), base->GetParameterRange());
}



/**
 * 座標系の意味論
 */

// Viewの定義空間 = baseのM_entity適用後 (S_defではない点に注意)
TEST(SurfaceViewTest, TryGetDefinedPointAt_EqualsBaseWithMEntity) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    i_ent::SurfaceView view(base);  // placement = Identity

    auto expected = base->TryGetPointAt(kU, kV);  // M_entity適用後の実空間点
    auto actual = view.TryGetDefinedPointAt(kU, kV);
    ASSERT_TRUE(expected.has_value());
    ASSERT_TRUE(actual.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*expected, *actual, kTol));
}

// placement=Identityのとき TryGetPointAt は base->TryGetPointAt と一致
TEST(SurfaceViewTest, TryGetPointAt_EqualsBaseWhenIdentity) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    i_ent::SurfaceView view(base);

    auto expected = base->TryGetPointAt(kU, kV);
    auto actual = view.TryGetPointAt(kU, kV);
    ASSERT_TRUE(expected.has_value());
    ASSERT_TRUE(actual.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*expected, *actual, kTol));
}

// 並進placement: 点は+T、接線(Su,Sv)はベクトルなので不変
TEST(SurfaceViewTest, TryGetPointAt_ShiftsByTranslation) {
    auto base = std::make_shared<MockSurface>();  // M_entity = Identity
    const Vector3d trans(5, -7, 11);
    const Matrix4d placement = MakeTransform(0.0, Vector3d(0, 0, 1), trans);
    i_ent::SurfaceView view(base, placement);

    auto base_pt = base->TryGetPointAt(kU, kV);
    auto view_pt = view.TryGetPointAt(kU, kV);
    ASSERT_TRUE(base_pt.has_value());
    ASSERT_TRUE(view_pt.has_value());
    const Vector3d expected_pt = *base_pt + trans;
    EXPECT_TRUE(i_num::IsApproxEqual(expected_pt, *view_pt, kTol))
            << "expected: " << expected_pt.transpose()
            << ", actual: " << view_pt->transpose();
}

// 並進placement: 接線(ベクトル)Su,Svは不変
TEST(SurfaceViewTest, TryGetTangentAt_InvariantUnderTranslation) {
    auto base = std::make_shared<MockSurface>();
    const Matrix4d placement =
        MakeTransform(0.0, Vector3d(0, 0, 1), Vector3d(5, -7, 11));
    i_ent::SurfaceView view(base, placement);

    auto base_tan = base->TryGetTangentAt(kU, kV);
    auto view_tan = view.TryGetTangentAt(kU, kV);
    ASSERT_TRUE(base_tan.has_value());
    ASSERT_TRUE(view_tan.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(base_tan->first, view_tan->first, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(base_tan->second, view_tan->second, kTol));
}

// 回転placement: 点・接線とも R を適用 (2-API整合で検証)
TEST(SurfaceViewTest, TryGet_RotatesUnderRotation) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    const Matrix4d placement =
        MakeTransform(igesio::tests::kPiHalf * 0.5, Vector3d(1, 1, 0),
                      Vector3d(2, 3, 4));
    i_ent::SurfaceView view(base, placement);

    // view.TryGet* == base.TryGet*(placement) であることを点・接線で確認
    auto exp_pt = base->TryGetPointAt(kU, kV, placement);
    auto act_pt = view.TryGetPointAt(kU, kV);
    auto exp_tan = base->TryGetTangentAt(kU, kV, placement);
    auto act_tan = view.TryGetTangentAt(kU, kV);
    ASSERT_TRUE(exp_pt.has_value() && act_pt.has_value());
    ASSERT_TRUE(exp_tan.has_value() && act_tan.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*exp_pt, *act_pt, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(exp_tan->first, act_tan->first, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(exp_tan->second, act_tan->second, kTol));
}

// 法線も2-API整合 (view.TryGetNormalAt == base.TryGetNormalAt(placement))
TEST(SurfaceViewTest, TryGetNormalAt_ConsistentWithBasePlacement) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    const Matrix4d placement =
        MakeTransform(igesio::tests::kPiHalf * 0.5, Vector3d(1, 1, 0),
                      Vector3d(2, 3, 4));
    i_ent::SurfaceView view(base, placement);

    auto exp_n = base->TryGetNormalAt(kU, kV, placement);
    auto act_n = view.TryGetNormalAt(kU, kV);
    ASSERT_TRUE(exp_n.has_value() && act_n.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*exp_n, *act_n, kTol));
}

// TryGetDefinedDerivatives(defined override) は base のIdentity-placement版と一致
TEST(SurfaceViewTest, TryGetDefinedDerivatives_EqualsBaseIdentityPlacement) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    i_ent::SurfaceView view(base);

    const unsigned int order = 1;
    auto expected =
        base->TryGetDerivatives(kU, kV, order, Matrix4d::Identity());
    auto actual = view.TryGetDefinedDerivatives(kU, kV, order);
    ASSERT_TRUE(expected.has_value());
    ASSERT_TRUE(actual.has_value());
    // (0,0), (1,0), (0,1) を比較
    EXPECT_TRUE(i_num::IsApproxEqual((*expected)(0, 0), (*actual)(0, 0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual((*expected)(1, 0), (*actual)(1, 0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual((*expected)(0, 1), (*actual)(0, 1), kTol));
}

// 階別規約: 並進placementで S(0,0)のみ +T、Su/Svは不変 (点/ベクトル取り違え検出)
TEST(SurfaceViewTest, TryGetDerivatives_OrderSemanticsUnderTranslation) {
    // Viewの焼き込みplacementはIdentityとし、4引数版にのみ並進placementを渡す
    auto base = std::make_shared<MockSurface>();  // M_entity = Identity
    i_ent::SurfaceView view(base);                // placement_ = Identity
    const Vector3d trans(5, -7, 11);
    const Matrix4d placement = MakeTransform(0.0, Vector3d(0, 0, 1), trans);

    const unsigned int order = 1;
    auto defined = view.TryGetDerivatives(kU, kV, order);
    auto shifted = view.TryGetDerivatives(kU, kV, order, placement);
    ASSERT_TRUE(defined.has_value() && shifted.has_value());

    // S(0,0)のみ +trans
    const Vector3d expected_00 = (*defined)(0, 0) + trans;
    EXPECT_TRUE(i_num::IsApproxEqual(expected_00, (*shifted)(0, 0), kTol));
    // Su, Svは不変
    EXPECT_TRUE(i_num::IsApproxEqual((*defined)(1, 0), (*shifted)(1, 0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual((*defined)(0, 1), (*shifted)(0, 1), kTol));
}

// 範囲外パラメータは placement を素通りして nullopt
TEST(SurfaceViewTest, TryGetPointAt_ReturnsNulloptWhenOutOfRange) {
    auto base = std::make_shared<MockSurface>();
    const Matrix4d placement =
        MakeTransform(0.0, Vector3d(0, 0, 1), Vector3d(5, -7, 11));
    i_ent::SurfaceView view(base, placement);

    EXPECT_FALSE(view.TryGetPointAt(-1.0, kV).has_value());
    EXPECT_FALSE(view.TryGetPointAt(kU, 2.0).has_value());
}



/**
 * バウンディングボックス (二重適用ガード)
 */

// GetDefinedBoundingBox は base->GetBoundingBox (M_entity済) と一致
TEST(SurfaceViewTest, GetDefinedBoundingBox_EqualsBaseBoundingBox) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    i_ent::SurfaceView view(base);

    auto base_bb = base->GetBoundingBox();
    auto view_bb = view.GetDefinedBoundingBox();
    EXPECT_TRUE(i_num::IsApproxEqual(base_bb.GetControl(),
                                     view_bb.GetControl(), kTol));
}

// 継承GetBoundingBoxは placement·(base BB) を返し、M_entityは二重適用されない
TEST(SurfaceViewTest, GetBoundingBox_AppliesPlacementWithoutDoublingMEntity) {
    auto base = std::make_shared<MockSurface>(SampleMEntity());
    const Vector3d trans(5, -7, 11);
    const Matrix4d placement = MakeTransform(0.0, Vector3d(0, 0, 1), trans);
    i_ent::SurfaceView view(base, placement);

    auto base_bb = base->GetBoundingBox();  // M_entity済
    auto view_bb = view.GetBoundingBox();   // placement·(M_entity済)

    // 並進placement -> 基点は+trans、サイズは不変
    const Vector3d expected_ctrl = base_bb.GetControl() + trans;
    EXPECT_TRUE(i_num::IsApproxEqual(expected_ctrl,
                                     view_bb.GetControl(), kTol));
    const auto base_sizes = base_bb.GetSizes();
    const auto view_sizes = view_bb.GetSizes();
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_NEAR(base_sizes[i], view_sizes[i], kTol);
    }
}
