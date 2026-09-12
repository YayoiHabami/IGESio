/**
 * @file entities/de/test_de_color.cpp
 * @brief entities/de/de_color.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-13
 * @copyright 2025 Yayoi Habami
 * @note entities/de/de_field_wrapper.hのテストも兼ねる
 */
#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>

#include "igesio/common/color.h"
#include "igesio/common/errors.h"
#include "igesio/entities/de/de_color.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Color;

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief Mockクラス: IColorDefinitionのテスト用
class MockColorDefinition : public i_ent::IColorDefinition {
 private:
    igesio::ObjectID id_;
    std::string name_;
    Color rgb_;

 public:
    explicit MockColorDefinition(const igesio::ObjectID& id,
                                 const std::string& name = "Custom Color",
                                 const Color& rgb = Color{0.1, 0.2, 0.3})
        : id_(id), name_(name), rgb_(rgb) {}

    const igesio::ObjectID& GetID() const override { return id_; }
    int GetFormNumber() const override { return 0; }
    i_ent::EntityType GetType() const override {
        return i_ent::EntityType::kColorDefinition;
    }
    std::string GetColorName() const override { return name_; }
    Color GetRGB() const override { return rgb_; }
};

}  // namespace



class DEColorTest : public ::testing::Test {
 protected:
    std::shared_ptr<i_ent::IColorDefinition> color_def_ptr_1;
    std::shared_ptr<i_ent::IColorDefinition> color_def_ptr_2;
    igesio::ObjectID id_1;
    igesio::ObjectID id_2;

    void SetUp() override {
        id_1 = igesio::IDGenerator::Generate(
                igesio::ObjectType::kEntityNew,
                static_cast<uint16_t>(i_ent::EntityType::kColorDefinition));
        id_2 = igesio::IDGenerator::Generate(
                igesio::ObjectType::kEntityNew,
                static_cast<uint16_t>(i_ent::EntityType::kColorDefinition));

        color_def_ptr_1 = std::make_shared<MockColorDefinition>(
                id_1, "Custom Red", Color{0.8, 0.2, 0.2});
        color_def_ptr_2 = std::make_shared<MockColorDefinition>(
                id_2, "Custom Blue", Color{0.2, 0.2, 0.8});
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
    EXPECT_EQ(igesio::IDGenerator::UnsetID(), color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ(nullptr, color.GetPointer());

    // デフォルトの色は黒
    EXPECT_EQ(Color{}, color.GetRGB());
}

// IDを指定するコンストラクタのテスト
TEST_F(DEColorTest, ConstructorWithID) {
    i_ent::DEColor color(id_1);

    EXPECT_EQ(i_ent::DEFieldValueType::kPointer, color.GetValueType());
    EXPECT_EQ(id_1, color.GetID());

    // ポインタを受け入れる用意はあるが、未設定
    EXPECT_FALSE(color.HasValidPointer());

    // ポインタは未設定でもIDは取得可能
    EXPECT_EQ(std::optional<igesio::ObjectID>(id_1), color.GetUnsetID());
    EXPECT_EQ(color.GetValue(), -id_1.ToInt());
}

// 色番号を指定するコンストラクタのテスト
TEST_F(DEColorTest, ConstructorWithColorNumber) {
    i_ent::DEColor color(i_ent::ColorNumber::kGreen);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, color.GetValueType());
    EXPECT_EQ(3, color.GetValue());
    EXPECT_EQ(igesio::IDGenerator::UnsetID(), color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ((Color{0.0, 1.0, 0.0}), color.GetRGB());
}

// int値を指定するコンストラクタのテスト
TEST_F(DEColorTest, ConstructorWithInt) {
    i_ent::DEColor color(4);  // Blue

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, color.GetValueType());
    EXPECT_EQ(4, color.GetValue());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ((Color{0.0, 0.0, 1.0}), color.GetRGB());
}

// int値を指定するコンストラクタのテスト（無効値 = デフォルト値）
TEST_F(DEColorTest, ConstructorWithIntZeroIsDefault) {
    i_ent::DEColor color(0);  // NoColor

    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, color.GetValueType());
    EXPECT_EQ(0, color.GetValue());
    EXPECT_EQ(igesio::IDGenerator::UnsetID(), color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ(Color{}, color.GetRGB());
}

// int値を指定するコンストラクタのテスト（異常値）
TEST_F(DEColorTest, ConstructorWithInvalidIntThrows) {
    // 負の値 (ポインタ) はint型としては与えられない
    EXPECT_THROW(i_ent::DEColor color(-1), igesio::DataFormatError);

    // 範囲外の値 (9以上) は無効
    EXPECT_THROW(i_ent::DEColor color(9), igesio::DataFormatError);

    // 境界: 有効範囲の内側 (8 = White) は例外を投げない
    EXPECT_NO_THROW(i_ent::DEColor color(8));
}



/**
 * ポインタ操作のテスト
 */

// ポインタを設定するテスト
TEST_F(DEColorTest, SetPointerSuccess) {
    i_ent::DEColor color(id_1);

    // ポインタを設定
    color.SetPointer(color_def_ptr_1);
    EXPECT_TRUE(color.HasValidPointer());
    EXPECT_EQ(i_ent::DEFieldValueType::kPointer, color.GetValueType());
    EXPECT_EQ(color_def_ptr_1, color.GetPointer());
    EXPECT_EQ(id_1, color.GetID());
    EXPECT_EQ(std::nullopt, color.GetUnsetID());

    EXPECT_EQ(color_def_ptr_1->GetRGB(), color.GetRGB());
}

// ポインタを設定するテスト（異なるIDのポインタを指定）
TEST_F(DEColorTest, SetPointerThrowsOnIDMismatch) {
    // 異なるIDで予約
    i_ent::DEColor color(id_2);

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
    EXPECT_EQ(id_1, color.GetID());
    EXPECT_EQ(color_def_ptr_1, color.GetPointer());
    EXPECT_EQ(color_def_ptr_1->GetRGB(), color.GetRGB());

    // 別のポインタで上書き
    color.OverwritePointer(color_def_ptr_2);

    EXPECT_EQ(id_2, color.GetID());
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
    i_ent::DEColor color(id_1);
    color.SetPointer(color_def_ptr_1);
    ASSERT_TRUE(color.HasValidPointer());

    // IDを上書きすると、ポインタが無効化される
    color.OverwriteID(id_2);

    EXPECT_EQ(id_2, color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ(nullptr, color.GetPointer());

    // ID 789のポインタを受け入れる準備ができているはず
    EXPECT_EQ(std::optional<igesio::ObjectID>(id_2), color.GetUnsetID());
}

// 正の値を設定するテスト
TEST_F(DEColorTest, SetColorInvalidatesPointer) {
    i_ent::DEColor color(id_1);
    color.SetPointer(color_def_ptr_1);
    ASSERT_TRUE(color.HasValidPointer());

    // 正の値を設定すると、ポインタが無効化される
    color.SetColor(i_ent::ColorNumber::kMagenta);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, color.GetValueType());
    EXPECT_EQ(6, color.GetValue());
    EXPECT_EQ(igesio::IDGenerator::UnsetID(), color.GetID());
    EXPECT_FALSE(color.HasValidPointer());
    EXPECT_EQ((Color{1.0, 0.0, 1.0}), color.GetRGB());
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
    i_ent::DEColor color_pointer_reserved(id_1);
    EXPECT_EQ(color_pointer_reserved.GetValue(), -id_1.ToInt());

    // Pointer (ポインタ設定済み) -> IDの負の値
    i_ent::DEColor color_pointer_set(id_1);
    color_pointer_set.SetPointer(color_def_ptr_1);
    EXPECT_EQ(color_pointer_set.GetValue(), -id_1.ToInt());
}

// GetCMYのテスト
TEST_F(DEColorTest, GetCMY) {
    // 正の値を設定 -> RGB(0, 1, 1)
    i_ent::DEColor color_positive(i_ent::ColorNumber::kCyan);
    const auto cmy_positive = color_positive.GetCMY();
    EXPECT_NEAR(cmy_positive[0], 1.0, kTol);
    EXPECT_NEAR(cmy_positive[1], 0.0, kTol);
    EXPECT_NEAR(cmy_positive[2], 0.0, kTol);

    // ポインタを設定 -> RGB(0.8, 0.2, 0.2)
    i_ent::DEColor color_pointer(id_1);
    color_pointer.SetPointer(color_def_ptr_1);
    const auto cmy_pointer = color_pointer.GetCMY();
    EXPECT_NEAR(cmy_pointer[0], 0.2, kTol);
    EXPECT_NEAR(cmy_pointer[1], 0.8, kTol);
    EXPECT_NEAR(cmy_pointer[2], 0.8, kTol);
}


/**
 * リセットのテスト
 */

TEST_F(DEColorTest, Reset) {
    // ポインタを設定している状態から開始
    i_ent::DEColor color(id_1);
    color.SetPointer(color_def_ptr_1);
    ASSERT_TRUE(color.HasValidPointer());

    color.Reset();

    // リセット後、デフォルトコンストラクタと同じ状態になるはず
    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, color.GetValueType());
    EXPECT_EQ(0, color.GetValue());
    EXPECT_EQ(igesio::IDGenerator::UnsetID(), color.GetID());
    EXPECT_FALSE(color.HasValidPointer());

    // 正の値を設定している状態から開始
    i_ent::DEColor color2(i_ent::ColorNumber::kRed);
    ASSERT_EQ(i_ent::DEFieldValueType::kPositive, color2.GetValueType());

    color2.Reset();

    // 同様にデフォルト値にリセットされるはず
    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, color2.GetValueType());
    EXPECT_EQ(0, color2.GetValue());
}



/**
 * ToColor / ClosestColorNumber のテスト
 */

// 標準色8色 (kBlack〜kWhite) とkNoColorの対応
TEST(ColorNumberConversionTest, ToColor_StandardColors) {
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kNoColor), Color{});
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kBlack), Color{});
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kRed), (Color{1.0, 0.0, 0.0}));
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kGreen), (Color{0.0, 1.0, 0.0}));
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kBlue), (Color{0.0, 0.0, 1.0}));
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kYellow), (Color{1.0, 1.0, 0.0}));
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kMagenta), (Color{1.0, 0.0, 1.0}));
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kCyan), (Color{0.0, 1.0, 1.0}));
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kWhite), (Color{1.0, 1.0, 1.0}));
    // 標準色は不透明
    EXPECT_EQ(i_ent::ToColor(i_ent::ColorNumber::kRed).a, 1.0);
}

// 標準色8色はすべて自分自身に写像される
TEST(ColorNumberConversionTest, ClosestColorNumber_PureStandardColors) {
    for (int i = 1; i <= 8; ++i) {
        const auto number = static_cast<i_ent::ColorNumber>(i);
        EXPECT_EQ(i_ent::ClosestColorNumber(i_ent::ToColor(number)), number)
                << "color number " << i;
    }
}

// 標準色近傍の色は最も近い標準色へ写像される
TEST(ColorNumberConversionTest, ClosestColorNumber_NearColors) {
    EXPECT_EQ(i_ent::ClosestColorNumber(Color{0.9, 0.9, 0.9}),
              i_ent::ColorNumber::kWhite);
    EXPECT_EQ(i_ent::ClosestColorNumber(Color{0.1, 0.1, 0.1}),
              i_ent::ColorNumber::kBlack);
    EXPECT_EQ(i_ent::ClosestColorNumber(Color{0.9, 0.05, 0.05}),
              i_ent::ColorNumber::kRed);
}

// 同距離の場合は番号の小さい標準色が選ばれる (中間灰色は全8色から等距離)
TEST(ColorNumberConversionTest, ClosestColorNumber_TieBreaksToSmallerNumber) {
    EXPECT_EQ(i_ent::ClosestColorNumber(Color{0.5, 0.5, 0.5}),
              i_ent::ColorNumber::kBlack);
}

// α成分は距離に影響しない
TEST(ColorNumberConversionTest, ClosestColorNumber_IgnoresAlpha) {
    EXPECT_EQ(i_ent::ClosestColorNumber(Color{0.9, 0.9, 0.9, 0.0}),
              i_ent::ColorNumber::kWhite);
}
