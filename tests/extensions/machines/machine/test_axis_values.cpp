/**
 * @file tests/extensions/machines/machine/test_axis_values.cpp
 * @brief 軸の値の表現 (machine/axis_values) のテスト
 * @author Yayoi Habami
 * @date 2026-09-10
 * @copyright 2026 Yayoi Habami
 * @note 対象: JointVector (構築子3種 / Size / Empty / operator[] / Values / ==) と
 *       NcValues (初期化子リスト構築 / Get / GetOr / At / Contains / Set / Merge /
 *       Size / Empty / Entries / ==)
 *       - 正常系 (代表値): 長さ指定と配列からの構築、添字の読み書き、
 *         初期化子リストの重複キーは後勝ち、`Set`は位置を保って上書き、
 *         `Merge`は重ねる側が勝ち新規軸は末尾、`==`は順序を無視
 *       - 正常系 (境界値・退化): 既定構築の空、`{}`からの構築、長さ0同士の一致
 *       - 異常系: 未指定の軸で`At`が`std::out_of_range`
 *       TODO: `JointVector`の異常系は該当なし (添字は境界検査を持たない設計で,
 *       軸数の検査は運動学関数の入口で行う. `test_forward_kinematics.cpp`で検証)
 * @note 運動学モデルとの結合 (`JointsFromNc`の反復順等) は
 *       `test_forward_kinematics.cpp`側にある.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/extensions/machines/machine/axis_values.h"

namespace {

namespace mc = igesio::extensions::machines;

/// @brief 項目の軸名を並べる (反復順の検証用)
std::vector<std::string> Names(const mc::NcValues& nc) {
    std::vector<std::string> names;
    for (const mc::NcEntry& entry : nc.Entries()) names.push_back(entry.register_name);
    return names;
}

}  // namespace



// ---- JointVector ----

TEST(AxisValuesTest, JointVector_DefaultIsEmpty) {
    const mc::JointVector q;
    EXPECT_EQ(q.Size(), 0u);
    EXPECT_TRUE(q.Empty());
    EXPECT_TRUE(q.Values().empty());
}

TEST(AxisValuesTest, JointVector_SizeConstructorFillsValue) {
    const mc::JointVector zeros(3);
    ASSERT_EQ(zeros.Size(), 3u);
    for (const double value : zeros.Values()) EXPECT_DOUBLE_EQ(value, 0.0);

    const mc::JointVector filled(4, 1.5);
    ASSERT_EQ(filled.Size(), 4u);
    EXPECT_FALSE(filled.Empty());
    for (std::size_t i = 0; i < filled.Size(); ++i) EXPECT_DOUBLE_EQ(filled[i], 1.5);
}

TEST(AxisValuesTest, JointVector_ExplicitFromVectorKeepsOrder) {
    const std::vector<double> raw = {1.0, 2.0, 3.0};
    const mc::JointVector q(raw);
    ASSERT_EQ(q.Size(), raw.size());
    ASSERT_EQ(q.Values().size(), raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        EXPECT_DOUBLE_EQ(q[i], raw[i]);
        EXPECT_DOUBLE_EQ(q.Values()[i], raw[i]);
    }
}

TEST(AxisValuesTest, JointVector_IndexIsWritable) {
    mc::JointVector q(3);
    q[1] = 7.0;
    EXPECT_DOUBLE_EQ(q[1], 7.0);
    EXPECT_DOUBLE_EQ(q.Values()[1], 7.0);
    EXPECT_DOUBLE_EQ(q[0], 0.0);
    EXPECT_DOUBLE_EQ(q[2], 0.0);
}

TEST(AxisValuesTest, JointVector_EqualityIsExact) {
    const mc::JointVector a(std::vector<double>{1.0, 2.0});
    const mc::JointVector same(std::vector<double>{1.0, 2.0});
    const mc::JointVector differs(std::vector<double>{1.0, 2.0 + 1e-15});
    const mc::JointVector longer(std::vector<double>{1.0, 2.0, 0.0});
    EXPECT_TRUE(a == same);
    EXPECT_FALSE(a != same);
    EXPECT_TRUE(a != differs);
    EXPECT_TRUE(a != longer);
    EXPECT_TRUE(mc::JointVector() == mc::JointVector());
}



// ---- NcValues ----

TEST(AxisValuesTest, NcValues_EmptyLiteral) {
    const mc::NcValues defaulted;
    const mc::NcValues braced = {};
    EXPECT_TRUE(defaulted.Empty());
    EXPECT_TRUE(braced.Empty());
    EXPECT_EQ(braced.Size(), 0u);
    EXPECT_TRUE(defaulted == braced);
}

TEST(AxisValuesTest, NcValues_InitializerListLaterWins) {
    const mc::NcValues nc = {{"A", 1.0}, {"C", 2.0}, {"A", 3.0}};
    EXPECT_EQ(nc.Size(), 2u);
    EXPECT_DOUBLE_EQ(nc.At("A"), 3.0);
    EXPECT_DOUBLE_EQ(nc.At("C"), 2.0);
    // 上書きしても位置は最初に現れた場所のまま
    EXPECT_EQ(Names(nc), (std::vector<std::string>{"A", "C"}));
}

TEST(AxisValuesTest, NcValues_GetAndGetOr) {
    const mc::NcValues nc = {{"X", 10.0}};
    ASSERT_TRUE(nc.Get("X").has_value());
    EXPECT_DOUBLE_EQ(*nc.Get("X"), 10.0);
    EXPECT_FALSE(nc.Get("Y").has_value());
    EXPECT_DOUBLE_EQ(nc.GetOr("X", -1.0), 10.0);
    EXPECT_DOUBLE_EQ(nc.GetOr("Y", -1.0), -1.0);
    EXPECT_TRUE(nc.Contains("X"));
    EXPECT_FALSE(nc.Contains("Y"));
    EXPECT_FALSE(nc.Contains(""));
}

TEST(AxisValuesTest, At_ThrowsOutOfRangeWhenRegisterIsMissing) {
    const mc::NcValues nc = {{"X", 10.0}};
    EXPECT_NO_THROW(nc.At("X"));
    EXPECT_THROW(nc.At("Y"), std::out_of_range);
    try {
        nc.At("Y");
        FAIL() << "expected std::out_of_range";
    } catch (const std::out_of_range& e) {
        EXPECT_NE(std::string(e.what()).find("Y"), std::string::npos);
    }
}

TEST(AxisValuesTest, NcValues_SetOverwritesInPlace) {
    mc::NcValues nc = {{"X", 1.0}, {"Y", 2.0}, {"Z", 3.0}};
    nc.Set("Y", 20.0);
    EXPECT_EQ(nc.Size(), 3u);
    EXPECT_DOUBLE_EQ(nc.At("Y"), 20.0);
    EXPECT_EQ(Names(nc), (std::vector<std::string>{"X", "Y", "Z"}));
    nc.Set("A", 4.0);   // 新規軸は末尾
    EXPECT_EQ(nc.Size(), 4u);
    EXPECT_EQ(Names(nc), (std::vector<std::string>{"X", "Y", "Z", "A"}));
    EXPECT_DOUBLE_EQ(nc.Entries().back().value, 4.0);
}

TEST(AxisValuesTest, NcValues_MergeOverlayWins) {
    mc::NcValues base = {{"X", 1.0}, {"Y", 2.0}};
    const mc::NcValues overlay = {{"Y", 20.0}, {"A", 30.0}};
    base.Merge(overlay);
    EXPECT_EQ(base.Size(), 3u);
    EXPECT_DOUBLE_EQ(base.At("X"), 1.0);
    EXPECT_DOUBLE_EQ(base.At("Y"), 20.0);
    EXPECT_DOUBLE_EQ(base.At("A"), 30.0);
    EXPECT_EQ(Names(base), (std::vector<std::string>{"X", "Y", "A"}));
    // 重ねる側は変化しない
    EXPECT_EQ(overlay.Size(), 2u);
    EXPECT_EQ(Names(overlay), (std::vector<std::string>{"Y", "A"}));

    mc::NcValues empty;
    empty.Merge(overlay);
    EXPECT_TRUE(empty == overlay);
    base.Merge(mc::NcValues{});
    EXPECT_EQ(base.Size(), 3u);
}

TEST(AxisValuesTest, NcValues_EqualityIgnoresOrder) {
    const mc::NcValues ac = {{"A", 1.0}, {"C", 2.0}};
    const mc::NcValues ca = {{"C", 2.0}, {"A", 1.0}};
    EXPECT_TRUE(ac == ca);
    EXPECT_FALSE(ac != ca);
    EXPECT_TRUE(ac != mc::NcValues({{"A", 1.0}, {"C", 2.0 + 1e-15}}));   // 値違い
    EXPECT_TRUE(ac != mc::NcValues({{"A", 1.0}, {"B", 2.0}}));           // 軸違い
    EXPECT_TRUE(ac != mc::NcValues({{"A", 1.0}}));                       // 要素数違い
    EXPECT_TRUE(ac != mc::NcValues({{"A", 1.0}, {"C", 2.0}, {"X", 0.0}}));
}
