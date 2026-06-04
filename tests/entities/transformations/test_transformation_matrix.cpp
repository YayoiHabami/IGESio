/**
 * @file entities/transformations/test_transformation_matrix.cpp
 * @brief TransformationMatrix (Type 124) のテスト
 * @author Yayoi Habami
 * @date 2026-06-01
 *
 * ### 対象
 * - アクセサ: GetRotation / GetTranslation / GetTransformation
 *   (R*v + T の同次変換行列を構成すること)
 * - フォーム種別: GetMatrixType / SetMatrixType
 * - 検証 (severity): ValidatePD (Validate 経由)。回転行列の正規直交性・フォーム
 *   整合は「幾何的品質」のため kWarning とし、行列は適用 (描画) 可能なため
 *   is_valid をブロックしない。直接コンストラクタも is_valid のみを見るため、
 *   kWarning のみであれば throw しない。
 * - フォーム別の構造チェック: フォーム0/1 (行列式の符号) およびフォーム10/11/12
 *   (FEM 用の特殊行列構造)
 * - 参照: SetReference / OverwriteTransformationMatrix / GetRefTransformation の
 *   基本動作と循環参照の棄却 (両APIが同一の循環判定を共有すること)
 *
 * TODO: 以下は本テストで未検証。
 *   - 未知のフォーム番号 (ValidatePD の default 節, kError) は SetMainPDParameters
 *     が構築時に throw するため (default 節へ到達しない)、単体テストは構成不可。
 */
#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "igesio/common/errors.h"
#include "igesio/common/validation_result.h"
#include "igesio/numerics/matrix.h"
#include "igesio/entities/transformations/transformation_matrix.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Matrix3d;
using igesio::Matrix4d;
using igesio::Vector3d;
using igesio::ValidationSeverity;
using i_ent::MatrixType;
using i_ent::TransformationMatrix;

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 円周率
constexpr double kPi = 3.14159265358979323846;

/// @brief Z軸まわりに角度angle [rad] 回転させる直交正規行列 (det=+1) を返す
/// @param angle 回転角 [rad]
/// @return 右手系の回転行列 (フォーム0の代表的な妥当行列)
/// @note 第3列が[0,0,1]・第3行が[0,0,1]であり、フォーム11 (円筒座標系) の
///       構造 [[c,-s,0],[s,c,0],[0,0,1]] も満たす
Matrix3d RotationZ(const double angle) {
    Matrix3d rot;
    rot << std::cos(angle), -std::sin(angle), 0.0,
           std::sin(angle),  std::cos(angle), 0.0,
           0.0,              0.0,             1.0;
    return rot;
}

/// @brief X軸まわりに角度angle [rad] 回転させる直交正規行列 (det=+1) を返す
/// @param angle 回転角 [rad]
/// @return 右手系の回転行列
/// @note 第3列が[0,0,1]でないため、フォーム11 (円筒座標系) の構造を満たさない
Matrix3d RotationX(const double angle) {
    Matrix3d rot;
    rot << 1.0, 0.0,              0.0,
           0.0, std::cos(angle), -std::sin(angle),
           0.0, std::sin(angle),  std::cos(angle);
    return rot;
}

/// @brief 仕様 (Figure 34) の例の回転行列を返す
/// @return R = [[0,0,1],[0,1,0],[-1,0,0]]
/// @note 直交正規・det=+1 のためフォーム0で妥当 (警告なし)
Matrix3d SpecExampleRotation() {
    Matrix3d rot;
    rot << 0.0, 0.0, 1.0,
           0.0, 1.0, 0.0,
          -1.0, 0.0, 0.0;
    return rot;
}

/// @brief det=-1 の直交正規行列 (z軸方向の反射) を返す
/// @return diag(1, 1, -1)
/// @note 直交正規だが行列式が-1のためフォーム1で妥当 (フォーム0ではdet違反)
Matrix3d ReflectionZ() {
    Matrix3d refl = Matrix3d::Identity();
    refl(2, 2) = -1.0;
    return refl;
}

/// @brief フォーム12 (球座標系) の構造を満たす直交正規行列を返す
/// @return θ0=90°, φ0=0° の球座標行列 [[1,0,0],[0,0,1],[0,-1,0]]
/// @note 第3列の第3成分 R(3,3)=0 を満たす (直交正規・det=+1)
Matrix3d SphericalExampleRotation() {
    Matrix3d rot;
    rot << 1.0,  0.0, 0.0,
           0.0,  0.0, 1.0,
           0.0, -1.0, 0.0;
    return rot;
}

/// @brief 検証結果に含まれる、指定した重大度のメッセージ数を数える
/// @param result 検証結果
/// @param sev 数える重大度
/// @return 該当するメッセージの数
int CountSeverity(const igesio::ValidationResult& result,
                  const ValidationSeverity sev) {
    int count = 0;
    for (const auto& e : result.errors) {
        if (e.severity == sev) ++count;
    }
    return count;
}

}  // namespace



/**
 * アクセサ (GetRotation / GetTranslation / GetTransformation)
 */

// GetRotation / GetTranslationはコンストラクタで与えた値をそのまま返す
TEST(TransformationMatrixAccessorTest, GetRotationAndTranslation_ReturnConstructorValues) {
    const Matrix3d rot = RotationZ(kPi / 6.0);  // 30度
    const Vector3d trans(1.0, 2.0, 3.0);
    const TransformationMatrix tm(rot, trans);

    EXPECT_TRUE(tm.GetRotation().isApprox(rot, kTol));
    EXPECT_NEAR(tm.GetTranslation()[0], 1.0, kTol);
    EXPECT_NEAR(tm.GetTranslation()[1], 2.0, kTol);
    EXPECT_NEAR(tm.GetTranslation()[2], 3.0, kTol);
}

// 平行移動ベクトルの既定値はゼロベクトル
TEST(TransformationMatrixAccessorTest, GetTranslation_DefaultsToZero) {
    const TransformationMatrix tm(Matrix3d::Identity());
    EXPECT_NEAR(tm.GetTranslation().norm(), 0.0, kTol);
}

// GetTransformationは [[R, T], [0, 0, 0, 1]] の同次変換行列を構成する
TEST(TransformationMatrixAccessorTest, GetTransformation_BuildsHomogeneousMatrix) {
    const Matrix3d rot = RotationZ(kPi / 6.0);
    const Vector3d trans(1.0, 2.0, 3.0);
    const TransformationMatrix tm(rot, trans);

    const Matrix4d m = tm.GetTransformation();
    // 左上3x3は回転行列 (block<3,3>のカンマがマクロ引数と衝突するため変数に取り出す)
    const Matrix3d upper_left = m.block<3, 3>(0, 0);
    EXPECT_TRUE(upper_left.isApprox(rot, kTol));
    // 右上3x1は平行移動ベクトル
    EXPECT_NEAR(m(0, 3), 1.0, kTol);
    EXPECT_NEAR(m(1, 3), 2.0, kTol);
    EXPECT_NEAR(m(2, 3), 3.0, kTol);
    // 最下行は (0, 0, 0, 1)
    EXPECT_NEAR(m(3, 0), 0.0, kTol);
    EXPECT_NEAR(m(3, 1), 0.0, kTol);
    EXPECT_NEAR(m(3, 2), 0.0, kTol);
    EXPECT_NEAR(m(3, 3), 1.0, kTol);
}



/**
 * フォーム種別 (GetMatrixType / SetMatrixType)
 */

// GetMatrixTypeはフォーム番号に対応する種別を返す
TEST(TransformationMatrixTypeTest, GetMatrixType_ReflectsFormNumber) {
    const TransformationMatrix tm0(Matrix3d::Identity(), Vector3d::Zero(), 0);
    EXPECT_EQ(tm0.GetMatrixType(), MatrixType::kDefault);

    // det=-1 の直交正規行列 (反射) はフォーム1で妥当
    const TransformationMatrix tm1(ReflectionZ(), Vector3d::Zero(), 1);
    EXPECT_EQ(tm1.GetMatrixType(), MatrixType::kLeftHanded);

    const TransformationMatrix tm10(Matrix3d::Identity(), Vector3d::Zero(), 10);
    EXPECT_EQ(tm10.GetMatrixType(), MatrixType::kCartesianOffset);

    const TransformationMatrix tm11(RotationZ(kPi / 4.0), Vector3d::Zero(), 11);
    EXPECT_EQ(tm11.GetMatrixType(), MatrixType::kCylindricalCoordinates);

    const TransformationMatrix tm12(SphericalExampleRotation(), Vector3d::Zero(), 12);
    EXPECT_EQ(tm12.GetMatrixType(), MatrixType::kSphericalCoordinates);
}

// SetMatrixTypeは妥当な種別へ変更でき、GetMatrixTypeに反映される
TEST(TransformationMatrixTypeTest, SetMatrixType_ChangesTypeForValidValues) {
    TransformationMatrix tm(Matrix3d::Identity());
    ASSERT_EQ(tm.GetMatrixType(), MatrixType::kDefault);

    EXPECT_TRUE(tm.SetMatrixType(MatrixType::kLeftHanded));
    EXPECT_EQ(tm.GetMatrixType(), MatrixType::kLeftHanded);

    EXPECT_TRUE(tm.SetMatrixType(MatrixType::kCylindricalCoordinates));
    EXPECT_EQ(tm.GetMatrixType(), MatrixType::kCylindricalCoordinates);
}

// SetMatrixTypeは無効な種別 (列挙範囲外) では変更せずfalseを返す
TEST(TransformationMatrixTypeTest, SetMatrixType_ReturnsFalseForInvalidValue) {
    TransformationMatrix tm(Matrix3d::Identity());
    EXPECT_FALSE(tm.SetMatrixType(static_cast<MatrixType>(99)));
    EXPECT_EQ(tm.GetMatrixType(), MatrixType::kDefault);  // 変更されない
}



/**
 * 検証 (ValidatePD / Validate) のseverityと共通の正規直交性チェック
 */

// 正規直交な単位行列 (form 0) → 警告なし・is_valid=true
TEST(TransformationMatrixValidateTest, Identity_IsValidNoWarning) {
    const TransformationMatrix tm(Matrix3d::Identity());
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// 仕様 Figure 34の例: 入力基底ベクトルが期待どおり変換される。
// R*e1=(0,0,-1), R*e2=(0,1,0), R*e3=(1,0,0)。フォーム0で妥当 (警告なし)。
TEST(TransformationMatrixValidateTest, SpecExampleForm0_TransformsBasisVectorsAndIsClean) {
    const TransformationMatrix tm(SpecExampleRotation());
    const Matrix3d r = tm.GetRotation();

    const Vector3d o1 = r * Vector3d(1.0, 0.0, 0.0);
    const Vector3d o2 = r * Vector3d(0.0, 1.0, 0.0);
    const Vector3d o3 = r * Vector3d(0.0, 0.0, 1.0);
    EXPECT_TRUE(o1.isApprox(Vector3d(0.0, 0.0, -1.0), kTol));
    EXPECT_TRUE(o2.isApprox(Vector3d(0.0, 1.0, 0.0), kTol));
    EXPECT_TRUE(o3.isApprox(Vector3d(1.0, 0.0, 0.0), kTol));

    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// 非正規直交な回転行列 (第1列ノルム=2) → 行列は適用可能なためValidatePDは
// 警告 (kWarning) を出すがis_valid=true (描画ブロックしない)。
// NOTE: 直接コンストラクタはValidatePD().is_validを見るため、kWarningのみならthrowしない。
TEST(TransformationMatrixValidateTest, NonOrthonormal_IsValidWithWarning) {
    Matrix3d rot;
    rot << 2.0, 0.0, 0.0,   // 第1列 = (2,0,0): ノルム2 (非単位)
           0.0, 1.0, 0.0,
           0.0, 0.0, 1.0;
    // form 0 (既定)。非正規直交でもis_valid=true (kWarning) のためthrowしない
    const TransformationMatrix tm(rot);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);  // 描画はブロックしない (本対処の要点)
    EXPECT_EQ(CountSeverity(result, ValidationSeverity::kError), 0);
    EXPECT_GE(CountSeverity(result, ValidationSeverity::kWarning), 1);
}

// 非正規直交でもコンストラクタはthrowしない (kWarningのみのため)
TEST(TransformationMatrixValidateTest, Constructor_DoesNotThrowOnWarningOnlyMatrix) {
    Matrix3d rot;
    rot << 2.0, 0.0, 0.0,
           0.0, 1.0, 0.0,
           0.0, 0.0, 1.0;
    EXPECT_NO_THROW({ const TransformationMatrix tm(rot); });
}

// 列が非単位かつ非直交な行列 → 複数の警告。is_valid=true・throwなし
TEST(TransformationMatrixValidateTest, NonOrthogonalColumns_MultipleWarningsButValid) {
    Matrix3d rot;
    rot << 1.0, 1.0, 0.0,   // 第2列=(1,1,0): 非単位かつ第1列と非直交
           0.0, 1.0, 0.0,
           0.0, 0.0, 1.0;
    const TransformationMatrix tm(rot);  // フォーム0
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_GE(CountSeverity(result, ValidationSeverity::kWarning), 2);
}



/**
 * フォーム0/1 の行列式 (右手系/左手系) チェック
 */

// フォーム0・正規直交な回転行列 (det=+1) → 警告なし・is_valid=true
TEST(TransformationMatrixValidateTest, Form0_RightHandedRotation_NoWarning) {
    const TransformationMatrix tm(RotationZ(kPi / 3.0));  // 60度
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// フォーム0・行列式が-1 (反射) → det違反の警告。ただし適用可能なためis_valid=true
TEST(TransformationMatrixValidateTest, Form0_NegativeDeterminant_WarnsButValid) {
    const TransformationMatrix tm(ReflectionZ(), Vector3d::Zero(), 0);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(CountSeverity(result, ValidationSeverity::kError), 0);
    EXPECT_GE(CountSeverity(result, ValidationSeverity::kWarning), 1);
}

// フォーム1・正規直交な反射行列 (det=-1) → 警告なし・is_valid=true
TEST(TransformationMatrixValidateTest, Form1_LeftHandedReflection_NoWarning) {
    const TransformationMatrix tm(ReflectionZ(), Vector3d::Zero(), 1);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// フォーム1・行列式が+1 (右手系回転) → det違反の警告。is_valid=true
TEST(TransformationMatrixValidateTest, Form1_PositiveDeterminant_WarnsButValid) {
    const TransformationMatrix tm(RotationZ(kPi / 4.0), Vector3d::Zero(), 1);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(CountSeverity(result, ValidationSeverity::kError), 0);
    EXPECT_GE(CountSeverity(result, ValidationSeverity::kWarning), 1);
}



/**
 * フォーム10/11/12 (FEM 用) の構造チェック
 */

// フォーム10: Rが単位行列 → 警告なし・is_valid=true
TEST(TransformationMatrixValidateTest, Form10_Identity_NoWarning) {
    const TransformationMatrix tm(Matrix3d::Identity(), Vector3d(1.0, 2.0, 3.0), 10);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// フォーム10: Rが単位行列でない → 構造違反の警告。is_valid=true
TEST(TransformationMatrixValidateTest, Form10_NonIdentity_WarnsButValid) {
    const TransformationMatrix tm(RotationZ(kPi / 4.0), Vector3d::Zero(), 10);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_GE(CountSeverity(result, ValidationSeverity::kWarning), 1);
}

// フォーム11: 円筒座標系の構造 [[c,-s,0],[s,c,0],[0,0,1]] → 警告なし
TEST(TransformationMatrixValidateTest, Form11_CylindricalStructure_NoWarning) {
    const TransformationMatrix tm(RotationZ(kPi / 3.0), Vector3d::Zero(), 11);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// フォーム11: 円筒構造に従わない (X軸回転で第3列が[0,0,1]でない) → 警告。is_valid=true
TEST(TransformationMatrixValidateTest, Form11_NonCylindricalStructure_WarnsButValid) {
    const TransformationMatrix tm(RotationX(kPi / 4.0), Vector3d::Zero(), 11);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_GE(CountSeverity(result, ValidationSeverity::kWarning), 1);
}

// フォーム12: 第3列の第3成分が0 (球座標構造) → 警告なし
TEST(TransformationMatrixValidateTest, Form12_ThirdComponentZero_NoWarning) {
    const TransformationMatrix tm(SphericalExampleRotation(), Vector3d::Zero(), 12);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty());
}

// フォーム12: 第3列の第3成分が0でない (単位行列) → 構造違反の警告。is_valid=true
TEST(TransformationMatrixValidateTest, Form12_ThirdComponentNonZero_WarnsButValid) {
    const TransformationMatrix tm(Matrix3d::Identity(), Vector3d::Zero(), 12);
    const auto result = tm.Validate();
    EXPECT_TRUE(result.is_valid);
    EXPECT_GE(CountSeverity(result, ValidationSeverity::kWarning), 1);
}



/**
 * 参照 (SetReference / GetRefTransformation)
 */

// GetRefTransformationは既定では参照なし (nullptr) を返す
TEST(TransformationMatrixReferenceTest, GetRefTransformation_DefaultsToNull) {
    const TransformationMatrix tm(Matrix3d::Identity());
    EXPECT_EQ(tm.GetRefTransformation(), nullptr);
}

// SetReferenceは参照を設定でき、GetRefTransformationで取得できる
TEST(TransformationMatrixReferenceTest, SetReference_StoresReference) {
    auto tm_a = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    std::shared_ptr<i_ent::ITransformation> tm_b =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());

    EXPECT_TRUE(tm_a->SetReference(tm_b));
    ASSERT_NE(tm_a->GetRefTransformation(), nullptr);
    EXPECT_EQ(tm_a->GetRefTransformation()->GetID(), tm_b->GetID());
}

// SetReference(nullptr) は参照を解除する
TEST(TransformationMatrixReferenceTest, SetReference_NullClearsReference) {
    auto tm_a = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    std::shared_ptr<i_ent::ITransformation> tm_b =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    ASSERT_TRUE(tm_a->SetReference(tm_b));
    ASSERT_NE(tm_a->GetRefTransformation(), nullptr);

    EXPECT_TRUE(tm_a->SetReference(std::shared_ptr<i_ent::ITransformation>(nullptr)));
    EXPECT_EQ(tm_a->GetRefTransformation(), nullptr);
}

// 自己参照 (this -> this) は循環として棄却され、参照は設定されない
// NOTE: SetReferenceはconst/非constの2オーバーロードを持つため、
//       引数はITransformationのshared_ptrとして渡し曖昧さを避ける
TEST(TransformationMatrixReferenceTest, SetReference_RejectsSelfReference) {
    std::shared_ptr<i_ent::ITransformation> tm =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    EXPECT_FALSE(tm->SetReference(tm));
    EXPECT_EQ(tm->GetRefTransformation(), nullptr);
}

// 2要素の循環 (a->b の後の b->a) は棄却され、bの参照は設定されない
TEST(TransformationMatrixReferenceTest, SetReference_RejectsTwoNodeCycle) {
    std::shared_ptr<i_ent::ITransformation> tm_a =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    std::shared_ptr<i_ent::ITransformation> tm_b =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    ASSERT_TRUE(tm_a->SetReference(tm_b));   // a -> b
    EXPECT_FALSE(tm_b->SetReference(tm_a));  // b -> a は循環
    EXPECT_EQ(tm_b->GetRefTransformation(), nullptr);
}

// 多段の循環 (a->b->c の後の c->a) は棄却される
TEST(TransformationMatrixReferenceTest, SetReference_RejectsMultiNodeCycle) {
    std::shared_ptr<i_ent::ITransformation> tm_a =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    std::shared_ptr<i_ent::ITransformation> tm_b =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    std::shared_ptr<i_ent::ITransformation> tm_c =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    ASSERT_TRUE(tm_a->SetReference(tm_b));   // a -> b
    ASSERT_TRUE(tm_b->SetReference(tm_c));   // b -> c
    EXPECT_FALSE(tm_c->SetReference(tm_a));  // c -> a は循環
    EXPECT_EQ(tm_c->GetRefTransformation(), nullptr);
}

// 循環しない参照チェーン (a->b->c) は設定でき、各参照を辿れる (近道辺の誤棄却なし)
TEST(TransformationMatrixReferenceTest, SetReference_AcyclicChainIsAccepted) {
    std::shared_ptr<i_ent::ITransformation> tm_a =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    std::shared_ptr<i_ent::ITransformation> tm_b =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    std::shared_ptr<i_ent::ITransformation> tm_c =
        std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    EXPECT_TRUE(tm_a->SetReference(tm_b));   // a -> b
    EXPECT_TRUE(tm_b->SetReference(tm_c));   // b -> c

    ASSERT_NE(tm_a->GetRefTransformation(), nullptr);
    EXPECT_EQ(tm_a->GetRefTransformation()->GetID(), tm_b->GetID());
    ASSERT_NE(tm_b->GetRefTransformation(), nullptr);
    EXPECT_EQ(tm_b->GetRefTransformation()->GetID(), tm_c->GetID());
}



/**
 * EntityBase::OverwriteTransformationMatrix (DEフィールド7) と循環参照の棄却
 * @note OverwriteTransformationMatrixはオーバーロードが1つのため、引数に
 *       shared_ptr<TransformationMatrix>をそのまま渡しても曖昧にならない。
 */

// OverwriteTransformationMatrixは変換行列を設定し、DEフィールド7に保持される
TEST(TransformationMatrixReferenceTest,
     OverwriteTransformationMatrix_StoresReference) {
    auto tm_a = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    auto tm_b = std::make_shared<TransformationMatrix>(Matrix3d::Identity());

    EXPECT_TRUE(tm_a->OverwriteTransformationMatrix(tm_b));
    const auto stored = tm_a->GetTransformationMatrix().GetPointer();
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->GetID(), tm_b->GetID());
}

// nullptrの場合はfalseを返し、参照は設定されない
TEST(TransformationMatrixReferenceTest,
     OverwriteTransformationMatrix_ReturnsFalseForNull) {
    auto tm = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    EXPECT_FALSE(tm->OverwriteTransformationMatrix(
        std::shared_ptr<const i_ent::ITransformation>(nullptr)));
    EXPECT_EQ(tm->GetTransformationMatrix().GetPointer(), nullptr);
}

// 自己参照 (this -> this) は循環として棄却され、参照は設定されない
TEST(TransformationMatrixReferenceTest,
     OverwriteTransformationMatrix_RejectsSelfReference) {
    auto tm = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    EXPECT_FALSE(tm->OverwriteTransformationMatrix(tm));
    EXPECT_EQ(tm->GetTransformationMatrix().GetPointer(), nullptr);
}

// 2要素の循環 (a->b の後の b->a) は棄却され、bの参照は設定されない
TEST(TransformationMatrixReferenceTest,
     OverwriteTransformationMatrix_RejectsTwoNodeCycle) {
    auto tm_a = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    auto tm_b = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    ASSERT_TRUE(tm_a->OverwriteTransformationMatrix(tm_b));   // a -> b
    EXPECT_FALSE(tm_b->OverwriteTransformationMatrix(tm_a));  // b -> a は循環
    EXPECT_EQ(tm_b->GetTransformationMatrix().GetPointer(), nullptr);
}

// SetReferenceで張った参照もOverwriteTransformationMatrixの循環判定で考慮される
// (両APIが同一の循環判定ロジックを共有することの確認)
TEST(TransformationMatrixReferenceTest,
     OverwriteTransformationMatrix_DetectsCycleAcrossApis) {
    auto tm_a = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    auto tm_b = std::make_shared<TransformationMatrix>(Matrix3d::Identity());
    // a -> b をSetReferenceで設定 (オーバーロード曖昧回避のため明示キャスト)
    ASSERT_TRUE(tm_a->SetReference(
        std::static_pointer_cast<i_ent::ITransformation>(tm_b)));
    // b -> a をOverwriteTransformationMatrixで設定 → 循環のため棄却
    EXPECT_FALSE(tm_b->OverwriteTransformationMatrix(tm_a));
    EXPECT_EQ(tm_b->GetTransformationMatrix().GetPointer(), nullptr);
}



/**
 * エラーケース: コンストラクタの例外型
 */

// 境界: パラメータ数が最小値12のすぐ外 (11個) はEntityParameterError
TEST(TransformationMatrixErrorTest,
     Constructor_ThrowsEntityParameterErrorWhenTooFewParams) {
    const auto param = igesio::IGESParameterVector{
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0  // 11個 (T3が欠落)
    };
    EXPECT_THROW(TransformationMatrix tm(
        i_ent::RawEntityDE::ByDefault(
            i_ent::EntityType::kTransformationMatrix),
        param), igesio::EntityParameterError);
}
