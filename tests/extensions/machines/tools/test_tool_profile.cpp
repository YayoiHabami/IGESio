/**
 * @file tests/extensions/machines/tools/test_tool_profile.cpp
 * @brief machines拡張の工具輪郭 (tools/tool_profile・tools/tool_assembly) のテスト
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeSimpleToolProfile / ValidateToolProfile / CloseElementOnAxis /
 *       CuttingLength / Reach / MaxRadius / PartExtent / GaugeLength /
 *       各Parse*・*Name / ControlLocal / ToolMountOffset
 *       - 簡易アセンブリ: ball・square・radiusの母線構成、シャンクの有無、
 *         指令点、ゲージライン、派生値、警告 (ホルダが切れ刃に被る)、
 *         幾何が成立しない寸法の例外
 *       - 検証: 簡易輪郭の受理と、各検証項目の違反ごとの例外文言
 *       - 派生値: 円弧の張り出し (主要角の通過) を含むこと
 *       - ゲージライン→ホルダ上端→全長の順の採用と警告、
 *         差し込み部分 (テーパ) を持つホルダでの値
 *       - 制御点: kTip/kGaugeとG43の扱い、取り付けオフセット
 */
#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Vector2d;
using igesio::Vector3d;
using mc::ProfileSegment;
using mc::SimpleToolSpec;
using mc::ToolPart;
using mc::ToolProfile;
using mc::ToolProfileElement;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 例外メッセージに指定語が含まれることを検証しつつ呼び出す
/// @param fn 例外を投げるはずの処理
/// @param keyword メッセージに含まれるべき語
template <class Fn>
void ExpectInvalidArgumentContaining(Fn fn, const std::string& keyword) {
    try {
        fn();
        FAIL() << "std::invalid_argument was not thrown (expected: " << keyword << ")";
    } catch (const std::invalid_argument& e) {
        EXPECT_NE(std::string(e.what()).find(keyword), std::string::npos)
                << "message: " << e.what();
    }
}

/// @brief 2次元点の一致を検証する
void ExpectPoint(const Vector2d& actual, const double r, const double z) {
    EXPECT_NEAR(actual.x(), r, kTol);
    EXPECT_NEAR(actual.y(), z, kTol);
}

/// @brief 直径10・切れ刃長30・工具長80・突き出し60・ホルダφ40×60のボール工具 (指令点=中心)
SimpleToolSpec BallSpec() {
    SimpleToolSpec spec;
    spec.cutter = SimpleToolSpec::Cutter::kBall;
    spec.command_point = SimpleToolSpec::CommandPoint::kCenter;
    spec.diameter = 10.0;
    spec.cutting_length = 30.0;
    spec.tool_length = 80.0;
    spec.overhang = 60.0;
    spec.holder_diameter = 40.0;
    spec.holder_length = 60.0;
    return spec;
}

/// @brief BallSpecと同寸のスクエア工具 (指令点=先端)
SimpleToolSpec SquareSpec() {
    SimpleToolSpec spec = BallSpec();
    spec.cutter = SimpleToolSpec::Cutter::kSquare;
    spec.command_point = SimpleToolSpec::CommandPoint::kTip;
    return spec;
}

/// @brief BallSpecと同寸のラジアス工具 (コーナR = 1、指令点=先端)
SimpleToolSpec RadiusSpec() {
    SimpleToolSpec spec = SquareSpec();
    spec.cutter = SimpleToolSpec::Cutter::kRadius;
    spec.corner_radius = 1.0;
    return spec;
}

/// @brief 指定部位の要素を返す (無ければnullptr)
const ToolProfileElement* FindElement(const ToolProfile& profile, const ToolPart part) {
    for (const auto& element : profile.elements) {
        if (element.part == part) return &element;
    }
    return nullptr;
}

/// @brief 直線列 (頂点列) の要素を作る
ToolProfileElement PolylineElement(const ToolPart part,
                                   const std::vector<Vector2d>& vertices) {
    ToolProfileElement element;
    element.part = part;
    for (std::size_t i = 0; i + 1 < vertices.size(); ++i) {
        element.segments.push_back(
                ProfileSegment::Line(vertices[i], vertices[i + 1]));
    }
    return element;
}

/// @brief 検証を通る最小の輪郭 (スクエア形の切れ刃要素1つ: 半径5・長さ10)
ToolProfile MinimalProfile() {
    ToolProfile profile;
    profile.elements.push_back(PolylineElement(ToolPart::kCutter, {
            Vector2d(0.0, 0.0), Vector2d(5.0, 0.0), Vector2d(5.0, 10.0),
            Vector2d(0.0, 10.0)}));
    return profile;
}

/// @brief 主軸への差し込み部分を持つ輪郭 (MinimalProfileの切れ刃 + テーパ付きホルダ)
/// @note ホルダは半径20の円柱 (z=40〜120) の上にフランジ面 (z=120) から先細りの
///       テーパ (z=120〜150、半径20→15) を持つ. ゲージラインはフランジ面 z=120
///       であり、z=120〜150が主軸内に入る. `command_point_z`は0
ToolProfile TaperHolderProfile() {
    ToolProfile profile = MinimalProfile();
    profile.elements.push_back(PolylineElement(ToolPart::kHolder, {
            Vector2d(0.0, 40.0), Vector2d(20.0, 40.0), Vector2d(20.0, 120.0),
            Vector2d(15.0, 120.0), Vector2d(15.0, 150.0), Vector2d(0.0, 150.0)}));
    profile.gauge_line_z = 120.0;
    return profile;
}

}  // namespace



// ---- 簡易アセンブリ ----

TEST(ToolProfileTest, Simple_BallHasThreeElements) {
    std::vector<mc::Diagnostic> warnings;
    const ToolProfile profile = mc::MakeSimpleToolProfile(BallSpec(), &warnings);
    EXPECT_TRUE(warnings.empty());
    ASSERT_EQ(profile.elements.size(), 3u);
    EXPECT_EQ(profile.elements[0].part, ToolPart::kCutter);
    EXPECT_EQ(profile.elements[1].part, ToolPart::kShank);
    EXPECT_EQ(profile.elements[2].part, ToolPart::kHolder);

    // 切れ刃: 1/4円弧 (0,0)→(5,5) 中心(0,5) 反時計回り、縦線 (5,5)→(5,30)、端面 (5,30)→(0,30)
    const auto& cutter = profile.elements[0].segments;
    ASSERT_EQ(cutter.size(), 3u);
    EXPECT_EQ(cutter[0].kind, ProfileSegment::Kind::kArc);
    ExpectPoint(cutter[0].start, 0.0, 0.0);
    ExpectPoint(cutter[0].end, 5.0, 5.0);
    ExpectPoint(cutter[0].center, 0.0, 5.0);
    EXPECT_TRUE(cutter[0].counter_clockwise);
    EXPECT_EQ(cutter[1].kind, ProfileSegment::Kind::kLine);
    ExpectPoint(cutter[1].start, 5.0, 5.0);
    ExpectPoint(cutter[1].end, 5.0, 30.0);
    EXPECT_EQ(cutter[2].kind, ProfileSegment::Kind::kLine);
    ExpectPoint(cutter[2].end, 0.0, 30.0);

    EXPECT_NEAR(profile.command_point_z, 5.0, kTol);
    // ゲージライン = ホルダ上端 (overhang + holder_length)
    ASSERT_TRUE(profile.gauge_line_z.has_value());
    EXPECT_NEAR(*profile.gauge_line_z, 120.0, kTol);
    EXPECT_NEAR(profile.CuttingLength(), 30.0, kTol);
    EXPECT_NEAR(profile.Reach(), 120.0, kTol);
    EXPECT_NEAR(profile.MaxRadius(), 20.0, kTol);
    const auto shank = profile.PartExtent(ToolPart::kShank);
    ASSERT_TRUE(shank.has_value());
    EXPECT_NEAR((*shank)[0], 30.0, kTol);
    EXPECT_NEAR((*shank)[1], 80.0, kTol);
}

TEST(ToolProfileTest, Simple_SquareIsLinesOnly) {
    const ToolProfile profile = mc::MakeSimpleToolProfile(SquareSpec(), nullptr);
    const ToolProfileElement* cutter = FindElement(profile, ToolPart::kCutter);
    ASSERT_NE(cutter, nullptr);
    ASSERT_EQ(cutter->segments.size(), 3u);
    for (const auto& segment : cutter->segments) {
        EXPECT_EQ(segment.kind, ProfileSegment::Kind::kLine);
    }
    ExpectPoint(cutter->segments[0].start, 0.0, 0.0);
    ExpectPoint(cutter->segments[0].end, 5.0, 0.0);
    ExpectPoint(cutter->segments[1].end, 5.0, 30.0);
    ExpectPoint(cutter->segments[2].end, 0.0, 30.0);
    EXPECT_NEAR(profile.command_point_z, 0.0, kTol);
}

TEST(ToolProfileTest, Simple_RadiusHasCornerArc) {
    const ToolProfile profile = mc::MakeSimpleToolProfile(RadiusSpec(), nullptr);
    const ToolProfileElement* cutter = FindElement(profile, ToolPart::kCutter);
    ASSERT_NE(cutter, nullptr);
    ASSERT_EQ(cutter->segments.size(), 4u);
    EXPECT_EQ(cutter->segments[0].kind, ProfileSegment::Kind::kLine);
    ExpectPoint(cutter->segments[0].end, 4.0, 0.0);
    EXPECT_EQ(cutter->segments[1].kind, ProfileSegment::Kind::kArc);
    ExpectPoint(cutter->segments[1].start, 4.0, 0.0);
    ExpectPoint(cutter->segments[1].end, 5.0, 1.0);
    ExpectPoint(cutter->segments[1].center, 4.0, 1.0);
    EXPECT_TRUE(cutter->segments[1].counter_clockwise);
    EXPECT_EQ(cutter->segments[2].kind, ProfileSegment::Kind::kLine);
    ExpectPoint(cutter->segments[2].end, 5.0, 30.0);
    EXPECT_EQ(cutter->segments[3].kind, ProfileSegment::Kind::kLine);
    ExpectPoint(cutter->segments[3].end, 0.0, 30.0);
}

TEST(ToolProfileTest, Simple_BallWithoutStraightFlute) {
    // 切れ刃長 == 半径: 縦線が無く、円弧→端面の2区間になる
    SimpleToolSpec spec = BallSpec();
    spec.cutting_length = 5.0;
    const ToolProfile profile = mc::MakeSimpleToolProfile(spec, nullptr);
    const ToolProfileElement* cutter = FindElement(profile, ToolPart::kCutter);
    ASSERT_NE(cutter, nullptr);
    ASSERT_EQ(cutter->segments.size(), 2u);
    EXPECT_EQ(cutter->segments[0].kind, ProfileSegment::Kind::kArc);
    ExpectPoint(cutter->segments[0].end, 5.0, 5.0);
    EXPECT_EQ(cutter->segments[1].kind, ProfileSegment::Kind::kLine);
    ExpectPoint(cutter->segments[1].start, 5.0, 5.0);
    ExpectPoint(cutter->segments[1].end, 0.0, 5.0);
    EXPECT_NO_THROW(mc::ValidateToolProfile(profile));
}

TEST(ToolProfileTest, Simple_NoShankWhenToolLengthEqualsCuttingLength) {
    SimpleToolSpec spec = BallSpec();
    spec.tool_length = spec.cutting_length;
    const ToolProfile profile = mc::MakeSimpleToolProfile(spec, nullptr);
    ASSERT_EQ(profile.elements.size(), 2u);
    EXPECT_EQ(FindElement(profile, ToolPart::kShank), nullptr);
    EXPECT_FALSE(profile.PartExtent(ToolPart::kShank).has_value());
}

TEST(ToolProfileTest, Simple_WarnsWhenHolderCoversCuttingEdge) {
    SimpleToolSpec spec = BallSpec();
    spec.overhang = 20.0;  // < cutting_length (30)
    std::vector<mc::Diagnostic> warnings;
    const ToolProfile profile = mc::MakeSimpleToolProfile(spec, &warnings);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0].severity, mc::Severity::kWarning);
    EXPECT_NE(warnings[0].message.find("covers"), std::string::npos)
            << warnings[0].message;

    // 形状は変えない: 切れ刃は30まで、ホルダは20〜80
    EXPECT_NEAR(profile.CuttingLength(), 30.0, kTol);
    const auto holder = profile.PartExtent(ToolPart::kHolder);
    ASSERT_TRUE(holder.has_value());
    EXPECT_NEAR((*holder)[0], 20.0, kTol);
    EXPECT_NEAR((*holder)[1], 80.0, kTol);
    EXPECT_NO_THROW(mc::ValidateToolProfile(profile));
}

TEST(ToolProfileTest, Simple_ThrowsOnGeometricViolations) {
    {
        SimpleToolSpec spec = BallSpec();
        spec.cutting_length = 4.0;  // < 半径5
        ExpectInvalidArgumentContaining(
                [&] { mc::MakeSimpleToolProfile(spec, nullptr); }, "ball radius");
    }
    {
        SimpleToolSpec spec = RadiusSpec();
        spec.corner_radius = 5.0;  // >= 半径
        ExpectInvalidArgumentContaining(
                [&] { mc::MakeSimpleToolProfile(spec, nullptr); }, "corner_radius");
    }
    {
        SimpleToolSpec spec = RadiusSpec();
        spec.corner_radius = 2.0;
        spec.cutting_length = 1.0;  // < corner_radius
        ExpectInvalidArgumentContaining(
                [&] { mc::MakeSimpleToolProfile(spec, nullptr); }, "corner_radius");
    }
    {
        SimpleToolSpec spec = BallSpec();
        spec.tool_length = 20.0;  // < cutting_length
        ExpectInvalidArgumentContaining(
                [&] { mc::MakeSimpleToolProfile(spec, nullptr); }, "tool_length");
    }
    {
        SimpleToolSpec spec = SquareSpec();
        spec.command_point = SimpleToolSpec::CommandPoint::kCenter;
        ExpectInvalidArgumentContaining(
                [&] { mc::MakeSimpleToolProfile(spec, nullptr); }, "center");
    }
    {
        SimpleToolSpec spec = BallSpec();
        spec.holder_length = 0.0;
        ExpectInvalidArgumentContaining(
                [&] { mc::MakeSimpleToolProfile(spec, nullptr); }, "positive");
    }
    {
        SimpleToolSpec spec = BallSpec();
        spec.diameter = -10.0;
        ExpectInvalidArgumentContaining(
                [&] { mc::MakeSimpleToolProfile(spec, nullptr); }, "positive");
    }
}



// ---- 検証 ----

TEST(ToolProfileTest, Validate_AcceptsSimpleProfiles) {
    EXPECT_NO_THROW(mc::ValidateToolProfile(mc::MakeSimpleToolProfile(BallSpec(), nullptr)));
    EXPECT_NO_THROW(mc::ValidateToolProfile(mc::MakeSimpleToolProfile(SquareSpec(), nullptr)));
    EXPECT_NO_THROW(mc::ValidateToolProfile(mc::MakeSimpleToolProfile(RadiusSpec(), nullptr)));
    EXPECT_NO_THROW(mc::ValidateToolProfile(MinimalProfile()));
    EXPECT_NO_THROW(mc::ValidateToolProfile(TaperHolderProfile()));
}

TEST(ToolProfileTest, Validate_RejectsEachRule) {
    // 要素が空
    ExpectInvalidArgumentContaining(
            [] { mc::ValidateToolProfile(ToolProfile{}); }, "no elements");
    // 切れ刃要素が無い
    {
        ToolProfile profile;
        profile.elements.push_back(PolylineElement(ToolPart::kHolder, {
                Vector2d(0.0, 0.0), Vector2d(5.0, 0.0), Vector2d(0.0, 0.0)}));
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "no cutter element");
    }
    // 区間が空の要素
    {
        ToolProfile profile = MinimalProfile();
        profile.elements.push_back(ToolProfileElement{ToolPart::kHolder, "", {}, {}, 1.0f});
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "element[1] (holder): no segments");
    }
    // 零長の直線
    {
        ToolProfile profile = MinimalProfile();
        profile.elements[0].segments[1] =
                ProfileSegment::Line(Vector2d(5.0, 0.0), Vector2d(5.0, 0.0));
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "segment[1]: zero-length line");
    }
    // 円弧の端点が中心から等距離でない
    {
        ToolProfile profile = MinimalProfile();
        profile.elements[0].segments[1] = ProfileSegment::Arc(
                Vector2d(5.0, 0.0), Vector2d(5.0, 10.0), Vector2d(5.0, 3.0));
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "not equidistant");
    }
    // 全円 (始点 == 終点)
    {
        ToolProfile profile = MinimalProfile();
        profile.elements[0].segments[1] = ProfileSegment::Arc(
                Vector2d(5.0, 0.0), Vector2d(5.0, 0.0), Vector2d(5.0, 5.0));
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "degenerate arc");
    }
    // 不連続
    {
        ToolProfile profile = MinimalProfile();
        profile.elements[0].segments[1] =
                ProfileSegment::Line(Vector2d(5.0, 1.0), Vector2d(5.0, 10.0));
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); },
                "segment[0]: not connected to the next segment");
    }
    // 負のr (直線の端点)
    {
        ToolProfile profile = MinimalProfile();
        profile.elements[0].segments[0] =
                ProfileSegment::Line(Vector2d(0.0, 0.0), Vector2d(-5.0, 0.0));
        profile.elements[0].segments[1] =
                ProfileSegment::Line(Vector2d(-5.0, 0.0), Vector2d(5.0, 10.0));
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "segment[0]: negative radius");
    }
    // 負のr (角度πを通る円弧: (0,0)→(0,10) 中心(0,5) を時計回り = 軸の左側を通る)
    {
        ToolProfile profile = MinimalProfile();
        profile.elements[0].segments = {
                ProfileSegment::Arc(Vector2d(0.0, 0.0), Vector2d(0.0, 10.0),
                                    Vector2d(0.0, 5.0), false)};
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "segment[0]: negative radius");
        // 反時計回りなら右側 (r > 0) を通るので受理される
        profile.elements[0].segments[0].counter_clockwise = true;
        EXPECT_NO_THROW(mc::ValidateToolProfile(profile));
    }
    // 軸上で始まらない / 終わらない
    {
        ToolProfile profile = MinimalProfile();
        profile.elements[0].segments.erase(profile.elements[0].segments.begin());
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "does not start on the axis");
    }
    {
        ToolProfile profile = MinimalProfile();
        profile.elements[0].segments.pop_back();
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "does not end on the axis");
    }
    // 先端が原点でない (切れ刃が z = 1 から始まる)
    {
        ToolProfile profile;
        profile.elements.push_back(PolylineElement(ToolPart::kCutter, {
                Vector2d(0.0, 1.0), Vector2d(5.0, 1.0), Vector2d(5.0, 10.0),
                Vector2d(0.0, 10.0)}));
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "tip is not at the origin");
    }
    // 先端が原点でない (他の要素が原点より先端側に出る)
    {
        ToolProfile profile = MinimalProfile();
        profile.elements.push_back(PolylineElement(ToolPart::kHolder, {
                Vector2d(0.0, -1.0), Vector2d(8.0, -1.0), Vector2d(8.0, 5.0),
                Vector2d(0.0, 5.0)}));
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "tip is not at the origin");
    }
    // 指令点が範囲外
    {
        ToolProfile profile = MinimalProfile();
        profile.command_point_z = 10.5;
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "command point is outside");
        profile.command_point_z = -0.5;
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "command point is outside");
    }
    // ゲージラインが範囲外 (境界の内側 [0, Reach()] は受理)
    {
        ToolProfile profile = MinimalProfile();
        profile.gauge_line_z = 10.0;
        EXPECT_NO_THROW(mc::ValidateToolProfile(profile));
        profile.gauge_line_z = 0.0;
        EXPECT_NO_THROW(mc::ValidateToolProfile(profile));
        profile.gauge_line_z = 10.5;
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "gauge line is outside");
        profile.gauge_line_z = -0.5;
        ExpectInvalidArgumentContaining(
                [&] { mc::ValidateToolProfile(profile); }, "gauge line is outside");
    }
}

TEST(ToolProfileTest, CloseElementOnAxis_AddsCaps) {
    ToolProfileElement element = PolylineElement(
            ToolPart::kShank, {Vector2d(5.0, 0.0), Vector2d(5.0, 10.0)});
    mc::CloseElementOnAxis(&element);
    ASSERT_EQ(element.segments.size(), 3u);
    ExpectPoint(element.segments[0].start, 0.0, 0.0);
    ExpectPoint(element.segments[0].end, 5.0, 0.0);
    ExpectPoint(element.segments[1].start, 5.0, 0.0);
    ExpectPoint(element.segments[1].end, 5.0, 10.0);
    ExpectPoint(element.segments[2].start, 5.0, 10.0);
    ExpectPoint(element.segments[2].end, 0.0, 10.0);

    // 既に閉じていれば不変
    mc::CloseElementOnAxis(&element);
    EXPECT_EQ(element.segments.size(), 3u);
    ToolProfileElement empty;
    mc::CloseElementOnAxis(&empty);
    EXPECT_TRUE(empty.segments.empty());
}



// ---- 派生値 ----

TEST(ToolProfileTest, Derived_ArcExtremaAreIncluded) {
    // 中心(0,R)の1/4円弧 (0,0)→(R,R): 端点のrが最大 (張り出しなし)
    const double r = 5.0;
    ToolProfile profile;
    ToolProfileElement element;
    element.part = ToolPart::kCutter;
    element.segments = {
            ProfileSegment::Arc(Vector2d(0.0, 0.0), Vector2d(r, r),
                                Vector2d(0.0, r)),
            ProfileSegment::Line(Vector2d(r, r), Vector2d(0.0, r))};
    profile.elements.push_back(element);
    EXPECT_NEAR(profile.MaxRadius(), r, kTol);
    EXPECT_NEAR(profile.Reach(), r, kTol);

    // 半円弧 (0,0)→(0,2R) 中心(0,R) 反時計回り: 角度0を通過し、center.r + radius = R
    element.segments = {
            ProfileSegment::Arc(Vector2d(0.0, 0.0), Vector2d(0.0, 2.0 * r),
                                Vector2d(0.0, r), true)};
    profile.elements[0] = element;
    EXPECT_NEAR(profile.MaxRadius(), r, kTol);
    EXPECT_NEAR(profile.Reach(), 2.0 * r, kTol);
    EXPECT_NEAR(profile.CuttingLength(), 2.0 * r, kTol);

    // 上端を越えて張り出す円弧: (0,0)→(2R,0) 中心(R,0) 時計回り は角度π/2を通過し、
    // 最大zは center.z + radius = R
    element.segments = {
            ProfileSegment::Arc(Vector2d(0.0, 0.0), Vector2d(2.0 * r, 0.0),
                                Vector2d(r, 0.0), false)};
    profile.elements[0] = element;
    EXPECT_NEAR(profile.Reach(), r, kTol);
    EXPECT_NEAR(profile.MaxRadius(), 2.0 * r, kTol);
}

TEST(ToolProfileTest, GaugeLength_PrefersGaugeLineThenHolderTopThenReach) {
    std::vector<mc::Diagnostic> warnings;
    // ゲージライン指定あり (簡易輪郭はホルダ上端): 警告なし
    const ToolProfile simple = mc::MakeSimpleToolProfile(BallSpec(), nullptr);
    EXPECT_NEAR(simple.GaugeLength(&warnings), 120.0, kTol);
    EXPECT_TRUE(warnings.empty());

    // ゲージライン未指定・ホルダあり: ホルダ上端で代用し警告
    ToolProfile without_gauge = simple;
    without_gauge.gauge_line_z.reset();
    EXPECT_NEAR(without_gauge.GaugeLength(&warnings), 120.0, kTol);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("gauge line not specified"), std::string::npos)
            << warnings[0].message;
    EXPECT_NE(warnings[0].message.find("holder top"), std::string::npos)
            << warnings[0].message;

    // ゲージライン未指定・ホルダなし: 全長で代用し警告
    ToolProfile without_holder = without_gauge;
    without_holder.elements.pop_back();  // ホルダを除く
    EXPECT_NEAR(without_holder.GaugeLength(&warnings),
                without_holder.Reach(), kTol);
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_NE(warnings[1].message.find("tool reach"), std::string::npos)
            << warnings[1].message;
    EXPECT_NEAR(without_holder.GaugeLength(nullptr), 80.0, kTol);
}

TEST(ToolProfileTest, GaugeLength_TaperHolderUsesGaugeLineNotHolderTop) {
    // 差し込み部分 (テーパ) を持つホルダ: ゲージ長はフランジ面 (120) であり,
    // ホルダ上端 (150) ではない. 差し込み深さは Reach() - gauge_line_z
    std::vector<mc::Diagnostic> warnings;
    const ToolProfile profile = TaperHolderProfile();
    EXPECT_NEAR(profile.Reach(), 150.0, kTol);
    EXPECT_NEAR(profile.GaugeLength(&warnings), 120.0, kTol);
    EXPECT_TRUE(warnings.empty());
    EXPECT_NEAR(profile.Reach() - *profile.gauge_line_z, 30.0, kTol);

    // アセンブリの取り付けと制御点は輪郭のゲージ長 (120) に従い、
    // ホルダ上端 (150) には影響されない
    mc::ToolAssemblySpec spec;
    spec.profile = profile;
    spec.control_point = mc::ControlPoint::kTip;
    const Vector3d tip = mc::ControlLocal(spec);
    EXPECT_NEAR(tip.x(), 0.0, kTol);
    EXPECT_NEAR(tip.y(), 0.0, kTol);
    EXPECT_NEAR(tip.z(), -120.0, kTol);
    const igesio::Matrix4d offset = mc::ToolMountOffset(spec);
    EXPECT_NEAR(offset(2, 3), -120.0, kTol);
}



// ---- 名称の相互変換 ----

TEST(ToolProfileTest, Names_RoundTrip) {
    for (const ToolPart part : {ToolPart::kCutter, ToolPart::kShank, ToolPart::kHolder}) {
        const auto parsed = mc::ParseToolPart(mc::ToolPartName(part));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, part);
    }
    EXPECT_EQ(mc::ToolPartName(ToolPart::kCutter), "cutter");
    EXPECT_EQ(mc::ToolPartName(ToolPart::kShank), "shank");
    EXPECT_EQ(mc::ToolPartName(ToolPart::kHolder), "holder");
    EXPECT_FALSE(mc::ParseToolPart("Cutter").has_value());

    for (const auto cutter : {SimpleToolSpec::Cutter::kBall, SimpleToolSpec::Cutter::kSquare,
                              SimpleToolSpec::Cutter::kRadius}) {
        const auto parsed = mc::ParseSimpleCutter(mc::SimpleCutterName(cutter));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, cutter);
    }
    EXPECT_EQ(mc::SimpleCutterName(SimpleToolSpec::Cutter::kRadius), "radius");
    EXPECT_FALSE(mc::ParseSimpleCutter("bull").has_value());

    for (const auto point : {SimpleToolSpec::CommandPoint::kTip,
                             SimpleToolSpec::CommandPoint::kCenter}) {
        const auto parsed = mc::ParseSimpleCommandPoint(mc::SimpleCommandPointName(point));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, point);
    }
    EXPECT_FALSE(mc::ParseSimpleCommandPoint("").has_value());

    for (const auto point : {mc::ControlPoint::kTip, mc::ControlPoint::kGauge}) {
        const auto parsed = mc::ParseControlPoint(mc::ControlPointName(point));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, point);
    }
    EXPECT_EQ(mc::ControlPointName(mc::ControlPoint::kGauge), "gauge");
    EXPECT_FALSE(mc::ParseControlPoint("tcp").has_value());
}



// ---- 制御点と取り付けオフセット ----

TEST(ToolProfileTest, ControlLocal_TipAndGauge) {
    mc::ToolAssemblySpec spec;
    spec.number = 1;
    // 指令点z = 5、ゲージライン (ホルダ上端) z = 120
    spec.profile = mc::MakeSimpleToolProfile(BallSpec(), nullptr);

    // kTip: (0, 0, command_point_z - ゲージ長). G43は無視する
    spec.control_point = mc::ControlPoint::kTip;
    EXPECT_TRUE(mc::ControlLocal(spec).isApprox(Vector3d(0.0, 0.0, -115.0), kTol));
    EXPECT_TRUE(mc::ControlLocal(spec, 12.0).isApprox(Vector3d(0.0, 0.0, -115.0), kTol));

    // kGauge: 原点、G43有効時は (0, 0, -g43_length)
    spec.control_point = mc::ControlPoint::kGauge;
    EXPECT_TRUE(mc::ControlLocal(spec).isZero(kTol));
    EXPECT_TRUE(mc::ControlLocal(spec, 12.0).isApprox(Vector3d(0.0, 0.0, -12.0), kTol));

    // 取り付けオフセット: 回転なし、並進 (0, 0, -ゲージ長)
    const igesio::Matrix4d offset = mc::ToolMountOffset(spec);
    EXPECT_TRUE(mc::RotationPart(offset).isIdentity(kTol));
    EXPECT_TRUE(mc::TranslationPart(offset).isApprox(Vector3d(0.0, 0.0, -120.0), kTol));
    // 先端 (工具座標の原点) は取り付けフレームで (0, 0, -120) に写る
    EXPECT_TRUE(mc::ApplyPoint(offset, Vector3d::Zero())
                        .isApprox(Vector3d(0.0, 0.0, -120.0), kTol));
}
