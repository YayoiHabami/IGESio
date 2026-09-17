/**
 * @file extensions/machines/scene/machine_scene.cpp
 * @brief 加工セットアップからのシーン構築
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/scene/machine_scene.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/point.h"
#include "igesio/entities/structures/color_definition.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/tools/tool_entities.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"

namespace igesio::extensions::machines {

namespace {

namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector3d;

/// @brief 色定義 (Type 314) を持つ曲線をアセンブリに登録する
/// @param node 登録先
/// @param curve 曲線 (色を上書きする)
/// @param color 色
/// @note 参照の自己完結のため、色定義も同じアセンブリに登録する
void AddColoredCurve(i_mod::Assembly& node,
                     const std::shared_ptr<i_ent::EntityBase>& curve,
                     const Color& color) {
    auto definition = i_ent::MakeColorDefinition(color);
    curve->OverwriteColor(definition);
    node.AddEntity(definition);
    node.AddEntity(curve);
}

/// @brief 原点から各軸方向に延びる3本の線分 (3軸) をアセンブリに登録する
/// @param node 登録先
/// @param length 各軸の長さ [mm]
void AddTriadLines(i_mod::Assembly& node, const double length) {
    const std::array<Vector3d, 3> axes = {
            Vector3d::UnitX(), Vector3d::UnitY(), Vector3d::UnitZ()};
    for (std::size_t i = 0; i < axes.size(); ++i) {
        AddColoredCurve(node, i_ent::MakeLine(Vector3d::Zero(), axes[i] * length),
                        kTriadColors[i]);
    }
}

/// @brief 制御点の工具座標を計算する
/// @param spec 工具の定義
/// @return 工具座標 (先端原点、+zが工具軸) での制御点
/// @note `ControlLocal`は`tool_mount`フレーム座標を返すため、工具に固定した
///       マーカーには工具座標に変換したものを用いる. G43は反映しない
Vector3d ControlPointInToolFrame(const ToolAssemblySpec& spec) {
    const double z = spec.control_point == ControlPoint::kGauge
                             ? spec.profile.GaugeLength(nullptr)
                             : spec.profile.command_point_z;
    return Vector3d(0.0, 0.0, z);
}

/// @brief 工具軸線と制御点マーカーを工具の子に作る
/// @param tool 工具のアセンブリ (`tool:<n>`)
/// @param spec 工具の定義
/// @param extra 工具軸線を輪郭の最上端から延ばす長さ [mm] (正の値)
void AddToolAxisAndControlPoint(i_mod::Assembly& tool,
                                const ToolAssemblySpec& spec, const double extra) {
    auto axis = MakeChildAssembly(tool, kToolAxisLineName);
    AddColoredCurve(*axis,
                    i_ent::MakeLine(Vector3d::Zero(),
                                    Vector3d(0.0, 0.0, spec.profile.Reach() + extra)),
                    kToolAxisColor);

    auto marker = MakeChildAssembly(tool, kControlPointName);
    AddColoredCurve(*marker, i_ent::MakePoint(ControlPointInToolFrame(spec)),
                    kToolAxisColor);
}

/// @brief 形状を読み込み、失敗した場合は警告を追加する
/// @param instance 形状
/// @param options 読込の設定
/// @param context 診断の発生個所
/// @param[out] warnings 警告の追加先
/// @return 形状のアセンブリ. 読めなかった場合は`nullptr`
/// @throw std::invalid_argument プリミティブの寸法が正でない場合
/// @note 1つの形状の失敗でシーン全体の構築を止めないため、ファイルの不在や
///       不正なファイル (`IGESioError`系) は警告にする
std::shared_ptr<i_mod::Assembly> TryLoadGeometry(
        const GeometryInstance& instance, const GeometryLoadOptions& options,
        const std::string& context, std::vector<Diagnostic>& warnings) {
    try {
        return LoadGeometry(instance.geometry, options, context, &warnings);
    } catch (const igesio::IGESioError& e) {
        warnings.push_back(Diagnostic{
                Severity::kWarning, context,
                std::string("geometry could not be loaded; skipped: ") + e.what(),
                instance.geometry.line});
        return nullptr;
    }
}

/// @brief 形状のアセンブリの名前を作成する
/// @param instance 形状
/// @return `geometry:<name>`または`model:<name>`
std::string GeometryNodeName(const GeometryInstance& instance) {
    if (instance.kind == GeometryInstance::Kind::kModel) {
        return std::string(kModelAssemblyPrefix) + instance.owner;
    }
    return std::string(kGeometryAssemblyPrefix) + instance.geometry.DisplayName();
}

/// @brief 形状の診断の発生個所を作成する
/// @param instance 形状
/// @return `"[[component]](X)"`または`"[[model]](stock)"`
std::string GeometryContext(const GeometryInstance& instance) {
    const std::string_view table =
            instance.kind == GeometryInstance::Kind::kModel ? "[[model]]"
                                                            : "[[component]]";
    return std::string(table) + "(" + instance.owner + ")";
}

/// @brief 形状の`role_tag`を取得する
/// @param instance 形状
/// @return 機械部品は`"machine"`、モデルは役割名
std::string GeometryRoleTag(const GeometryInstance& instance) {
    if (instance.role.has_value()) return std::string(ModelRoleName(*instance.role));
    return std::string(kMachinePartRoleTag);
}

/// @brief 接頭辞付きの名前を作る
/// @param prefix 接頭辞
/// @param id 識別子
std::string PrefixedName(const std::string_view prefix, const std::string_view id) {
    return std::string(prefix) + std::string(id);
}

}  // namespace



/**
 * ---- アセンブリの補助 ----
 */

std::shared_ptr<i_mod::Assembly> MakeChildAssembly(
        i_mod::Assembly& parent, const std::string_view name,
        const igesio::Matrix4d& transform) {
    auto child = i_mod::MakeAssembly(std::string(name));
    child->SetGlobalTransform(transform);
    parent.AddChildAssembly(child);
    return child;
}

std::shared_ptr<i_mod::Assembly> FindChildAssembly(
        const i_mod::Assembly& parent, const std::string_view name) {
    for (const auto& child : parent.GetChildAssemblies()) {
        if (child && child->Metadata().name == name) return child;
    }
    return nullptr;
}



/**
 * ---- 経路線の区間 ----
 */

namespace {

/// @brief 経路線の1区間 (同じワークオフセット・同じ種別の連続した制御点)
struct PathRun {
    /// @brief ワークオフセットid
    std::string work_offset;
    /// @brief 早送りか
    bool rapid = false;
    /// @brief 制御点の列 (ワーク座標. 先頭は直前の区間の終点)
    std::vector<Vector3d> points;
};

/// @brief 現在の区間を閉じ、2点以上あれば区間の一覧に加える
/// @param[in,out] current 現在の区間 (閉じた後は空)
/// @param[out] runs 区間の一覧
void ClosePathRun(PathRun& current, std::vector<PathRun>& runs) {
    if (current.points.size() >= 2) runs.push_back(std::move(current));
    current = PathRun{};
}

/// @brief 制御点の列を現在の区間に加える
/// @param work_offset 制御点のワークオフセットid
/// @param rapid 早送りか
/// @param points 加える制御点 (ワーク座標)
/// @param[in,out] current 現在の区間
/// @param[out] runs 区間の一覧 (区間が切り替わったときに閉じた区間を加える)
/// @note ワークオフセットか種別が変わったら区間を切り替える. 同じワークオフセット
///       内の切り替えでは直前の区間の終点を新しい区間の始点にし、経路線を繋ぐ
void AppendPathPoints(const std::string& work_offset, const bool rapid,
                      const std::vector<Vector3d>& points,
                      PathRun& current, std::vector<PathRun>& runs) {
    const bool open = !current.points.empty();
    if (open && (current.work_offset != work_offset || current.rapid != rapid)) {
        const std::optional<Vector3d> resume =
                current.work_offset == work_offset
                        ? std::optional<Vector3d>(current.points.back())
                        : std::nullopt;
        ClosePathRun(current, runs);
        if (resume.has_value()) current.points.push_back(*resume);
    }
    current.work_offset = work_offset;
    current.rapid = rapid;
    current.points.insert(current.points.end(), points.begin(), points.end());
}

/// @brief 動作レコードの制御点の列 (ワーク座標) を取得する
/// @param record レコード
/// @param state レコード適用前の状態 (円弧の始点に用いる)
/// @param chord_tolerance 円弧の折れ線化の弦誤差 [mm]
/// @return 移動なら終点、円弧なら折れ線化した通過点. 経路線に含めない
///         レコード (機械座標の移動、軸の指令のみの移動、始点不明の円弧、
///         動作でないレコード) は`std::nullopt`
std::optional<std::vector<Vector3d>> PathPointsOf(
        const ClRecord& record, const ClState& state, const double chord_tolerance) {
    if (const auto* motion = std::get_if<ClGoto>(&record)) {
        if (motion->frame == MotionFrame::kMachine || !motion->point.has_value()) {
            return std::nullopt;
        }
        return std::vector<Vector3d>{*motion->point};
    }
    if (const auto* arc = std::get_if<ClArc>(&record)) {
        if (!state.position.has_value()) return std::nullopt;
        return DiscretizeArc(*state.position, *arc, chord_tolerance);
    }
    return std::nullopt;
}

/// @brief プログラムの制御点を区間に分ける
/// @param program CLプログラム
/// @param initial_offset ワークオフセット未選択のときのid
/// @param known_offsets ワーク座標系に定義されたid
/// @param chord_tolerance 円弧の折れ線化の弦誤差 [mm]
/// @param[out] warnings 未定義のワークオフセットの警告 (idごとに1回)
/// @return 区間の一覧 (2点以上の区間のみ)
std::vector<PathRun> CollectPathRuns(
        const ClProgram& program, const std::string& initial_offset,
        const std::set<std::string>& known_offsets,
        const double chord_tolerance, std::vector<Diagnostic>& warnings) {
    std::vector<PathRun> runs;
    PathRun current;
    std::set<std::string> reported;
    ClState state;
    state.work_offset = initial_offset;
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        const ClRecord& record = program.records[i];
        if (IsMotion(record) && !std::holds_alternative<ClDwell>(record)) {
            const std::optional<std::vector<Vector3d>> points =
                    PathPointsOf(record, state, chord_tolerance);
            const bool known = known_offsets.count(state.work_offset) > 0;
            if (!known && reported.insert(state.work_offset).second) {
                warnings.push_back(Diagnostic{
                        Severity::kWarning, "record " + std::to_string(i),
                        "work offset is not defined; path lines skipped: "
                        + state.work_offset,
                        program.HasSources() ? program.sources[i].line : 0});
            }
            if (points.has_value() && known) {
                const auto* motion = std::get_if<ClGoto>(&record);
                const bool rapid = motion != nullptr
                                   && motion->kind == MotionKind::kRapid;
                AppendPathPoints(state.work_offset, rapid, *points, current, runs);
            } else {
                ClosePathRun(current, runs);
            }
        }
        state.Apply(record);
    }
    ClosePathRun(current, runs);
    return runs;
}

}  // namespace



/**
 * ---- MachineScene ----
 */

void MachineScene::Build(const MachiningSetup& setup,
                         const std::shared_ptr<i_mod::Assembly>& root,
                         const SceneBuildOptions& options) {
    if (!root) throw std::invalid_argument("Scene root must not be null");
    if (IsBuilt()) Clear();

    model_ = setup.Model();
    tools_ = setup.Tools();
    work_frames_ = setup.WorkFrames();
    initial_work_offset_ = setup.InitialWorkOffset();
    arc_chord_tolerance_ = options.arc_chord_tolerance;
    root_ = root;
    const std::string machine_name =
            PrefixedName(kMachineAssemblyPrefix, model_->Definition().name);
    machine_ = MakeChildAssembly(*root, machine_name);

    // 途中で失敗した場合に不完全な木を残さない
    try {
        BuildComponents();
        if (options.build_triads) BuildTriads();
        BuildGeometries(setup, options);
        if (options.build_tools) BuildTools(options);
        BuildWorkFrames(options);

        auto trace = MakeChildAssembly(*machine_, kMachineTraceAssemblyName);
        nodes_[trace->Metadata().name] = trace;
        auto& work_mount = *components_[model_->WorkMountIndex()];
        for (const std::string_view name :
             {kTrajectoryAssemblyName, kWorkTraceAssemblyName}) {
            auto node = MakeChildAssembly(work_mount, name);
            nodes_[node->Metadata().name] = node;
        }
        SetActiveTool(setup.InitialTool());
    } catch (...) {
        Clear();
        throw;
    }
}

void MachineScene::BuildComponents() {
    components_.clear();
    for (std::size_t i = 0; i < model_->ComponentCount(); ++i) {
        components_.push_back(
                MakeChildAssembly(*machine_, model_->Component(i).name));
    }
}

void MachineScene::BuildGeometries(const MachiningSetup& setup,
                                   const SceneBuildOptions& options) {
    geometries_.clear();
    for (const GeometryInstance& instance : setup.Geometries()) {
        GeometryNode node;
        node.kind = instance.kind;
        node.owner = instance.owner;
        node.carrier = instance.carrier;
        node.role = instance.role;
        node.visible = instance.visible;
        node.assembly = TryLoadGeometry(instance, options.geometry,
                                        GeometryContext(instance), warnings_);
        if (node.assembly) {
            node.assembly->Metadata().name = GeometryNodeName(instance);
            node.assembly->Metadata().role_tag = GeometryRoleTag(instance);
            node.assembly->SetGlobalTransform(instance.placement);
            node.assembly->SetVisible(instance.visible);
            components_[instance.carrier]->AddChildAssembly(node.assembly);
        }
        geometries_.push_back(std::move(node));
    }
}

void MachineScene::BuildTriads() {
    auto machine_triad = MakeChildAssembly(*machine_, kMachineTriadName);
    AddTriadLines(*machine_triad, kTriadLength);
    nodes_[machine_triad->Metadata().name] = machine_triad;

    auto mount_triad = MakeChildAssembly(
            *components_[model_->ToolMountIndex()], kToolMountTriadName,
            model_->MountPlacement(MountKind::kToolMount));
    AddTriadLines(*mount_triad, kTriadLength);
    nodes_[mount_triad->Metadata().name] = mount_triad;
}

void MachineScene::BuildTools(const SceneBuildOptions& options) {
    auto& tool_mount = *components_[model_->ToolMountIndex()];
    const igesio::Matrix4d h_tm = model_->MountPlacement(MountKind::kToolMount);
    tool_assemblies_.clear();
    for (const auto& [number, spec] : tools_) {
        auto tool = MakeToolAssembly(spec);
        tool->SetGlobalTransform(h_tm * ToolMountOffset(spec));
        tool_mount.AddChildAssembly(tool);
        if (options.tool_axis_extra > 0.0) {
            AddToolAxisAndControlPoint(*tool, spec, options.tool_axis_extra);
        }
        tool_assemblies_[number] = tool;
    }
}

void MachineScene::BuildWorkFrames(const SceneBuildOptions& options) {
    for (const WorkFrame& frame : work_frames_) {
        auto& carrier = *components_[frame.carrier];
        if (options.build_work_frames) {
            auto triad = MakeChildAssembly(
                    carrier, PrefixedName(kWorkFrameAssemblyPrefix, frame.id),
                    frame.w0);
            AddTriadLines(*triad, kTriadLength);
            nodes_[triad->Metadata().name] = triad;
        }

        auto paths = MakeChildAssembly(
                carrier, PrefixedName(kPathsAssemblyPrefix, frame.id), frame.w0);
        MakeChildAssembly(*paths, kRapidPathsName)
                ->SetColorOverride(kRapidPathColor);
        MakeChildAssembly(*paths, kCutPathsName)->SetColorOverride(kCutPathColor);
        nodes_[paths->Metadata().name] = paths;

        auto attach = MakeChildAssembly(
                carrier, PrefixedName(kAttachAssemblyPrefix, frame.id), frame.w0);
        nodes_[attach->Metadata().name] = attach;
    }
}

void MachineScene::Clear() {
    if (!IsBuilt()) return;

    if (const auto root = root_.lock()) {
        root->RemoveChildAssembly(machine_->GetID(), i_mod::RemovalPolicy::kOrphan);
    }
    machine_.reset();
    root_.reset();
    model_.reset();
    tools_.clear();
    work_frames_.clear();
    initial_work_offset_.clear();
    components_.clear();
    geometries_.clear();
    tool_assemblies_.clear();
    nodes_.clear();
    active_tool_ = kNoTool;
    warnings_.clear();
}

void MachineScene::RequireBuilt(const std::string_view operation) const {
    if (IsBuilt()) return;
    throw std::logic_error(std::string(operation) + ": MachineScene is not built");
}

const MachineModel& MachineScene::Model() const {
    RequireBuilt("Model");
    return *model_;
}

std::shared_ptr<i_mod::Assembly> MachineScene::FindNode(
        const std::string_view name) const {
    const auto it = nodes_.find(std::string(name));
    return it == nodes_.end() ? nullptr : it->second;
}



/**
 * ---- 姿勢と工具 ----
 */

void MachineScene::ApplyPose(const JointVector& q) {
    RequireBuilt("ApplyPose");
    Forward(*model_, q, &forward_buffer_);
    for (std::size_t i = 0; i < components_.size(); ++i) {
        components_[i]->SetGlobalTransform(forward_buffer_[i]);
    }
}

void MachineScene::ResetToZeroPose() {
    RequireBuilt("ResetToZeroPose");
    for (const auto& component : components_) {
        component->SetGlobalTransform(igesio::Matrix4d::Identity());
    }
}

void MachineScene::SetActiveTool(const int number) {
    RequireBuilt("SetActiveTool");
    active_tool_ = tool_assemblies_.count(number) > 0 ? number : kNoTool;
    for (const auto& [tool_number, tool] : tool_assemblies_) {
        tool->SetVisible(tool_number == active_tool_);
    }
}



/**
 * ---- 可視性の切り替え ----
 */

void MachineScene::SetHolderVisible(const bool visible) {
    for (const auto& [number, tool] : tool_assemblies_) {
        SetToolPartVisible(*tool, ToolPart::kHolder, visible);
    }
}

void MachineScene::SetPathsVisible(const bool visible) {
    for (const WorkFrame& frame : work_frames_) {
        if (const auto paths = PathsAssembly(frame.id)) paths->SetVisible(visible);
    }
}

void MachineScene::SetRapidPathsVisible(const bool visible) {
    for (const WorkFrame& frame : work_frames_) {
        const auto paths = PathsAssembly(frame.id);
        if (!paths) continue;
        if (const auto rapid = FindChildAssembly(*paths, kRapidPathsName)) {
            rapid->SetVisible(visible);
        }
    }
}

void MachineScene::SetWorkFramesVisible(const bool visible) {
    for (const WorkFrame& frame : work_frames_) {
        if (const auto triad = WorkFrameAssembly(frame.id)) triad->SetVisible(visible);
    }
}

void MachineScene::SetTriadsVisible(const bool visible) {
    for (const std::string_view name : {kMachineTriadName, kToolMountTriadName}) {
        if (const auto triad = FindNode(name)) triad->SetVisible(visible);
    }
}

void MachineScene::SetToolAxisVisible(const bool visible) {
    for (const auto& [number, tool] : tool_assemblies_) {
        for (const std::string_view name : {kToolAxisLineName, kControlPointName}) {
            if (const auto child = FindChildAssembly(*tool, name)) {
                child->SetVisible(visible);
            }
        }
    }
}

void MachineScene::SetMachinePartsVisible(const bool visible) {
    for (const GeometryNode& node : geometries_) {
        if (!node.assembly || !node.visible) continue;
        if (node.kind == GeometryInstance::Kind::kMachinePart) {
            node.assembly->SetVisible(visible);
        }
    }
}

void MachineScene::SetModelRoleVisible(const ModelRole role, const bool visible) {
    for (const GeometryNode& node : geometries_) {
        if (!node.assembly || !node.visible) continue;
        if (node.role.has_value() && *node.role == role) {
            node.assembly->SetVisible(visible);
        }
    }
}



/**
 * ---- 経路線 ----
 */

void MachineScene::RebuildPaths(const ClProgram& program,
                                std::vector<Diagnostic>* warnings) {
    RequireBuilt("RebuildPaths");
    std::set<std::string> known;
    for (const WorkFrame& frame : work_frames_) {
        known.insert(frame.id);
        const auto paths = PathsAssembly(frame.id);
        for (const auto& child : paths->GetChildAssemblies()) child->Clear();
    }

    std::vector<Diagnostic> local;
    const std::vector<PathRun> runs = CollectPathRuns(
            program, initial_work_offset_, known, arc_chord_tolerance_,
            warnings != nullptr ? *warnings : local);
    for (const PathRun& run : runs) {
        const auto paths = PathsAssembly(run.work_offset);
        const auto target = FindChildAssembly(
                *paths, run.rapid ? kRapidPathsName : kCutPathsName);
        target->AddEntity(i_ent::MakeLinearPath(run.points));
    }
}



/**
 * ---- アセンブリの取得 ----
 */

std::shared_ptr<i_mod::Assembly> MachineScene::ComponentAssembly(
        const std::string_view name) const {
    if (!IsBuilt()) return nullptr;
    const std::optional<std::size_t> index = model_->FindComponent(name);
    return index.has_value() ? components_[*index] : nullptr;
}

std::shared_ptr<i_mod::Assembly> MachineScene::ToolMountAssembly() const {
    return IsBuilt() ? components_[model_->ToolMountIndex()] : nullptr;
}

std::shared_ptr<i_mod::Assembly> MachineScene::WorkMountAssembly() const {
    return IsBuilt() ? components_[model_->WorkMountIndex()] : nullptr;
}

std::shared_ptr<i_mod::Assembly> MachineScene::ToolAssembly(const int number) const {
    const auto it = tool_assemblies_.find(number);
    return it == tool_assemblies_.end() ? nullptr : it->second;
}

std::map<int, ObjectID> MachineScene::ToolAssemblyIds() const {
    std::map<int, ObjectID> ids;
    for (const auto& [number, tool] : tool_assemblies_) ids[number] = tool->GetID();
    return ids;
}

std::shared_ptr<i_mod::Assembly> MachineScene::GeometryAssembly(
        const std::size_t index) const {
    return index < geometries_.size() ? geometries_[index].assembly : nullptr;
}

std::shared_ptr<i_mod::Assembly> MachineScene::ModelAssembly(
        const std::string_view name) const {
    for (const GeometryNode& node : geometries_) {
        if (node.kind == GeometryInstance::Kind::kModel && node.owner == name) {
            return node.assembly;
        }
    }
    return nullptr;
}

std::shared_ptr<i_mod::Assembly> MachineScene::WorkFrameAssembly(
        const std::string_view id) const {
    return FindNode(PrefixedName(kWorkFrameAssemblyPrefix, id));
}

std::shared_ptr<i_mod::Assembly> MachineScene::PathsAssembly(
        const std::string_view id) const {
    return FindNode(PrefixedName(kPathsAssemblyPrefix, id));
}

std::shared_ptr<i_mod::Assembly> MachineScene::AttachAssembly(
        const std::string_view id) const {
    return FindNode(PrefixedName(kAttachAssemblyPrefix, id));
}

std::shared_ptr<i_mod::Assembly> MachineScene::TrajectoryAssembly() const {
    return FindNode(kTrajectoryAssemblyName);
}

std::shared_ptr<i_mod::Assembly> MachineScene::MachineTraceAssembly() const {
    return FindNode(kMachineTraceAssemblyName);
}

std::shared_ptr<i_mod::Assembly> MachineScene::WorkTraceAssembly() const {
    return FindNode(kWorkTraceAssemblyName);
}

std::vector<ObjectID> MachineScene::MetallicSurfaceIds() const {
    std::vector<ObjectID> ids;
    for (const auto& [number, tool] : tool_assemblies_) {
        for (const ToolPart part : {ToolPart::kCutter, ToolPart::kShank}) {
            const auto container = FindToolPart(*tool, part);
            if (!container) continue;
            for (const auto& surface : container->FindEntitiesByType(
                         i_ent::EntityType::kSurfaceOfRevolution, true)) {
                ids.push_back(surface->GetID());
            }
        }
    }
    return ids;
}



/**
 * ---- ワークビューの支援 ----
 */

igesio::Matrix4d MachineScene::WorkMountWorldTransform() const {
    RequireBuilt("WorkMountWorldTransform");
    return components_[model_->WorkMountIndex()]->GetWorldTransform();
}

std::vector<ObjectID> MachineScene::WorkViewHiddenIds(
        const WorkViewOptions& options) const {
    std::vector<ObjectID> ids;
    if (!IsBuilt()) return ids;

    const std::size_t work_mount = model_->WorkMountIndex();
    for (const GeometryNode& node : geometries_) {
        if (!node.assembly || node.kind != GeometryInstance::Kind::kMachinePart) {
            continue;
        }
        if (options.show_work_mount_parts && node.carrier == work_mount) continue;
        ids.push_back(node.assembly->GetID());
    }
    for (const std::string_view name :
         {kMachineTriadName, kMachineTraceAssemblyName}) {
        if (const auto node = FindNode(name)) ids.push_back(node->GetID());
    }
    return ids;
}

std::vector<ObjectID> MachineScene::MachineViewHiddenIds() const {
    std::vector<ObjectID> ids;
    if (const auto node = TrajectoryAssembly()) ids.push_back(node->GetID());
    return ids;
}

}  // namespace igesio::extensions::machines
