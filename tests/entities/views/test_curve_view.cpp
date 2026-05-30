/**
 * @file tests/entities/views/test_curve_view.cpp
 * @brief entities/views/curve_view.hのテスト
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象 (CurveView):
 *   - コンストラクタ (nullガード、ID採番、View畳み込み)
 *   - デストラクタ / ムーブ (int型IDの解放、二重解放回避)
 *   - 委譲: GetSourceID/GetType/GetFormNumber/IsSupported/NDim/IsClosed/
 *           GetParameterRange
 *   - 座標系の意味論: TryGetDefined* / TryGet*(継承) / TryGetDerivatives(2引数) /
 *                     GetDefinedBoundingBox / GetBoundingBox(継承)
 *
 * TODO: 法線N/従法線Bの配置適用は接線Tと同一コードパス(Transform→ApplyTransform,
 *       is_point=false)のため、本ファイルでは接線で代表検証する。2-API整合テストで
 *       N/Bも間接的に網羅される。
 * TODO: 異常系のうち例外を投げるのはコンストラクタのnullガードのみ。範囲外パラメータは
 *       例外ではなくnulloptを返す仕様のため正常系(退化)として扱う。
 */
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/matrix.h"
#include "igesio/numerics/tolerance.h"
#include "igesio/entities/views/curve_view.h"
#include "./views_for_testing.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Matrix4d;
using igesio::Vector3d;
using igesio::tests::MakeTransform;
using igesio::tests::MockCurve;

/// @brief 浮動小数比較の許容誤差
constexpr double kTol = 1e-9;

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
TEST(CurveViewTest, Constructor_ThrowsInvalidArgumentWhenBaseIsNull) {
    std::shared_ptr<const i_ent::ICurve> null_base;
    EXPECT_THROW({ i_ent::CurveView view(null_base); (void)view; },
                 std::invalid_argument);
}

// 有効なbaseでは例外を投げない
TEST(CurveViewTest, Constructor_DoesNotThrowWhenBaseIsValid) {
    auto base = std::make_shared<MockCurve>();
    EXPECT_NO_THROW({ i_ent::CurveView view(base); (void)view; });
}

// 固有IDはkEntityView型を持つ
TEST(CurveViewTest, GetID_IsEntityViewType) {
    auto base = std::make_shared<MockCurve>();
    i_ent::CurveView view(base);

    ASSERT_TRUE(view.GetID().IsSet());
    EXPECT_EQ(view.GetID().GetIdentifier()->GetObjectType(),
              igesio::ObjectType::kEntityView);
    // baseのIDとは別個
    EXPECT_NE(view.GetID(), base->GetID());
}

// 同一baseから生成した2つのViewは別個のIDを持つ
TEST(CurveViewTest, GetID_IsUniquePerInstance) {
    auto base = std::make_shared<MockCurve>();
    i_ent::CurveView v1(base);
    i_ent::CurveView v2(base);

    EXPECT_NE(v1.GetID(), v2.GetID());
    EXPECT_NE(v1.GetID().ToInt(), v2.GetID().ToInt());
}

// View畳み込み: 内側Viewへ重ねると元曲線へ繋ぎ直り、変換が合成される
TEST(CurveViewTest, Fold_RebindsToOriginalAndComposesPlacement) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    const Matrix4d p_inner = MakeTransform(0.0, Vector3d(0, 0, 1),
                                           Vector3d(1, 0, 0));
    const Matrix4d p_outer = MakeTransform(0.0, Vector3d(0, 0, 1),
                                           Vector3d(0, 5, 0));

    auto inner = std::make_shared<i_ent::CurveView>(base, p_inner);
    i_ent::CurveView outer(inner, p_outer);

    // (a) GetSourceIDは内側Viewではなく元曲線のID
    EXPECT_EQ(outer.GetSourceID(), base->GetID());

    // (b) サンプル点は合成変換 p_outer·p_inner を1回掛けた値と一致
    const double t = igesio::tests::kPiHalf * 0.5;
    const Matrix4d composed = p_outer * p_inner;
    auto expected = base->TryGetPointAt(t, composed);
    auto actual = outer.TryGetPointAt(t);
    ASSERT_TRUE(expected.has_value());
    ASSERT_TRUE(actual.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*expected, *actual, kTol))
            << "expected: " << expected->transpose()
            << ", actual: " << actual->transpose();
}

// デストラクタはint型IDを解放する (外部強参照でweak_ptr expiryと切り分け)
TEST(CurveViewTest, Destructor_ReleasesIntID) {
    int real_id = igesio::kInvalidIntID;
    igesio::ObjectID kept;  // Identifierを生存させる強参照
    {
        auto base = std::make_shared<MockCurve>();
        i_ent::CurveView view(base);
        real_id = view.GetID().ToInt();
        kept = view.GetID();
        EXPECT_TRUE(igesio::IDGenerator::TryGetByIntID(real_id).has_value());
    }
    // keptが生存しているためexpireしない。nulloptはmap.erase(=Release)の証拠
    EXPECT_FALSE(igesio::IDGenerator::TryGetByIntID(real_id).has_value());
}

// ムーブ後: 実IDはムーブ先へ移り、ムーブ元は空ID(=Release(0)無害)になる
TEST(CurveViewTest, Move_LeavesSourceEmptyToAvoidDoubleRelease) {
    auto base = std::make_shared<MockCurve>();
    auto v1 = std::make_unique<i_ent::CurveView>(base);
    const int real_id = v1->GetID().ToInt();
    const auto src_id = v1->GetSourceID();
    igesio::ObjectID kept = v1->GetID();  // 強参照

    i_ent::CurveView v2 = std::move(*v1);
    // ムーブ先が実IDと委譲を引き継ぐ
    EXPECT_EQ(v2.GetID().ToInt(), real_id);
    EXPECT_EQ(v2.GetSourceID(), src_id);
    // ムーブ元は空ID -> ToInt()==kInvalidIntID (二重解放ガードの核心)
    EXPECT_FALSE(v1->GetID().IsSet());
    EXPECT_EQ(v1->GetID().ToInt(), igesio::kInvalidIntID);

    // ムーブ元を破棄してもRelease(0)は無害で、実IDは解放されない
    v1.reset();
    EXPECT_TRUE(igesio::IDGenerator::TryGetByIntID(real_id).has_value());
}

// コピー不可・ムーブ可
TEST(CurveViewTest, TypeTraits_IsMoveOnly) {
    static_assert(!std::is_copy_constructible_v<i_ent::CurveView>,
                  "CurveView must not be copy constructible");
    static_assert(!std::is_copy_assignable_v<i_ent::CurveView>,
                  "CurveView must not be copy assignable");
    static_assert(std::is_move_constructible_v<i_ent::CurveView>,
                  "CurveView must be move constructible");
    SUCCEED();
}



/**
 * 委譲 (素通し)
 */

// メタデータはbaseへ委譲される
TEST(CurveViewTest, Delegation_ForwardsMetadataToBase) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    i_ent::CurveView view(base);

    EXPECT_EQ(view.GetSourceID(), base->GetID());
    EXPECT_EQ(view.GetType(), base->GetType());
    EXPECT_EQ(view.GetFormNumber(), base->GetFormNumber());
    EXPECT_EQ(view.IsSupported(), base->IsSupported());
    EXPECT_EQ(view.NDim(), base->NDim());
    EXPECT_EQ(view.IsClosed(), base->IsClosed());
    EXPECT_EQ(view.GetParameterRange(), base->GetParameterRange());
}



/**
 * 座標系の意味論
 */

// Viewの定義空間 = baseのM_entity適用後 (P_defではない点に注意)
TEST(CurveViewTest, TryGetDefinedPointAt_EqualsBaseWithMEntity) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    i_ent::CurveView view(base);  // placement = Identity

    const double t = igesio::tests::kPiHalf * 0.5;
    auto expected = base->TryGetPointAt(t);  // M_entity適用後の実空間点
    auto actual = view.TryGetDefinedPointAt(t);
    ASSERT_TRUE(expected.has_value());
    ASSERT_TRUE(actual.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*expected, *actual, kTol));
}

// placement=Identityのとき TryGetPointAt は base->TryGetPointAt と一致
TEST(CurveViewTest, TryGetPointAt_EqualsBaseWhenIdentity) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    i_ent::CurveView view(base);

    const double t = igesio::tests::kPiHalf * 0.5;
    auto expected = base->TryGetPointAt(t);
    auto actual = view.TryGetPointAt(t);
    ASSERT_TRUE(expected.has_value());
    ASSERT_TRUE(actual.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*expected, *actual, kTol));
}

// 並進placement: 点は+T、接線はベクトルなので不変
TEST(CurveViewTest, TryGetPointAt_ShiftsByTranslation) {
    auto base = std::make_shared<MockCurve>();  // M_entity = Identity
    const Vector3d trans(5, -7, 11);
    const Matrix4d placement = MakeTransform(0.0, Vector3d(0, 0, 1), trans);
    i_ent::CurveView view(base, placement);

    const double t = igesio::tests::kPiHalf * 0.5;
    auto base_pt = base->TryGetPointAt(t);
    auto view_pt = view.TryGetPointAt(t);
    ASSERT_TRUE(base_pt.has_value());
    ASSERT_TRUE(view_pt.has_value());
    const Vector3d expected_pt = *base_pt + trans;
    EXPECT_TRUE(i_num::IsApproxEqual(expected_pt, *view_pt, kTol))
            << "expected: " << expected_pt.transpose()
            << ", actual: " << view_pt->transpose();
}

// 並進placement: 接線(ベクトル)は不変
TEST(CurveViewTest, TryGetTangentAt_InvariantUnderTranslation) {
    auto base = std::make_shared<MockCurve>();
    const Matrix4d placement =
        MakeTransform(0.0, Vector3d(0, 0, 1), Vector3d(5, -7, 11));
    i_ent::CurveView view(base, placement);

    const double t = igesio::tests::kPiHalf * 0.5;
    auto base_tan = base->TryGetTangentAt(t);
    auto view_tan = view.TryGetTangentAt(t);
    ASSERT_TRUE(base_tan.has_value());
    ASSERT_TRUE(view_tan.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*base_tan, *view_tan, kTol));
}

// 回転placement: 点・接線とも R を適用 (2-API整合で検証)
TEST(CurveViewTest, TryGet_RotatesUnderRotation) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    const Matrix4d placement =
        MakeTransform(igesio::tests::kPiHalf * 0.5, Vector3d(1, 1, 0),
                      Vector3d(2, 3, 4));
    i_ent::CurveView view(base, placement);

    const double t = igesio::tests::kPiHalf * 0.5;
    // view.TryGet* == base.TryGet*(placement) であることを点・接線で確認
    auto exp_pt = base->TryGetPointAt(t, placement);
    auto act_pt = view.TryGetPointAt(t);
    auto exp_tan = base->TryGetTangentAt(t, placement);
    auto act_tan = view.TryGetTangentAt(t);
    ASSERT_TRUE(exp_pt.has_value() && act_pt.has_value());
    ASSERT_TRUE(exp_tan.has_value() && act_tan.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*exp_pt, *act_pt, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(*exp_tan, *act_tan, kTol));
}

// 法線・従法線も2-API整合 (view.TryGet* == base.TryGet*(placement))
TEST(CurveViewTest, TryGetNormalBinormal_ConsistentWithBasePlacement) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    const Matrix4d placement =
        MakeTransform(igesio::tests::kPiHalf * 0.5, Vector3d(1, 1, 0),
                      Vector3d(2, 3, 4));
    i_ent::CurveView view(base, placement);

    const double t = igesio::tests::kPiHalf * 0.5;
    auto exp_n = base->TryGetNormalAt(t, placement);
    auto act_n = view.TryGetNormalAt(t);
    auto exp_b = base->TryGetBinormalAt(t, placement);
    auto act_b = view.TryGetBinormalAt(t);
    ASSERT_TRUE(exp_n.has_value() && act_n.has_value());
    ASSERT_TRUE(exp_b.has_value() && act_b.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*exp_n, *act_n, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(*exp_b, *act_b, kTol));
}

// TryGetDefinedDerivatives(defined override) は base のIdentity-placement版と一致
TEST(CurveViewTest, TryGetDefinedDerivatives_EqualsBaseIdentityPlacement) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    i_ent::CurveView view(base);

    const double t = igesio::tests::kPiHalf * 0.5;
    const unsigned int n = 2;
    auto expected = base->TryGetDerivatives(t, n, Matrix4d::Identity());
    auto actual = view.TryGetDefinedDerivatives(t, n);
    ASSERT_TRUE(expected.has_value());
    ASSERT_TRUE(actual.has_value());
    ASSERT_EQ(expected->derivatives.size(), actual->derivatives.size());
    for (unsigned int k = 0; k <= n; ++k) {
        EXPECT_TRUE(i_num::IsApproxEqual((*expected)[k], (*actual)[k], kTol))
                << "derivative order " << k << " mismatch";
    }
}

// 階別規約: 並進placementで 0階のみ +T、1階以上は不変 (点/ベクトル取り違え検出)
TEST(CurveViewTest, TryGetDerivatives_OrderSemanticsUnderTranslation) {
    // Viewの焼き込みplacementはIdentityとし、3引数版にのみ並進placementを渡す
    // (焼き込みplacementと3引数placementの二重適用を避ける)
    auto base = std::make_shared<MockCurve>();  // M_entity = Identity
    i_ent::CurveView view(base);                // placement_ = Identity
    const Vector3d trans(5, -7, 11);
    const Matrix4d placement = MakeTransform(0.0, Vector3d(0, 0, 1), trans);

    const double t = igesio::tests::kPiHalf * 0.5;
    const unsigned int n = 2;
    // 2引数版 (= M_entity済、ここではIdentityなので定義空間と一致)
    auto defined = view.TryGetDerivatives(t, n);
    // 配置適用版 (3引数) は placement を後掛けする
    auto shifted = view.TryGetDerivatives(t, n, placement);
    ASSERT_TRUE(defined.has_value() && shifted.has_value());

    // 0階のみ +trans
    const Vector3d expected_0 = (*defined)[0] + trans;
    EXPECT_TRUE(i_num::IsApproxEqual(expected_0, (*shifted)[0], kTol));
    // 1階以上は不変
    EXPECT_TRUE(i_num::IsApproxEqual((*defined)[1], (*shifted)[1], kTol));
    EXPECT_TRUE(i_num::IsApproxEqual((*defined)[2], (*shifted)[2], kTol));
}

// 範囲外パラメータは placement を素通りして nullopt
TEST(CurveViewTest, TryGetPointAt_ReturnsNulloptWhenOutOfRange) {
    auto base = std::make_shared<MockCurve>();
    const Matrix4d placement =
        MakeTransform(0.0, Vector3d(0, 0, 1), Vector3d(5, -7, 11));
    i_ent::CurveView view(base, placement);

    EXPECT_FALSE(view.TryGetPointAt(-1.0).has_value());
    EXPECT_FALSE(view.TryGetPointAt(igesio::tests::kPiHalf + 1.0).has_value());
}



/**
 * バウンディングボックス (二重適用ガード)
 */

// GetDefinedBoundingBox は base->GetBoundingBox (M_entity済) と一致
TEST(CurveViewTest, GetDefinedBoundingBox_EqualsBaseBoundingBox) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    i_ent::CurveView view(base);

    auto base_bb = base->GetBoundingBox();
    auto view_bb = view.GetDefinedBoundingBox();
    EXPECT_TRUE(i_num::IsApproxEqual(base_bb.GetControl(),
                                     view_bb.GetControl(), kTol));
}

// 継承GetBoundingBoxは placement·(base BB) を返し、M_entityは二重適用されない
TEST(CurveViewTest, GetBoundingBox_AppliesPlacementWithoutDoublingMEntity) {
    auto base = std::make_shared<MockCurve>(SampleMEntity());
    const Vector3d trans(5, -7, 11);
    const Matrix4d placement = MakeTransform(0.0, Vector3d(0, 0, 1), trans);
    i_ent::CurveView view(base, placement);

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
