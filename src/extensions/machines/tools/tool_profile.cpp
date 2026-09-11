/**
 * @file extensions/machines/tools/tool_profile.cpp
 * @brief 工具・ホルダのデータモデルと、工具の簡易アセンブリ
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/tools/tool_profile.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"

namespace igesio::extensions::machines {

namespace {

using igesio::Vector2d;

/// @brief 幾何の許容誤差 (長さゼロ・軸上判定・連続性)
constexpr double kTol = kDegenerateTolerance;

/// @brief 診断の発生箇所
constexpr const char* kContext = "tool";

/// @brief 2次元極座標平面の円弧の角度表現
/// @note 角度は+r軸から反時計回りに測る.
///       本体の`CircularArc`は使わず、検証と派生値の計算で共用する
struct ArcSweep {
    /// @brief 始点の角度 [rad]
    double start_angle = 0.0;
    /// @brief 角度範囲 [rad]
    /// @note 向きによらず正. 始点と終点が一致すれば2π
    double sweep = 0.0;
};

/// @brief 円弧の半径 (始点と中心の距離) を返す
double ArcRadius(const ProfileSegment& arc) {
    return (arc.start - arc.center).norm();
}

/// @brief 角度を[0, 2π)へ正規化する
/// @note 2πの直前 (許容誤差の内側) は0とみなし、端点の判定が浮動小数の
///       誤差で反転しないようにする
double NormalizeTurn(const double angle) {
    double normalized = std::fmod(angle, kFullTurn);
    if (normalized < 0.0) normalized += kFullTurn;
    if (normalized >= kFullTurn - kTol) normalized = 0.0;
    return normalized;
}

/// @brief 円弧の始点角と角度範囲を求める
ArcSweep ArcSweepOf(const ProfileSegment& arc) {
    const Vector2d to_start = arc.start - arc.center;
    const Vector2d to_end = arc.end - arc.center;
    const double start_angle = std::atan2(to_start.y(), to_start.x());
    const double end_angle = std::atan2(to_end.y(), to_end.x());
    const double signed_sweep =
            arc.counter_clockwise ? end_angle - start_angle : start_angle - end_angle;
    double sweep = NormalizeTurn(signed_sweep);

    // 始点と終点が一致する円弧は全円として扱う (検証時に拒否)
    if (sweep <= kTol) sweep = kFullTurn;
    return ArcSweep{start_angle, sweep};
}

/// @brief 円弧が指定した角度の方向を通過するか (端点を含む)
/// @param arc 円弧区間
/// @param angle 判定する角度 [rad] (+r軸から反時計回り)
bool ArcPassesThrough(const ProfileSegment& arc, const double angle) {
    const ArcSweep sweep = ArcSweepOf(arc);
    // 始点から進行方向に測った指定角までの角度
    const double offset = NormalizeTurn(
            arc.counter_clockwise ? angle - sweep.start_angle
                                  : sweep.start_angle - angle);
    return offset <= sweep.sweep + kTol;
}

/// @brief 点集合のr・zの範囲
struct Extents {
    /// @brief 最小r
    double min_r = 0.0;
    /// @brief 最大r
    double max_r = 0.0;
    /// @brief 最小z
    double min_z = 0.0;
    /// @brief 最大z
    double max_z = 0.0;

    /// @brief 点を含めるように広げる
    void Include(const double r, const double z) {
        min_r = std::min(min_r, r);
        max_r = std::max(max_r, r);
        min_z = std::min(min_z, z);
        max_z = std::max(max_z, z);
    }
};

/// @brief 要素の点集合 (区間の端点と円弧の凸部) の範囲を求める
/// @return 区間が無ければ`std::nullopt`
std::optional<Extents> ElementExtents(const ToolProfileElement& element) {
    if (element.segments.empty()) return std::nullopt;
    const Vector2d& first = element.segments.front().start;
    Extents extents{first.x(), first.x(), first.y(), first.y()};
    for (const ProfileSegment& segment : element.segments) {
        extents.Include(segment.start.x(), segment.start.y());
        extents.Include(segment.end.x(), segment.end.y());
        if (segment.kind != ProfileSegment::Kind::kArc) continue;

        // 円弧が主要角 (0, π/2, π, 3π/2) を含む場合は、その方向の凸部を含める
        const double radius = ArcRadius(segment);
        const Vector2d& c = segment.center;
        if (ArcPassesThrough(segment, 0.0)) extents.Include(c.x() + radius, c.y());
        if (ArcPassesThrough(segment, kQuarterTurn)) extents.Include(c.x(), c.y() + radius);
        if (ArcPassesThrough(segment, kHalfTurn)) extents.Include(c.x() - radius, c.y());
        if (ArcPassesThrough(segment, 3.0 * kQuarterTurn)) {
            extents.Include(c.x(), c.y() - radius);
        }
    }
    return extents;
}

/// @brief 要素識別用の文字列 (`"element[i] (<part>)"`)
std::string ElementLabel(const std::size_t index, const ToolPart part) {
    return "element[" + std::to_string(index) + "] ("
           + std::string(ToolPartName(part)) + ")";
}

/// @brief 検証違反の例外を投げる
/// @param detail `"ToolProfile: "`に続く文言
[[noreturn]] void Fail(const std::string& detail) {
    throw std::invalid_argument("ToolProfile: " + detail);
}

/// @brief 区間1つを検証する (長さゼロ・円弧の整合・負の半径)
/// @param segment 検証する区間
/// @param label 例外文言の識別用ラベル (`"element[i] (<part>) segment[j]"`)
void ValidateSegment(const ProfileSegment& segment, const std::string& label) {
    if (segment.kind == ProfileSegment::Kind::kLine) {
        if ((segment.end - segment.start).norm() <= kTol) {
            Fail(label + ": zero-length line");
        }
    } else {
        const double radius = ArcRadius(segment);
        if (std::abs((segment.end - segment.center).norm() - radius) > kTol) {
            Fail(label + ": arc endpoints are not equidistant from the center");
        }
        if (radius <= kTol || (segment.end - segment.start).norm() <= kTol) {
            Fail(label + ": degenerate arc");
        }
    }
    // 直線の端点と、角度πを通過する円弧の軸側への張り出しは負のrになりうる
    double min_r = std::min(segment.start.x(), segment.end.x());
    if (segment.kind == ProfileSegment::Kind::kArc
            && ArcPassesThrough(segment, kHalfTurn)) {
        min_r = std::min(min_r, segment.center.x() - ArcRadius(segment));
    }
    if (min_r < -kTol) Fail(label + ": negative radius");
}

/// @brief 工具・ホルダの部位要素1つを検証する
/// @param element 検証する要素
/// @param index 要素の添字 (例外文言用)
/// @note 区間の存在・各区間・連続性・軸上の始終点
void ValidateElement(const ToolProfileElement& element, const std::size_t index) {
    const std::string label = ElementLabel(index, element.part);
    if (element.segments.empty()) Fail(label + ": no segments");
    for (std::size_t j = 0; j < element.segments.size(); ++j) {
        const std::string segment_label = label + " segment[" + std::to_string(j) + "]";
        ValidateSegment(element.segments[j], segment_label);
        if (j + 1 < element.segments.size()
                && (element.segments[j].end - element.segments[j + 1].start).norm()
                       > kTol) {
            Fail(segment_label + ": not connected to the next segment");
        }
    }
    if (element.segments.front().start.x() > kTol) {
        Fail(label + ": does not start on the axis");
    }
    if (element.segments.back().end.x() > kTol) {
        Fail(label + ": does not end on the axis");
    }
}

/// @brief 指定部位の要素の範囲を合成する (nulloptなら全部位)
std::optional<Extents> ProfileExtents(const ToolProfile& profile,
                                      const std::optional<ToolPart> part) {
    std::optional<Extents> merged;
    for (const ToolProfileElement& element : profile.elements) {
        if (part.has_value() && element.part != *part) continue;
        const std::optional<Extents> extents = ElementExtents(element);
        if (!extents.has_value()) continue;
        if (!merged.has_value()) {
            merged = extents;
            continue;
        }
        merged->Include(extents->min_r, extents->min_z);
        merged->Include(extents->max_r, extents->max_z);
    }
    return merged;
}

}  // namespace



std::string_view ToolPartName(const ToolPart part) {
    switch (part) {
        case ToolPart::kCutter: return "cutter";
        case ToolPart::kShank: return "shank";
        case ToolPart::kHolder: return "holder";
    }
    return "cutter";
}

std::optional<ToolPart> ParseToolPart(const std::string_view text) {
    if (text == "cutter") return ToolPart::kCutter;
    if (text == "shank") return ToolPart::kShank;
    if (text == "holder") return ToolPart::kHolder;
    return std::nullopt;
}

const std::array<float, 3>& DefaultPartColor(const ToolPart part) {
    switch (part) {
        case ToolPart::kCutter: return kDefaultCutterColor;
        case ToolPart::kShank: return kDefaultShankColor;
        case ToolPart::kHolder: return kDefaultHolderColor;
    }
    return kDefaultCutterColor;
}

ProfileSegment ProfileSegment::Line(
        const Vector2d& start, const Vector2d& end) {
    ProfileSegment segment;
    segment.kind = Kind::kLine;
    segment.start = start;
    segment.end = end;
    return segment;
}

ProfileSegment ProfileSegment::Arc(
        const Vector2d& start, const Vector2d& end,
        const Vector2d& center, const bool counter_clockwise) {
    ProfileSegment segment;
    segment.kind = Kind::kArc;
    segment.start = start;
    segment.end = end;
    segment.center = center;
    segment.counter_clockwise = counter_clockwise;
    return segment;
}

void ValidateToolProfile(const ToolProfile& profile) {
    if (profile.elements.empty()) Fail("no elements");
    const bool has_cutter = std::any_of(
            profile.elements.begin(), profile.elements.end(),
            [](const ToolProfileElement& e) { return e.part == ToolPart::kCutter; });
    if (!has_cutter) Fail("no cutter element");

    for (std::size_t i = 0; i < profile.elements.size(); ++i) {
        ValidateElement(profile.elements[i], i);
    }

    // 先端が原点にあることを確認
    // いずれかの切れ刃要素が原点から始まり、どの要素も原点より先端側に出ない
    const bool starts_at_origin = std::any_of(
            profile.elements.begin(), profile.elements.end(),
            [](const ToolProfileElement& e) {
                return e.part == ToolPart::kCutter
                       && e.segments.front().start.norm() <= kTol;
            });
    const std::optional<Extents> extents = ProfileExtents(profile, std::nullopt);
    if (!starts_at_origin || !extents.has_value() || extents->min_z < -kTol) {
        Fail("tip is not at the origin");
    }

    if (profile.command_point_z < -kTol ||
        profile.command_point_z > extents->max_z + kTol) {
        Fail("command point is outside the tool");
    }
    if (profile.gauge_line_z.has_value()
            && (*profile.gauge_line_z < -kTol
                || *profile.gauge_line_z > extents->max_z + kTol)) {
        Fail("gauge line is outside the tool");
    }
}

void CloseElementOnAxis(ToolProfileElement* element) {
    if (element == nullptr || element->segments.empty()) return;
    const Vector2d first = element->segments.front().start;
    if (first.x() > kTol) {
        element->segments.insert(
                element->segments.begin(),
                ProfileSegment::Line(Vector2d(0.0, first.y()), first));
    }
    const Vector2d last = element->segments.back().end;
    if (last.x() > kTol) {
        element->segments.push_back(
                ProfileSegment::Line(last, Vector2d(0.0, last.y())));
    }
}

double ToolProfile::CuttingLength() const {
    const std::optional<Extents> extents = ProfileExtents(*this, ToolPart::kCutter);
    return extents.has_value() ? extents->max_z : 0.0;
}

double ToolProfile::Reach() const {
    const std::optional<Extents> extents = ProfileExtents(*this, std::nullopt);
    return extents.has_value() ? extents->max_z : 0.0;
}

double ToolProfile::MaxRadius() const {
    const std::optional<Extents> extents = ProfileExtents(*this, std::nullopt);
    return extents.has_value() ? extents->max_r : 0.0;
}

std::optional<std::array<double, 2>> ToolProfile::PartExtent(
        const ToolPart part) const {
    const std::optional<Extents> extents = ProfileExtents(*this, part);
    if (!extents.has_value()) return std::nullopt;
    return std::array<double, 2>{extents->min_z, extents->max_z};
}

double ToolProfile::GaugeLength(std::vector<Diagnostic>* warnings) const {
    if (gauge_line_z.has_value()) return *gauge_line_z;

    // ゲージライン未指定の場合、差し込み部分を持たない輪郭とみなしてホルダ上端で代用する
    const std::optional<std::array<double, 2>> holder = PartExtent(ToolPart::kHolder);
    const double fallback = holder.has_value() ? (*holder)[1] : Reach();
    if (warnings != nullptr) {
        warnings->push_back(Diagnostic{
                Severity::kWarning, kContext,
                std::string("gauge line not specified; gauge length defaults to the ")
                + (holder.has_value() ? "holder top (" : "tool reach (")
                + FormatFixed(fallback, 3) + ")",
                0});
    }
    return fallback;
}



/**
 * 簡易アセンブリ (輪郭線ではなく寸法で工具を規定する) 関連
 */

namespace {

/// @brief 直線列 (頂点列) の要素を作る
/// @param part 部位
/// @param vertices 頂点 (r, z) の列 (先頭から順に直線で結ぶ)
ToolProfileElement MakePolylineElement(
        const ToolPart part, const std::vector<Vector2d>& vertices) {
    ToolProfileElement element;
    element.part = part;
    for (std::size_t i = 0; i + 1 < vertices.size(); ++i) {
        element.segments.push_back(
                ProfileSegment::Line(vertices[i], vertices[i + 1]));
    }
    return element;
}

/// @brief 簡易アセンブリの寸法が幾何として成立するかを検証する
/// @throw std::invalid_argument 成立しない場合
void ValidateSimpleToolSpec(const SimpleToolSpec& spec) {
    const auto require_positive = [](const double value, const char* name) {
        if (!(value > 0.0)) {
            throw std::invalid_argument(
                    std::string("SimpleToolSpec: ") + name + " must be positive (got "
                    + FormatFixed(value, 3) + ")");
        }
    };
    require_positive(spec.diameter, "diameter");
    require_positive(spec.cutting_length, "cutting_length");
    require_positive(spec.tool_length, "tool_length");
    require_positive(spec.overhang, "overhang");
    require_positive(spec.holder_diameter, "holder_diameter");
    require_positive(spec.holder_length, "holder_length");

    const double radius = spec.diameter / 2.0;
    switch (spec.cutter) {
        case SimpleToolSpec::Cutter::kBall:
            if (spec.cutting_length < radius) {
                throw std::invalid_argument(
                        "SimpleToolSpec: cutting_length ("
                        + FormatFixed(spec.cutting_length, 3)
                        + ") must be at least the ball radius ("
                        + FormatFixed(radius, 3) + ")");
            }
            break;
        case SimpleToolSpec::Cutter::kRadius:
            if (!(spec.corner_radius > 0.0) || spec.corner_radius >= radius) {
                throw std::invalid_argument(
                        "SimpleToolSpec: corner_radius (" + FormatFixed(spec.corner_radius, 3)
                        + ") must be in (0, " + FormatFixed(radius, 3) + ")");
            }
            if (spec.cutting_length < spec.corner_radius) {
                throw std::invalid_argument(
                        "SimpleToolSpec: cutting_length ("
                        + FormatFixed(spec.cutting_length, 3)
                        + ") must be at least corner_radius ("
                        + FormatFixed(spec.corner_radius, 3) + ")");
            }
            break;
        case SimpleToolSpec::Cutter::kSquare:
            break;
    }
    if (spec.cutting_length > spec.tool_length) {
        throw std::invalid_argument(
                "SimpleToolSpec: cutting_length (" + FormatFixed(spec.cutting_length, 3)
                + ") exceeds tool_length (" + FormatFixed(spec.tool_length, 3) + ")");
    }
    if (spec.command_point == SimpleToolSpec::CommandPoint::kCenter
            && spec.cutter != SimpleToolSpec::Cutter::kBall) {
        throw std::invalid_argument(
                "SimpleToolSpec: command_point = \"center\" is only valid for ball cutters");
    }
}

/// @brief 簡易アセンブリの切れ刃要素を作る
/// @note 先端 (0, 0) から始まり、上端の端面 (R, Lc) → (0, Lc) で終わる.
///       先端形状の上端と切れ刃上端が許容誤差内で一致すれば縦の直線は置かない
ToolProfileElement MakeSimpleCutter(const SimpleToolSpec& spec) {
    const double radius = spec.diameter / 2.0;
    const double length = spec.cutting_length;
    ToolProfileElement element;
    element.part = ToolPart::kCutter;
    switch (spec.cutter) {
        case SimpleToolSpec::Cutter::kBall:
            // 先端の円弧セグメント
            // 軸上の先端から径方向最大部 (R, R) までの1/4円弧 (中心 (0, R))
            element.segments.push_back(ProfileSegment::Arc(
                    Vector2d(0.0, 0.0), Vector2d(radius, radius), Vector2d(0.0, radius)));
            if (length > radius + kTol) {
                element.segments.push_back(ProfileSegment::Line(
                        Vector2d(radius, radius), Vector2d(radius, length)));
            }
            break;
        case SimpleToolSpec::Cutter::kSquare:
            element.segments.push_back(ProfileSegment::Line(
                    Vector2d(0.0, 0.0), Vector2d(radius, 0.0)));
            element.segments.push_back(ProfileSegment::Line(
                    Vector2d(radius, 0.0), Vector2d(radius, length)));
            break;
        case SimpleToolSpec::Cutter::kRadius: {
            // 先端の平面から角のコーナR (中心 (R - rc, rc)) 、外周
            const double rc = spec.corner_radius;
            element.segments.push_back(ProfileSegment::Line(
                    Vector2d(0.0, 0.0), Vector2d(radius - rc, 0.0)));
            element.segments.push_back(ProfileSegment::Arc(
                    Vector2d(radius - rc, 0.0), Vector2d(radius, rc),
                    Vector2d(radius - rc, rc)));
            if (length > rc + kTol) {
                element.segments.push_back(ProfileSegment::Line(
                        Vector2d(radius, rc), Vector2d(radius, length)));
            }
            break;
        }
    }
    // 上端の端面
    element.segments.push_back(ProfileSegment::Line(
            Vector2d(radius, length), Vector2d(0.0, length)));
    return element;
}

}  // namespace

std::optional<SimpleToolSpec::Cutter> ParseSimpleCutter(const std::string_view text) {
    if (text == "ball") return SimpleToolSpec::Cutter::kBall;
    if (text == "square") return SimpleToolSpec::Cutter::kSquare;
    if (text == "radius") return SimpleToolSpec::Cutter::kRadius;
    return std::nullopt;
}

std::string_view SimpleCutterName(const SimpleToolSpec::Cutter cutter) {
    switch (cutter) {
        case SimpleToolSpec::Cutter::kBall: return "ball";
        case SimpleToolSpec::Cutter::kSquare: return "square";
        case SimpleToolSpec::Cutter::kRadius: return "radius";
    }
    return "ball";
}

std::optional<SimpleToolSpec::CommandPoint>
ParseSimpleCommandPoint(const std::string_view text) {
    if (text == "tip") return SimpleToolSpec::CommandPoint::kTip;
    if (text == "center") return SimpleToolSpec::CommandPoint::kCenter;
    return std::nullopt;
}

std::string_view SimpleCommandPointName(
        const SimpleToolSpec::CommandPoint point) {
    return point == SimpleToolSpec::CommandPoint::kCenter ? "center" : "tip";
}

ToolProfile MakeSimpleToolProfile(const SimpleToolSpec& spec,
                                  std::vector<Diagnostic>* warnings) {
    ValidateSimpleToolSpec(spec);
    const double radius = spec.diameter / 2.0;
    const double holder_radius = spec.holder_diameter / 2.0;

    ToolProfile profile;
    profile.elements.push_back(MakeSimpleCutter(spec));

    // シャンク部：切れ刃上端から工具長まで (ホルダの内部も打ち切らない)
    if (spec.tool_length > spec.cutting_length + kTol) {
        profile.elements.push_back(MakePolylineElement(ToolPart::kShank, {
                Vector2d(0.0, spec.cutting_length), Vector2d(radius, spec.cutting_length),
                Vector2d(radius, spec.tool_length), Vector2d(0.0, spec.tool_length)}));
    }

    // ホルダ: 突き出し長の位置より上
    const double holder_top = spec.overhang + spec.holder_length;
    profile.elements.push_back(MakePolylineElement(ToolPart::kHolder, {
            Vector2d(0.0, spec.overhang), Vector2d(holder_radius, spec.overhang),
            Vector2d(holder_radius, holder_top), Vector2d(0.0, holder_top)}));
    // 簡易アセンブリではホルダ上端面をゲージラインとする
    profile.gauge_line_z = holder_top;

    if (spec.cutter == SimpleToolSpec::Cutter::kBall
            && spec.command_point == SimpleToolSpec::CommandPoint::kCenter) {
        profile.command_point_z = radius;
    }

    if (warnings != nullptr && spec.overhang < spec.cutting_length) {
        warnings->push_back(Diagnostic{
                Severity::kWarning, kContext,
                "holder covers the cutting edge: overhang " + FormatFixed(spec.overhang, 3)
                + " < cutting_length " + FormatFixed(spec.cutting_length, 3),
                0});
    }
    return profile;
}

}  // namespace igesio::extensions::machines
