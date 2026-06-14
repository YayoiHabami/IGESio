/**
 * @file examples/gui/surface_algorithm_verifier_gui.cpp
 * @brief サーフェスのアルゴリズム検証用GUI (IgesViewerGUIの派生) の実装
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "./surface_algorithm_verifier_gui.h"

#ifdef IGESIO_INSPECTION_EXTENSION_ENABLED

#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include <igesio/numerics/core/matrix.h>
#include <igesio/entities/interfaces/i_restricted_surface.h>
#include <igesio/models/assembly.h>

#include <igesio/extensions/inspection/coordinate_frame_group_graphics.h>
#include <igesio/extensions/inspection/instanced_entity_graphics.h>

namespace igesio::graphics {

namespace {

/// @brief ルートから指定ノードまでの累積配置変換 (root→node) を合成する
/// @param node 対象のAssemblyノード
/// @return 累積配置変換. レンダラの `accum = parent_accum · node.GetGlobalTransform()`
///         と一致する (ワールド座標 = accum · モデル空間座標)
/// @note ワールド座標で与えた基準方向をサーフェスのモデル空間へ写すために用いる
igesio::Matrix4d AccumulatedTransform(const models::Assembly* node) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    for (const models::Assembly* n = node; n != nullptr;) {
        m = n->GetGlobalTransform() * m;
        const auto parent = n->GetParent().lock();
        n = parent.get();
    }
    return m;
}

/// @brief 円周率
constexpr double kPi = 3.14159265358979323846;

/// @brief ワールド軸方向の単位ベクトルを取得する
/// @param axis 軸インデックス (0=X, 1=Y, 2=Z)
/// @return 対応する単位ベクトル (範囲外はX軸)
igesio::Vector3d AxisUnit(const int axis) {
    if (axis == 1) return igesio::Vector3d::UnitY();
    if (axis == 2) return igesio::Vector3d::UnitZ();
    return igesio::Vector3d::UnitX();
}

/// @brief ワールド軸まわりの回転行列を作成する
/// @param axis 軸インデックス (0=X, 1=Y, 2=Z)
/// @param angle 回転角 [rad]
/// @return 3x3回転行列 (範囲外はX軸まわり)
igesio::Matrix3d AxisRotation(const int axis, const double angle) {
    const double c = std::cos(angle), s = std::sin(angle);
    igesio::Matrix3d r = igesio::Matrix3d::Identity();
    if (axis == 1) {  // Y
        r(0, 0) = c;  r(0, 2) = s;
        r(2, 0) = -s; r(2, 2) = c;
    } else if (axis == 2) {  // Z
        r(0, 0) = c;  r(0, 1) = -s;
        r(1, 0) = s;  r(1, 1) = c;
    } else {  // X
        r(1, 1) = c;  r(1, 2) = -s;
        r(2, 1) = s;  r(2, 2) = c;
    }
    return r;
}

/// @brief 回転3x3と並進3x1から同次変換行列 (4x4) を組み立てる
/// @param rot 回転 (せん断・スケールを含みうる) 3x3行列
/// @param trans 並進3x1ベクトル
/// @return 同次変換行列
igesio::Matrix4d MakeAffine(const igesio::Matrix3d& rot,
                            const igesio::Vector3d& trans) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m.block<3, 3>(0, 0) = rot;
    m.block<3, 1>(0, 3) = trans;
    return m;
}

}  // namespace



SurfaceAlgorithmVerifierGUI::SurfaceAlgorithmVerifierGUI(
        const int width, const int height,
        const int msaa_samples, const std::string& initial_file)
        : IgesViewerGUI(width, height, msaa_samples, initial_file) {
    // 追加描画機能を配線する (冪等)
    extensions::inspection::RegisterInstancedEntityGraphics();
}

void SurfaceAlgorithmVerifierGUI::RenderExtraMenus() {
    if (ImGui::BeginMenu("Verify")) {
        ImGui::MenuItem("Instance Duplication", nullptr, &dup_window_open_);
        ImGui::EndMenu();
    }
}

void SurfaceAlgorithmVerifierGUI::RenderExtraWindows() {
    // 複製表示 (InstancedEntity) の検証ウィンドウ
    if (dup_window_open_) RenderDuplicationWindow();
}

bool SurfaceAlgorithmVerifierGUI::OnViewportClick(
        const double x, const double y, [[maybe_unused]] const int mods) {
    return false;
}


/**
 * 複製表示 (InstancedEntity) の検証関連
 */

void SurfaceAlgorithmVerifierGUI::RenderDuplicationWindow() {
    if (ImGui::Begin("Instance Duplication", &dup_window_open_)) {
        const auto active = GetScene().ActiveSelection().Active();
        ImGui::Text("Source: %s", active.has_value()
                ? std::to_string(active->ToInt()).c_str() : "(none)");
        ImGui::Combo("Unit", &dup_unit_, "Selected entity\0Owning assembly\0");
        // Assembly単位では所有Assemblyを表示し、ルート (シーン全体) を注意喚起する
        if (dup_unit_ == 1 && active.has_value()) {
            const auto* owner = GetScene().Root().FindOwner(*active);
            const bool is_root = owner && owner->GetParent().expired();
            const char* name = (owner && !owner->Metadata().name.empty())
                    ? owner->Metadata().name.c_str() : "(unnamed)";
            ImGui::Text("  Assembly: %s%s", name, is_root ? " [root]" : "");
        }
        ImGui::Checkbox("Include copy at original position",
                        &dup_include_origin_);
        ImGui::InputInt("Count", &dup_count_);
        if (dup_count_ < 1) dup_count_ = 1;
        ImGui::InputDouble("Span", &dup_span_);
        ImGui::Combo("Translate axis", &dup_translate_axis_, "X\0Y\0Z\0");
        ImGui::InputDouble("Angle step [deg]", &dup_angle_deg_);
        ImGui::Combo("Rotate axis", &dup_rotate_axis_, "X\0Y\0Z\0");

        const bool can_run = active.has_value();
        if (!can_run) ImGui::BeginDisabled();
        if (ImGui::Button("Duplicate selected")) RunDuplication();
        if (!can_run) ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Clear")) ClearDuplicates();

        if (!dup_status_.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", dup_status_.c_str());
        }
    }
    ImGui::End();
}

void SurfaceAlgorithmVerifierGUI::RunDuplication() {
    const auto active = GetScene().ActiveSelection().Active();
    if (!active.has_value()) {
        dup_status_ = "Select an entity first.";
        return;
    }
    auto* owner = GetScene().Root().FindOwner(*active);
    const auto entity = owner ? owner->GetEntity(*active) : nullptr;
    if (!entity) {
        dup_status_ = "Selected entity is no longer available.";
        return;
    }

    const int count = std::max(1, dup_count_);
    // 基準の配置変換 (root→owner). 基準のDE変換は描画側が内部適用するため含めない.
    // copy0をこのBに一致させると元エンティティと同一位置に重なる (描画一致の確認).
    const igesio::Matrix4d base = AccumulatedTransform(owner);
    const igesio::Vector3d anchor = base.block<3, 1>(0, 3);
    const double angle_step = dup_angle_deg_ * kPi / 180.0;
    const igesio::Vector3d dir = AxisUnit(dup_translate_axis_);

    std::vector<igesio::Matrix4d> transforms;
    transforms.reserve(count);
    for (int k = 0; k < count; ++k) {
        // include_origin時はi=0..N-1 (copy0が元位置)、非include時はi=1..N (全オフセット)
        const int i = dup_include_origin_ ? k : (k + 1);
        // アンカー周りでその場回転 (直線配列を保ちつつ向きだけ変える)
        const igesio::Matrix3d rot = AxisRotation(dup_rotate_axis_, i * angle_step);
        const igesio::Matrix4d spin = MakeAffine(rot, anchor - rot * anchor);
        const igesio::Matrix4d move = MakeAffine(
                igesio::Matrix3d::Identity(), (i * dup_span_) * dir);
        transforms.push_back(move * spin * base);
    }

    // 複製単位に応じてInstancedEntityを生成する.
    // entityモード: 選択エンティティ1つ. assemblyモード: 所有Assemblyを丸ごと.
    std::shared_ptr<extensions::inspection::InstancedEntity> instanced;
    if (dup_unit_ == 1) {
        // ルート (シーン全体) の複製は意図しないため拒否する
        if (owner->GetParent().expired()) {
            dup_status_ = "Owning assembly is the root; "
                    "select an entity inside a sub-assembly.";
            return;
        }
        instanced = extensions::inspection::MakeInstancedAssembly(
                *owner, std::move(transforms));
        if (instanced->MemberCount() == 0) {
            dup_status_ = "Owning assembly has no geometric entities.";
            return;
        }
    } else {
        instanced = extensions::inspection::MakeInstancedEntity(
                entity, std::move(transforms));
    }

    // 既存の複製を除去してから、新しい子Assemblyへ投入する
    ClearDuplicates();
    auto child = models::MakeAssembly("Instanced Duplicates");
    child->AddEntity(instanced);
    duplicates_assembly_id_ = child->GetID();
    AttachLoadedAssembly(child, /*replace=*/false);
    dup_status_ = "Created " + std::to_string(count) + " instances"
            + (dup_unit_ == 1
               ? " of " + std::to_string(instanced->MemberCount()) + " members"
               : "") + ".";
    RequestRedraw();
}

void SurfaceAlgorithmVerifierGUI::ClearDuplicates() {
    if (!duplicates_assembly_id_.has_value()) return;
    GetScene().Root().RemoveChildAssembly(*duplicates_assembly_id_,
                                          models::RemovalPolicy::kCascade);
    duplicates_assembly_id_.reset();
    OnModelEdited();
    RequestRedraw();
}

}  // namespace igesio::graphics

#endif  // IGESIO_INSPECTION_EXTENSION_ENABLED
