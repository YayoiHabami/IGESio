/**
 * @file extensions/machines/tools/tool_entities.cpp
 * @brief 工具・ホルダのエンティティ化 (Assemblyによるツリー構造の構築)
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/tools/tool_entities.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/structures/color_definition.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"

namespace igesio::extensions::machines {

namespace {

namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector2d;
using igesio::Vector3d;

/// @brief 色成分の量子化段階数 (8bit)
constexpr double kColorLevels = 255.0;

/// @brief 各部位のアセンブリの作成順序
/// @note いずれかの部位の輪郭が存在しない場合は、その部位を飛ばして作成する
constexpr std::array<ToolPart, 3> kPartOrder = {
        ToolPart::kCutter, ToolPart::kShank, ToolPart::kHolder};

/// @brief 母線の座標 (r, z) をXY平面の3次元座標 (x = r, y = z, z = 0) へ写す
Vector3d ToGeneratrixPoint(const Vector2d& rz) {
    return Vector3d(rz.x(), rz.y(), 0.0);
}

/// @brief 母線の座標系 (回転軸がY) から工具座標系 (軸がZ) への変換 R_x(+90°)
igesio::Matrix4d GeneratrixFrameTransform() {
    return MakeRigid(RotationAboutAxis(Vector3d::UnitX(), kQuarterTurn),
                     Vector3d::Zero());
}

/// @brief 色成分 (0~1) を8bit値へ量子化する
int ToColor255(const float channel) {
    const double clamped = std::clamp(static_cast<double>(channel), 0.0, 1.0);
    return static_cast<int>(std::lround(clamped * kColorLevels));
}

/// @brief 要素の色定義 (Type 314) を作る (色の省略時は部位の既定色)
std::shared_ptr<i_ent::ColorDefinition> MakeElementColor(
        const ToolProfileElement& element) {
    const std::array<float, 3>& rgb =
            element.color.has_value() ? *element.color : DefaultPartColor(element.part);
    return i_ent::MakeColorDefinitionFromRGB255(
            ToColor255(rgb[0]), ToColor255(rgb[1]), ToColor255(rgb[2]));
}

/// @brief 要素の母線とその構成曲線
struct Generatrix {
    /// @brief 母線 (Type 106、Type 100/110、またはType 102)
    std::shared_ptr<i_ent::ICurve> curve;
    /// @brief 母線がType 102のときの構成曲線 (物理従属. それ以外は空)
    std::vector<std::shared_ptr<i_ent::EntityBase>> constituents;
};

/// @brief 要素の母線を作る
/// @note 直線のみ→頂点列のType 106 (Form 11).
///       円弧を含む→区間ごとにType 100/110を作り、2本以上ならType 102で1本にする.
///       時計回りの円弧は`is_clockwise`で始終点の順序を保ったまま表す
Generatrix MakeGeneratrix(const ToolProfileElement& element) {
    const bool has_arc = std::any_of(
            element.segments.begin(), element.segments.end(),
            [](const ProfileSegment& s) { return s.kind == ProfileSegment::Kind::kArc; });
    if (!has_arc) {
        std::vector<Vector2d> vertices;
        vertices.reserve(element.segments.size() + 1);
        vertices.push_back(element.segments.front().start);
        for (const ProfileSegment& segment : element.segments) {
            vertices.push_back(segment.end);
        }
        return Generatrix{i_ent::MakeLinearPath(vertices), {}};
    }

    std::vector<std::shared_ptr<i_ent::ICurve>> curves;
    curves.reserve(element.segments.size());
    for (const ProfileSegment& segment : element.segments) {
        if (segment.kind == ProfileSegment::Kind::kArc) {
            curves.push_back(i_ent::MakeCircularArc(
                    segment.center, segment.start, segment.end, 0.0,
                    !segment.counter_clockwise));
        } else {
            curves.push_back(i_ent::MakeLine(ToGeneratrixPoint(segment.start),
                                             ToGeneratrixPoint(segment.end)));
        }
    }
    if (curves.size() == 1) return Generatrix{curves.front(), {}};

    Generatrix generatrix{i_ent::MakeCompositeCurve(curves), {}};
    for (const auto& curve : curves) {
        generatrix.constituents.push_back(
                std::dynamic_pointer_cast<i_ent::EntityBase>(curve));
    }
    return generatrix;
}

/// @brief 各部位要素に対応する回転体と付随エンティティを作り、親に登録する
/// @param container 切れ刃/シャンク/ホルダの部位アセンブリ
/// @param element 要素
/// @param index 部位内の要素の番号
void AddElementNode(i_mod::Assembly& container, const ToolProfileElement& element,
                    const std::size_t index) {
    auto node = i_mod::MakeAssembly(ToolElementName(element.part, index));
    node->SetGlobalTransform(GeneratrixFrameTransform());
    if (element.opacity < 1.0f) node->SetOpacityOverride(element.opacity);
    container.AddChildAssembly(node);

    Generatrix generatrix = MakeGeneratrix(element);
    auto [surface, axis] = i_ent::MakeSurfaceOfRevolution(
            Vector3d::Zero(), Vector3d::UnitY(), generatrix.curve);
    auto color = MakeElementColor(element);
    surface->OverwriteColor(color);

    // 登録順: 構成曲線 → 母線 → 軸線 → 色定義 → 面
    std::vector<std::shared_ptr<i_ent::EntityBase>> entities =
            std::move(generatrix.constituents);
    entities.push_back(std::dynamic_pointer_cast<i_ent::EntityBase>(generatrix.curve));
    entities.push_back(std::move(axis));
    entities.push_back(std::move(color));
    entities.push_back(std::move(surface));
    node->AddEntities(entities);
}

}  // namespace



std::string ToolAssemblyName(const int number) {
    return std::string(kToolAssemblyPrefix) + std::to_string(number);
}

std::string ToolElementName(const ToolPart part, const std::size_t index) {
    return std::string(ToolPartName(part)) + std::to_string(index);
}

std::shared_ptr<i_mod::Assembly> MakeToolAssembly(const ToolAssemblySpec& spec) {
    ValidateToolProfile(spec.profile);

    auto tool = i_mod::MakeAssembly(ToolAssemblyName(spec.number));
    for (const ToolPart part : kPartOrder) {
        std::shared_ptr<i_mod::Assembly> container;
        std::size_t index = 0;
        for (const ToolProfileElement& element : spec.profile.elements) {
            if (element.part != part) continue;
            if (!container) {
                container = i_mod::MakeAssembly(std::string(ToolPartName(part)));
                tool->AddChildAssembly(container);
            }
            AddElementNode(*container, element, index++);
        }
    }
    return tool;
}

std::shared_ptr<i_mod::Assembly> FindToolPart(const i_mod::Assembly& tool,
                                              const ToolPart part) {
    const std::string_view name = ToolPartName(part);
    for (const auto& child : tool.GetChildAssemblies()) {
        if (child && child->Metadata().name == name) return child;
    }
    return nullptr;
}

void SetToolPartVisible(i_mod::Assembly& tool,
                        const ToolPart part, const bool visible) {
    if (const auto container = FindToolPart(tool, part)) {
        container->SetVisible(visible);
    }
}

}  // namespace igesio::extensions::machines
