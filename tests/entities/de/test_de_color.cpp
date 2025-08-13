/**
 * @file entities/de/test_de_color.cpp
 * @brief entities/de/de_color.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-13
 * @copyright 2025 Yayoi Habami
 * @note entities/de/de_field_wrapper.hのテストも兼ねる
 */
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "igesio/common/errors.h"
#include "igesio/entities/de/de_color.h"

namespace {

namespace i_ent = igesio::entities;

/// @brief Mockクラス: IColorDefinitionのテスト用
class MockColorDefinition : public i_ent::IColorDefinition {
 private:
    uint64_t id_;
    std::string name_;
    std::array<double, 3> rgb_;

 public:
    explicit MockColorDefinition(uint64_t id,
                                 const std::string& name = "Custom Color",
                                 const std::array<double, 3>& rgb = {10.0, 20.0, 30.0})
        : id_(id), name_(name), rgb_(rgb) {}

    uint64_t GetID() const override { return id_; }
    int GetFormNumber() const override { return 0; }
    i_ent::EntityType GetType() const override {
        return i_ent::EntityType::kColorDefinition;
    }
    std::string GetColorName() const override { return name_; }
    std::array<double, 3> GetRGB() const override { return rgb_; }
};

}  // namespace



class DEColorTest : public ::testing::Test {
 protected:
    std::shared_ptr<i_ent::IColorDefinition> color_def_ptr_1;
    std::shared_ptr<i_ent::IColorDefinition> color_def_ptr_2;

    void SetUp() override {
        color_def_ptr_1 = std::make_shared<MockColorDefinition>(
                123, "Custom Red", std::array<double, 3>{80.0, 20.0, 20.0});
        color_def_ptr_2 = std::make_shared<MockColorDefinition>(
                456, "Custom Blue", std::array<double, 3>{20.0, 20.0, 80.0});
    }
};



/**
 * コンストラクタのテスト
 */

// コンストラクタのテスト
TEST_F(DEColorTest, DefaultConstructor) {
    i_ent::DEColor color;

    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, color.GetValueType());
    EXPECT_EQ(0, color.GetValue());
    EXPECT_EQ(igesio::kUnsetID, color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ(nullptr, color.GetPointer());

    // デフォルトの色は黒
    EXPECT_EQ((std::array<double, 3>{0.0, 0.0, 0.0}), color.GetRGB());
}

// IDを指定するコンストラクタのテスト
TEST_F(DEColorTest, ConstructorWithID) {
    uint64_t id = 123;
    i_ent::DEColor color(id);

    EXPECT_EQ(i_ent::DEFieldValueType::kPointer, color.GetValueType());
    EXPECT_EQ(id, color.GetID());

    // ポインタを受け入れる用意はあるが、未設定
    EXPECT_FALSE(color.HasValidPointer());

    // ポインタは未設定でもIDは取得可能
    EXPECT_EQ(std::optional<uint64_t>(id), color.GetUnsetID());
    EXPECT_EQ(color.GetValue(), -static_cast<int>(id));
}

// 色番号を指定するコンストラクタのテスト
TEST_F(DEColorTest, ConstructorWithColorNumber) {
    i_ent::DEColor color(i_ent::ColorNumber::kGreen);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, color.GetValueType());
    EXPECT_EQ(3, color.GetValue());
    EXPECT_EQ(igesio::kUnsetID, color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ((std::array<double, 3>{0.0, 100.0, 0.0}), color.GetRGB());
}

// int値を指定するコンストラクタのテスト
TEST_F(DEColorTest, ConstructorWithInt) {
    i_ent::DEColor color(4);  // Blue

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, color.GetValueType());
    EXPECT_EQ(4, color.GetValue());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ((std::array<double, 3>{0.0, 0.0, 100.0}), color.GetRGB());
}

// int値を指定するコンストラクタのテスト（無効値 = デフォルト値）
TEST_F(DEColorTest, ConstructorWithIntZeroIsDefault) {
    i_ent::DEColor color(0);  // NoColor

    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, color.GetValueType());
    EXPECT_EQ(0, color.GetValue());
    EXPECT_EQ(igesio::kUnsetID, color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ((std::array<double, 3>{0.0, 0.0, 0.0}), color.GetRGB());
}

// int値を指定するコンストラクタのテスト（異常値）
TEST_F(DEColorTest, ConstructorWithInvalidIntThrows) {
    // 負の値 (ポインタ) はint型としては与えられない
    EXPECT_THROW(i_ent::DEColor color(-1), std::invalid_argument);

    // 範囲外の値 (9以上) は無効
    EXPECT_THROW(i_ent::DEColor color(9), std::invalid_argument);
}



/**
 * ポインタ操作のテスト
 */

// ポインタを設定するテスト
TEST_F(DEColorTest, SetPointerSuccess) {
    // ID 123 で予約
    uint64_t id = 123;
    i_ent::DEColor color(id);

    // ポインタを設定
    color.SetPointer(color_def_ptr_1);
    EXPECT_TRUE(color.HasValidPointer());
    EXPECT_EQ(i_ent::DEFieldValueType::kPointer, color.GetValueType());
    EXPECT_EQ(color_def_ptr_1, color.GetPointer());
    EXPECT_EQ(123, color.GetID());
    EXPECT_EQ(std::nullopt, color.GetUnsetID());

    EXPECT_EQ(color_def_ptr_1->GetRGB(), color.GetRGB());
}

// ポインタを設定するテスト（異なるIDのポインタを指定）
TEST_F(DEColorTest, SetPointerThrowsOnIDMismatch) {
    // 異なるIDで予約
    uint64_t id = 999;
    i_ent::DEColor color(id);

    // 異なるIDのポインタを設定しようとすると例外を投げる
    EXPECT_THROW(color.SetPointer(color_def_ptr_1), std::invalid_argument);
}

TEST_F(DEColorTest, OverwritePointer) {
    // 正の値で初期化
    i_ent::DEColor color(i_ent::ColorNumber::kGreen);
    ASSERT_EQ(i_ent::DEFieldValueType::kPositive, color.GetValueType());

    // ポインタで上書きすると、タイプとIDが変更される
    color.OverwritePointer(color_def_ptr_1);

    EXPECT_EQ(i_ent::DEFieldValueType::kPointer, color.GetValueType());
    EXPECT_TRUE(color.HasValidPointer());
    EXPECT_EQ(123, color.GetID());
    EXPECT_EQ(color_def_ptr_1, color.GetPointer());
    EXPECT_EQ(color_def_ptr_1->GetRGB(), color.GetRGB());

    // 別のポインタで上書き
    color.OverwritePointer(color_def_ptr_2);

    EXPECT_EQ(456, color.GetID());
    EXPECT_EQ(color_def_ptr_2, color.GetPointer());
    EXPECT_EQ(color_def_ptr_2->GetRGB(), color.GetRGB());
}

// ポインタを上書きするテスト（nullptrを指定）
TEST_F(DEColorTest, OverwritePointerThrowsOnNull) {
    i_ent::DEColor color;
    std::shared_ptr<i_ent::IColorDefinition> null_ptr = nullptr;
    EXPECT_THROW(color.OverwritePointer(null_ptr), std::invalid_argument);
}



/**
 * IDと正値の操作のテスト
 */

// IDを上書きするテスト
TEST_F(DEColorTest, OverwriteIDInvalidatesPointer) {
    uint64_t id = 123;
    i_ent::DEColor color(id);
    color.SetPointer(color_def_ptr_1);
    ASSERT_TRUE(color.HasValidPointer());

    // IDを上書きすると、ポインタが無効化される
    uint64_t new_id = 789;
    color.OverwriteID(new_id);

    EXPECT_EQ(new_id, color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ(nullptr, color.GetPointer());

    // ID 789のポインタを受け入れる準備ができているはず
    EXPECT_EQ(std::optional<uint64_t>(new_id), color.GetUnsetID());
}

// 正の値を設定するテスト
TEST_F(DEColorTest, SetColorInvalidatesPointer) {
    uint64_t id = 123;
    i_ent::DEColor color(id);
    color.SetPointer(color_def_ptr_1);
    ASSERT_TRUE(color.HasValidPointer());

    // 正の値を設定すると、ポインタが無効化される
    color.SetColor(i_ent::ColorNumber::kMagenta);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, color.GetValueType());
    EXPECT_EQ(6, color.GetValue());
    EXPECT_EQ(igesio::kUnsetID, color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ((std::array<double, 3>{100.0, 0.0, 100.0}), color.GetRGB());
}



/**
 * 色の取得と変換のテスト
 */

// GetValue (IGESファイル上のDEフィールドでの値) 取得のテスト
TEST_F(DEColorTest, GetValueIntegrity) {
    // Default -> 0
    i_ent::DEColor color_default;
    EXPECT_EQ(0, color_default.GetValue());

    // Positive -> 対応する正の値
    i_ent::DEColor color_positive(i_ent::ColorNumber::kYellow);
    EXPECT_EQ(5, color_positive.GetValue());

    // Pointer (ID予約済み) -> IDの負の値
    uint64_t id = 123;
    i_ent::DEColor color_pointer_reserved(id);
    EXPECT_EQ(color_pointer_reserved.GetValue(),
              -static_cast<int>(id));

    // Pointer (ポインタ設定済み) -> IDの負の値
    i_ent::DEColor color_pointer_set(id);
    color_pointer_set.SetPointer(color_def_ptr_1);
    EXPECT_EQ(color_pointer_set.GetValue(),
              -static_cast<int>(id));
}

// GetCMYのテスト
TEST_F(DEColorTest, GetCMY) {
    // 正の値を設定 -> RGB(0, 100, 100)
    i_ent::DEColor color_positive(i_ent::ColorNumber::kCyan);
    EXPECT_EQ((std::array<double, 3>{100.0, 0.0, 0.0}), color_positive.GetCMY());

    // ポインタを設定 -> RGB(80, 20, 20)
    uint64_t id = 123;
    i_ent::DEColor color_pointer(id);
    color_pointer.SetPointer(color_def_ptr_1);
    EXPECT_EQ((std::array<double, 3>{20.0, 80.0, 80.0}), color_pointer.GetCMY());
}


/**
 * リセットのテスト
 */

TEST_F(DEColorTest, Reset) {
    // ポインタを設定している状態から開始
    uint64_t id = 123;
    i_ent::DEColor color(id);
    color.SetPointer(color_def_ptr_1);
    ASSERT_TRUE(color.HasValidPointer());

    color.Reset();

    // リセット後、デフォルトコンストラクタと同じ状態になるはず
    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, color.GetValueType());
    EXPECT_EQ(0, color.GetValue());
    EXPECT_EQ(igesio::kUnsetID, color.GetID());
    EXPECT_FALSE(color.HasValidPointer());

    // 正の値を設定している状態から開始
    i_ent::DEColor color2(i_ent::ColorNumber::kRed);
    ASSERT_EQ(i_ent::DEFieldValueType::kPositive, color2.GetValueType());

    color2.Reset();

    // 同様にデフォルト値にリセットされるはず
    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, color2.GetValueType());
    EXPECT_EQ(0, color2.GetValue());
}
