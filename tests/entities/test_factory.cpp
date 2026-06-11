/**
 * @file entities/test_factory.cpp
 * @brief entities/factory.h (EntityFactoryの登録API) のテスト
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `EntityFactory::RegisterEntityCreator`
 *       - `EntityFactory::UnregisterEntityCreator`
 *       - `EntityFactory::IsEntityCreatorRegistered`
 *       - `EntityFactory::RegisterUserEntityCreator` (ユーザー定義番号)
 *       - `EntityFactory::UnregisterUserEntityCreator`
 *       - `EntityFactory::IsUserEntityCreatorRegistered`
 *       - `EntityFactory::CreateEntity` (ユーザー登録経路・kUserDefined経路)
 *       - `CloneEntity` (ユーザー登録creator経由の複製・実番号の複製)
 *       - `EntityBase::GetTypeNumber`
 * @note TODO: type番号の数値境界の網羅はenum型の引数で構造的に制限されるため
 *       対象外とし、無効番号の代表値 (99, 101) のみ検証する.
 *       組み込みcreator自体の生成結果は各エンティティのテストで担保済みのため、
 *       ここでは検証しない.
 * @note おそらく最後まで実装しないであろうMACRO Definition (Type 306) を、
 *       ユーザー登録のテスト用のダミー実装として用いる
 */
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "igesio/common/errors.h"
#include "igesio/entities/factory.h"
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/structures/unsupported_entity.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::IGESParameterVector;
using i_ent::EntityFactory;
using i_ent::EntityType;

/// @brief 登録テスト用のダミーエンティティ (Type 306; MACRO Definition)
/// @note 形状の実装は持たず、「ユーザー定義クラスがファクトリ経由で生成される」
///       ことの確認にのみ用いる. PDパラメータの保持はUnsupportedEntityに委ねる
class DummyMacroDefinition : public i_ent::UnsupportedEntity {
 public:
    /// @brief コンストラクタ (CreateFunctionと同順の引数)
    DummyMacroDefinition(const i_ent::RawEntityDE& de,
                         const IGESParameterVector& params,
                         const igesio::pointer2ID& de2id,
                         const igesio::ObjectID& iges_id)
        : i_ent::UnsupportedEntity(de, params, de2id, iges_id) {}

    /// @brief ユーザー実装としてサポート済み扱いとする
    bool IsSupported() const override { return true; }
};

/// @brief DummyMacroDefinitionを生成する作成関数
EntityFactory::CreateFunction MakeDummyCreator() {
    return [](const i_ent::RawEntityDE& de, const IGESParameterVector& p,
              const igesio::pointer2ID& d2i, const igesio::ObjectID& iid) {
        return std::static_pointer_cast<i_ent::EntityBase>(
                std::make_shared<DummyMacroDefinition>(de, p, d2i, iid));
    };
}

/// @brief Type 306 (MACRO Definition) のデフォルトDEレコードを生成する
i_ent::RawEntityDE MacroDefinitionDE() {
    return i_ent::RawEntityDE::ByDefault(EntityType::kMacroDefinition);
}

/// @brief Type 306のPDパラメータを生成する
/// @note ダミー実装はパラメータを解釈しないため、内容は任意の数値列でよい
IGESParameterVector MacroDefinitionParams() {
    return IGESParameterVector{1.0, 2.0, 3.0};
}

/// @brief スコープ終了時にユーザー登録を解除するガード
/// @note テスト間でstaticなレジストリ状態が漏れないようにする
class CreatorGuard {
 public:
    /// @brief コンストラクタ
    /// @param type 解除対象のエンティティタイプ
    explicit CreatorGuard(const EntityType type) : type_(type) {}
    /// @brief デストラクタ (登録を解除する)
    ~CreatorGuard() { EntityFactory::UnregisterEntityCreator(type_); }

 private:
    /// @brief 解除対象のエンティティタイプ
    EntityType type_;
};

}  // namespace



/**
 * 正常系
 */

// 未登録の未実装タイプはUnsupportedEntityへフォールバックする (従来挙動)
TEST(EntityFactoryRegistrationTest, CreateEntity_ReturnsUnsupportedForUnregisteredType) {
    auto entity = EntityFactory::CreateEntity(
            MacroDefinitionDE(), MacroDefinitionParams(), {});
    ASSERT_NE(entity, nullptr);
    EXPECT_FALSE(entity->IsSupported());
    EXPECT_EQ(entity->GetType(), EntityType::kMacroDefinition);
    EXPECT_EQ(std::dynamic_pointer_cast<DummyMacroDefinition>(entity), nullptr);
}

// 登録後はCreateEntityがユーザー定義クラスを返す
TEST(EntityFactoryRegistrationTest, RegisterEntityCreator_CreatesUserEntity) {
    CreatorGuard guard(EntityType::kMacroDefinition);
    EntityFactory::RegisterEntityCreator(
            EntityType::kMacroDefinition, MakeDummyCreator());

    auto entity = EntityFactory::CreateEntity(
            MacroDefinitionDE(), MacroDefinitionParams(), {});
    ASSERT_NE(entity, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<DummyMacroDefinition>(entity), nullptr);
    EXPECT_TRUE(entity->IsSupported());
    EXPECT_EQ(entity->GetType(), EntityType::kMacroDefinition);
}

// ユーザー登録creator経由でCloneEntityが複製を生成できる
// (CloneEntityはDE/PDへのシリアライズ+CreateEntityの再構築で実装されるため、
//  登録された作成関数が複製経路でも引かれることを確認する)
TEST(EntityFactoryRegistrationTest, CloneEntity_UsesRegisteredCreator) {
    CreatorGuard guard(EntityType::kMacroDefinition);
    EntityFactory::RegisterEntityCreator(
            EntityType::kMacroDefinition, MakeDummyCreator());

    auto original = EntityFactory::CreateEntity(
            MacroDefinitionDE(), MacroDefinitionParams(), {});
    ASSERT_NE(original, nullptr);

    auto clone = i_ent::CloneEntity(*original);
    ASSERT_NE(clone, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<DummyMacroDefinition>(clone), nullptr);
    EXPECT_NE(clone->GetID(), original->GetID());  // 新IDが採番される
}

// Unregisterでユーザー登録を解除でき、フォールバック挙動へ戻る
TEST(EntityFactoryRegistrationTest, UnregisterEntityCreator_RemovesUserCreator) {
    EntityFactory::RegisterEntityCreator(
            EntityType::kMacroDefinition, MakeDummyCreator());

    EXPECT_TRUE(EntityFactory::UnregisterEntityCreator(EntityType::kMacroDefinition));
    // 解除後はUnsupportedEntityフォールバックへ戻る
    auto entity = EntityFactory::CreateEntity(
            MacroDefinitionDE(), MacroDefinitionParams(), {});
    EXPECT_FALSE(entity->IsSupported());
    // 既に解除済みのため2回目はfalse
    EXPECT_FALSE(EntityFactory::UnregisterEntityCreator(EntityType::kMacroDefinition));
}

// 組み込み実装は解除できない
TEST(EntityFactoryRegistrationTest, UnregisterEntityCreator_ReturnsFalseForBuiltin) {
    EXPECT_FALSE(EntityFactory::UnregisterEntityCreator(
            EntityType::kRationalBSplineCurve));
    // 組み込み実装は登録済みのまま
    EXPECT_TRUE(EntityFactory::IsEntityCreatorRegistered(
            EntityType::kRationalBSplineCurve));
}

// IsEntityCreatorRegisteredが組み込み・ユーザー登録の両方を反映する
TEST(EntityFactoryRegistrationTest, IsEntityCreatorRegistered_ReflectsRegistration) {
    // 組み込み実装
    EXPECT_TRUE(EntityFactory::IsEntityCreatorRegistered(EntityType::kCircularArc));
    // 未実装・未登録
    EXPECT_FALSE(EntityFactory::IsEntityCreatorRegistered(EntityType::kMacroDefinition));

    {
        CreatorGuard guard(EntityType::kMacroDefinition);
        EntityFactory::RegisterEntityCreator(
                EntityType::kMacroDefinition, MakeDummyCreator());
        EXPECT_TRUE(EntityFactory::IsEntityCreatorRegistered(
                EntityType::kMacroDefinition));
    }
    // ガードの解除後はfalseへ戻る
    EXPECT_FALSE(EntityFactory::IsEntityCreatorRegistered(EntityType::kMacroDefinition));
}



/**
 * 異常系
 */

// 組み込み実装済みタイプへの登録は拒否される
TEST(EntityFactoryRegistrationTest,
     RegisterEntityCreator_ThrowsInvalidArgumentWhenBuiltinType) {
    EXPECT_THROW(EntityFactory::RegisterEntityCreator(
            EntityType::kRationalBSplineCurve, MakeDummyCreator()),
            std::invalid_argument);
}

// 登録済みタイプへの再登録 (上書き) は拒否される
TEST(EntityFactoryRegistrationTest,
     RegisterEntityCreator_ThrowsInvalidArgumentWhenAlreadyRegistered) {
    CreatorGuard guard(EntityType::kMacroDefinition);
    EntityFactory::RegisterEntityCreator(
            EntityType::kMacroDefinition, MakeDummyCreator());

    EXPECT_THROW(EntityFactory::RegisterEntityCreator(
            EntityType::kMacroDefinition, MakeDummyCreator()),
            std::invalid_argument);
}

// 空の作成関数は拒否される
TEST(EntityFactoryRegistrationTest,
     RegisterEntityCreator_ThrowsInvalidArgumentWhenCreatorIsEmpty) {
    EXPECT_THROW(EntityFactory::RegisterEntityCreator(
            EntityType::kMacroDefinition, nullptr),
            std::invalid_argument);
}

// enumに存在しない値へのキャストで作られた無効なtype番号は拒否される
// (99: 有効範囲(100-)の直外, 101: 範囲内だが無効な奇数)
TEST(EntityFactoryRegistrationTest,
     RegisterEntityCreator_ThrowsInvalidArgumentWhenInvalidTypeNumber) {
    EXPECT_THROW(EntityFactory::RegisterEntityCreator(
            static_cast<EntityType>(99), MakeDummyCreator()),
            std::invalid_argument);
    EXPECT_THROW(EntityFactory::RegisterEntityCreator(
            static_cast<EntityType>(101), MakeDummyCreator()),
            std::invalid_argument);
}

// kUserDefinedの直接登録は拒否される (RegisterUserEntityCreatorを使う)
TEST(EntityFactoryRegistrationTest,
     RegisterEntityCreator_ThrowsInvalidArgumentWhenUserDefinedSentinel) {
    EXPECT_THROW(EntityFactory::RegisterEntityCreator(
            EntityType::kUserDefined, MakeDummyCreator()),
            std::invalid_argument);
}



/******************************************************************************
 * ユーザー定義番号 (kUserDefined; 600-699, 10000-99999) の登録・生成
 *****************************************************************************/

namespace {

/// @brief テストで使用するユーザー定義のtype番号
constexpr int kUserTypeNumber = 602;

/// @brief スコープ終了時にユーザー定義番号の登録を解除するガード
class UserCreatorGuard {
 public:
    /// @brief コンストラクタ
    /// @param type_number 解除対象のtype番号
    explicit UserCreatorGuard(const int type_number)
            : type_number_(type_number) {}
    /// @brief デストラクタ (登録を解除する)
    ~UserCreatorGuard() {
        EntityFactory::UnregisterUserEntityCreator(type_number_);
    }

 private:
    /// @brief 解除対象のtype番号
    int type_number_;
};

/// @brief ユーザー定義番号のDEレコードを生成する
i_ent::RawEntityDE UserDefinedDE(const int type_number = kUserTypeNumber) {
    return i_ent::RawEntityDE::ByDefaultUserDefined(type_number);
}

/// @brief EntityBaseのユーザー定義番号コンストラクタを直接利用する最小実装
/// @note コードからの直接構築経路 (EntityBase(int, ...)) の検証に用いる.
///       PDパラメータは解釈せず全てメインパラメータとして保持する
class MinimalUserEntity : public i_ent::EntityBase {
 public:
    /// @brief コンストラクタ
    /// @param type_number ユーザー定義のtype番号
    /// @param params PDレコードのパラメータ
    MinimalUserEntity(const int type_number, const IGESParameterVector& params)
            : EntityBase(type_number, params) {
        InitializePD({});
    }

    /// @brief PDレコードの検証 (テスト用のため常に成功)
    igesio::ValidationResult ValidatePD() const override {
        return igesio::ValidationResult::Success();
    }

 protected:
    /// @brief 全パラメータをメインパラメータとして返す
    IGESParameterVector GetMainPDParameters() const override {
        return pd_parameters_;
    }

    /// @brief 全パラメータをメインパラメータとして扱う
    size_t SetMainPDParameters(const igesio::pointer2ID&) override {
        return pd_parameters_.size();
    }
};

}  // namespace

// 正常系: 未登録のユーザー定義番号はUnsupportedEntityとして保持される
// (実番号はGetTypeNumber()で往復可能)
TEST(EntityFactoryUserDefinedTest, CreateEntity_ReturnsUnsupportedWhenUnregistered) {
    auto entity = EntityFactory::CreateEntity(
            UserDefinedDE(), IGESParameterVector{1.0, 2.0}, {});
    ASSERT_NE(entity, nullptr);
    EXPECT_FALSE(entity->IsSupported());
    EXPECT_EQ(entity->GetType(), EntityType::kUserDefined);
    EXPECT_EQ(entity->GetTypeNumber(), kUserTypeNumber);
}

// 正常系: 登録後はCreateEntityがユーザー定義クラスを返す
TEST(EntityFactoryUserDefinedTest, RegisterUserEntityCreator_CreatesUserEntity) {
    UserCreatorGuard guard(kUserTypeNumber);
    EntityFactory::RegisterUserEntityCreator(kUserTypeNumber, MakeDummyCreator());
    EXPECT_TRUE(EntityFactory::IsUserEntityCreatorRegistered(kUserTypeNumber));

    auto entity = EntityFactory::CreateEntity(
            UserDefinedDE(), IGESParameterVector{1.0, 2.0}, {});
    ASSERT_NE(entity, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<DummyMacroDefinition>(entity), nullptr);
    EXPECT_TRUE(entity->IsSupported());
    EXPECT_EQ(entity->GetType(), EntityType::kUserDefined);
    EXPECT_EQ(entity->GetTypeNumber(), kUserTypeNumber);
}

// 正常系: 登録creator経由のCloneEntityで複製でき、実番号も複製される
// (ToRawEntityPDのtype番号 (int) オーバーロード経路の確認を兼ねる)
TEST(EntityFactoryUserDefinedTest, CloneEntity_PreservesUserTypeNumber) {
    UserCreatorGuard guard(kUserTypeNumber);
    EntityFactory::RegisterUserEntityCreator(kUserTypeNumber, MakeDummyCreator());

    auto original = EntityFactory::CreateEntity(
            UserDefinedDE(), IGESParameterVector{1.0, 2.0}, {});
    ASSERT_NE(original, nullptr);

    auto clone = i_ent::CloneEntity(*original);
    ASSERT_NE(clone, nullptr);
    EXPECT_NE(std::dynamic_pointer_cast<DummyMacroDefinition>(clone), nullptr);
    EXPECT_EQ(clone->GetTypeNumber(), kUserTypeNumber);
    EXPECT_NE(clone->GetID(), original->GetID());
}

// 正常系: 未登録 (UnsupportedEntity) でもCloneEntityで実番号が複製される
TEST(EntityFactoryUserDefinedTest, CloneEntity_PreservesNumberForUnsupported) {
    auto original = EntityFactory::CreateEntity(
            UserDefinedDE(), IGESParameterVector{1.0, 2.0}, {});
    auto clone = i_ent::CloneEntity(*original);
    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->GetType(), EntityType::kUserDefined);
    EXPECT_EQ(clone->GetTypeNumber(), kUserTypeNumber);
}

// 正常系: 解除後はUnsupportedEntityフォールバックへ戻る
TEST(EntityFactoryUserDefinedTest, UnregisterUserEntityCreator_RemovesCreator) {
    EntityFactory::RegisterUserEntityCreator(kUserTypeNumber, MakeDummyCreator());
    EXPECT_TRUE(EntityFactory::UnregisterUserEntityCreator(kUserTypeNumber));
    EXPECT_FALSE(EntityFactory::IsUserEntityCreatorRegistered(kUserTypeNumber));
    EXPECT_FALSE(EntityFactory::UnregisterUserEntityCreator(kUserTypeNumber));

    auto entity = EntityFactory::CreateEntity(
            UserDefinedDE(), IGESParameterVector{1.0, 2.0}, {});
    EXPECT_FALSE(entity->IsSupported());
}

// 異常系: 予約範囲外の番号は拒否される (599/700: 600-699の直外,
// 9999/100000: 10000-99999の直外, 126: 組み込みのenum番号)
TEST(EntityFactoryUserDefinedTest,
     RegisterUserEntityCreator_ThrowsInvalidArgumentWhenOutOfRange) {
    EXPECT_THROW(EntityFactory::RegisterUserEntityCreator(599, MakeDummyCreator()),
                 std::invalid_argument);
    EXPECT_THROW(EntityFactory::RegisterUserEntityCreator(700, MakeDummyCreator()),
                 std::invalid_argument);
    EXPECT_THROW(EntityFactory::RegisterUserEntityCreator(9999, MakeDummyCreator()),
                 std::invalid_argument);
    EXPECT_THROW(EntityFactory::RegisterUserEntityCreator(100000, MakeDummyCreator()),
                 std::invalid_argument);
    EXPECT_THROW(EntityFactory::RegisterUserEntityCreator(126, MakeDummyCreator()),
                 std::invalid_argument);
}

// 異常系: 登録済み番号への再登録 (上書き) は拒否される
TEST(EntityFactoryUserDefinedTest,
     RegisterUserEntityCreator_ThrowsInvalidArgumentWhenAlreadyRegistered) {
    UserCreatorGuard guard(kUserTypeNumber);
    EntityFactory::RegisterUserEntityCreator(kUserTypeNumber, MakeDummyCreator());

    EXPECT_THROW(EntityFactory::RegisterUserEntityCreator(
            kUserTypeNumber, MakeDummyCreator()),
            std::invalid_argument);
}

// 異常系: 空の作成関数は拒否される
TEST(EntityFactoryUserDefinedTest,
     RegisterUserEntityCreator_ThrowsInvalidArgumentWhenCreatorIsEmpty) {
    EXPECT_THROW(EntityFactory::RegisterUserEntityCreator(kUserTypeNumber, nullptr),
                 std::invalid_argument);
}

namespace {

/// @brief 静的初期化子からの登録 (自動登録パターン)
/// @note ストレージが関数内static (BuiltinCreators/UserCreators) のため、
///       静的初期化順に依存せず安全に登録できることの検証. 番号699は
///       他のテストと干渉しない (プロセス終了まで登録されたままとなる)
const bool kStaticallyRegistered = [] {
    EntityFactory::RegisterUserEntityCreator(699, MakeDummyCreator());
    return true;
}();

}  // namespace

// 正常系: 静的初期化子からの登録が安全に成立している
TEST(EntityFactoryUserDefinedTest, StaticInitializationRegistrationIsSafe) {
    EXPECT_TRUE(kStaticallyRegistered);
    EXPECT_TRUE(EntityFactory::IsUserEntityCreatorRegistered(699));
}

// 正常系: EntityBaseのユーザー定義番号コンストラクタでコードから直接構築できる
TEST(EntityFactoryUserDefinedTest, ProgrammaticConstruction) {
    MinimalUserEntity entity(kUserTypeNumber, IGESParameterVector{1.0, 2.0});
    EXPECT_EQ(entity.GetType(), EntityType::kUserDefined);
    EXPECT_EQ(entity.GetTypeNumber(), kUserTypeNumber);
    EXPECT_EQ(entity.GetFormNumber(), 0);

    // GetRawEntityDEで実番号が復元されること (書き出し経路の前提)
    auto de = entity.GetRawEntityDE();
    EXPECT_EQ(de.entity_type, EntityType::kUserDefined);
    EXPECT_EQ(de.user_type_number, kUserTypeNumber);
    EXPECT_EQ(de.TypeNumber(), kUserTypeNumber);
}

// 異常系: 予約範囲外の番号での直接構築はDataFormatError
TEST(EntityFactoryUserDefinedTest,
     ProgrammaticConstruction_ThrowsDataFormatErrorWhenOutOfRange) {
    EXPECT_THROW(MinimalUserEntity(599, IGESParameterVector{}),
                 igesio::DataFormatError);
    EXPECT_THROW(MinimalUserEntity(700, IGESParameterVector{}),
                 igesio::DataFormatError);
}
