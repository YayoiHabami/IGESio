/**
 * @file extensions/machines/project/setup.cpp
 * @brief 加工セットアップ (プロジェクト定義をもとに構築する実行時情報)
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 構築順: 運動学モデル → 初期姿勢 → 工具表 → 取り付け名の解決 →
 *       全形状のゼロポーズ機械座標への同次変換 (`Geometries()`) →
 *       工具オフセットの実効値 → `[[collision.machine_pair]]`のペア規則.
 *       取り付け名の解決では、ワーク座標系・モデルのゼロポーズ機械座標への
 *       同次変換の計算、および閉路の検出も行う.
 */
#include "igesio/extensions/machines/project/setup.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/tools/tool_profile.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 取り付け先の解決結果
///        (所属コンポーネント, 取り付け先座標系→ゼロポーズ機械座標の同次変換A)
using AttachFrame = std::pair<std::size_t, igesio::Matrix4d>;

/// @brief 仕様違反を`DataFormatError`として投げる
/// @param context 発生箇所 (`"[[model]](stock)"`等)
/// @param message 内容
/// @param line 定義側のTOML行番号 (0なら省略)
/// @throw igesio::DataFormatError 常に投げる
[[noreturn]] void Fail(const std::string& context, const std::string& message,
                       const int line) {
    std::string text = context + ": " + message;
    if (line > 0) text += " (line " + std::to_string(line) + ")";
    throw igesio::DataFormatError(text);
}

/// @brief 警告を追加する
/// @param warnings 警告の集計先
/// @param context 発生箇所 (`"[[tool]](#1)"`等)
/// @param message 内容
/// @param line 定義側のTOML行番号 (0なら省略)
void Warn(std::vector<Diagnostic>& warnings, const std::string& context,
          const std::string& message, const int line = 0) {
    warnings.push_back(Diagnostic{Severity::kWarning, context, message, line});
}

/// @brief 他の個所で作られた警告を文脈と行番号を付け替えて転記する
/// @param source 元の警告
/// @param context 発生箇所 (`"[[tool]](#1)"`等)
/// @param line 定義側のTOML行番号 (0なら省略)
/// @param warnings 警告の集計先
void ForwardWarnings(const std::vector<Diagnostic>& source,
                     const std::string& context, const int line,
                     std::vector<Diagnostic>& warnings) {
    for (const Diagnostic& diagnostic : source) {
        warnings.push_back(Diagnostic{diagnostic.severity, context,
                                      diagnostic.message, line});
    }
}

/// @brief `[[tool]]`の文脈文字列
std::string ToolContext(const int number) {
    return "[[tool]](#" + std::to_string(number) + ")";
}

/// @brief `[[work_offset]]`の文脈文字列
std::string WorkOffsetContext(const std::string& id) {
    return "[[work_offset]](" + id + ")";
}

/// @brief `[[model]]`の文脈文字列
std::string ModelContext(const std::string& name) {
    return "[[model]](" + name + ")";
}



/**
 * ---- 工具表 ----
 */

/// @brief ライブラリ参照工具の参照先ライブラリを取得する
/// @param project プロジェクト定義
/// @param ref ライブラリ参照形式の工具
/// @return 別名が空でライブラリが1つならそのライブラリ. 無ければ`nullptr`
const ToolLibrarySpec* LibraryOf(const ProjectDefinition& project,
                                 const LibraryToolRef& ref) {
    if (!ref.source.empty()) return FindToolLibrary(project, ref.source);
    return project.tool_libraries.size() == 1 ? &project.tool_libraries.front()
                                              : nullptr;
}

/// @brief 簡易アセンブリ形式の工具を解決する
/// @param entry 工具表の項目
/// @param simple 簡易アセンブリ形式の工具仕様
/// @param warnings 警告の集計先
/// @return 解決した工具 (名前は`entry.name`、空なら切れ刃の既定名)
/// @throw igesio::DataFormatError 幾何が不正な場合 (読込済みの定義では起きない)
ToolAssemblySpec ResolveSimpleTool(
        const ToolEntry& entry, const SimpleToolSpec& simple,
        std::vector<Diagnostic>& warnings) {
    ToolAssemblySpec spec;
    std::vector<Diagnostic> local;
    try {
        spec.profile = MakeSimpleToolProfile(simple, &local);
    } catch (const std::invalid_argument& e) {
        Fail(ToolContext(entry.number) + ".simple", e.what(), entry.line);
    }
    ForwardWarnings(local, ToolContext(entry.number), entry.line, warnings);
    spec.name = entry.name.empty() ? std::string(SimpleCutterName(simple.cutter))
                                   : entry.name;
    return spec;
}

/// @brief ライブラリ参照形式の工具をコールバックで解決する
/// @param project プロジェクト定義
/// @param entry 工具表の項目
/// @param ref ライブラリ参照形式の工具仕様
/// @param options セットアップの構築設定
/// @param warnings 警告の集計先
/// @return 解決できなければ`std::nullopt` (警告を追加する)
/// @note ゲージラインは`gauge_length`があればその値、無ければ輪郭から確定して
///       `gauge_line_z`に書き込む (下流でフォールバック・警告を再現しない)
std::optional<ToolAssemblySpec> ResolveLibraryTool(
        const ProjectDefinition& project, const ToolEntry& entry,
        const LibraryToolRef& ref, const SetupOptions& options,
        std::vector<Diagnostic>& warnings) {
    const std::string context = ToolContext(entry.number);
    const ToolLibrarySpec* library = LibraryOf(project, ref);
    std::optional<ToolAssemblySpec> resolved;
    if (library != nullptr && options.tool_resolver) {
        resolved = options.tool_resolver(*library, ref);
    }
    if (!resolved.has_value()) {
        Warn(warnings, context,
             "library tool #" + std::to_string(entry.number)
             + " is unresolved; the control point falls back to the gauge point",
             entry.line);
        return std::nullopt;
    }
    if (!entry.name.empty()) resolved->name = entry.name;
    if (!entry.gauge_length.has_value()) {
        std::vector<Diagnostic> local;
        resolved->profile.gauge_line_z = resolved->profile.GaugeLength(&local);
        ForwardWarnings(local, context, entry.line, warnings);
    }
    return resolved;
}

/// @brief 工具表を作る (解決済みのみ)
/// @param project プロジェクト定義
/// @param options セットアップの構築設定
/// @param warnings 警告の集計先
/// @return 工具番号→解決済みの工具 (未解決のライブラリ参照は含まない)
/// @throw igesio::DataFormatError 簡易アセンブリの幾何が不正な場合
std::map<int, ToolAssemblySpec> ResolveTools(
        const ProjectDefinition& project, const SetupOptions& options,
        std::vector<Diagnostic>& warnings) {
    std::map<int, ToolAssemblySpec> tools;
    for (const ToolEntry& entry : project.tools) {
        std::optional<ToolAssemblySpec> spec;
        if (const auto* simple = std::get_if<SimpleToolSpec>(&entry.source);
            simple != nullptr) {
            spec = ResolveSimpleTool(entry, *simple, warnings);
        } else {
            spec = ResolveLibraryTool(project, entry,
                                      std::get<LibraryToolRef>(entry.source),
                                      options, warnings);
        }
        if (!spec.has_value()) continue;
        spec->number = entry.number;
        spec->control_point = entry.control_point;
        if (entry.gauge_length.has_value()) {
            spec->profile.gauge_line_z = *entry.gauge_length;
        }
        tools.emplace(entry.number, std::move(*spec));
    }
    return tools;
}



/**
 * ---- 取り付け先名の解決 ----
 */

/// @brief 取り付け先の名前の解決状態 (およびメモ化と閉路検出)
struct Resolver {
    /// @brief プロジェクト定義
    const ProjectDefinition& project;
    /// @brief 運動学モデル
    const MachineModel& model;
    /// @brief 初期姿勢の軸変位量 (登録値形式のチェーン外の軸に用いる)
    const JointVector& base_q;
    /// @brief 解決済みの名前 (メモ)
    std::map<std::string, AttachFrame>& frames;
    /// @brief 解決中の名前 (閉路検出)
    std::vector<std::string> visiting;
};

/// @brief 取り付け先の名前を解決する
///        (`ResolveWorkOffset`と相互再帰するため前方宣言、詳細は定義側)
AttachFrame Resolve(Resolver& resolver, const std::string& name,
                    const std::string& context, int line);

/// @brief 登録値形式のワーク座標→ゼロポーズ機械座標の同次変換W_0を計算する
/// @param resolver 名前解決の状態
/// @param spec ワークオフセットとワーク座標系
/// @param values `[[work_offset]].values`の登録値 (チェーン上の軸のNC値)
/// @param carrier ワーク座標系の所属コンポーネント (work_mount) の添字
/// @return W_0
/// @throw igesio::DataFormatError `values`にチェーン外の軸がある場合
/// @note W_0 = F_a(q*)⁻¹ · T(p_from). q*はチェーン上の軸を`values` (省略0),
///       チェーン外の軸を`base_q`とした軸変位量. p_fromはq*における基準点の
///       機械座標. F(0) = Iなので補正項は不要
igesio::Matrix4d RegisteredWorkFrame(
        const Resolver& resolver, const WorkOffsetSpec& spec,
        const NcValues& values, const std::size_t carrier) {
    const MachineModel& model = resolver.model;
    const std::string context = WorkOffsetContext(spec.id) + ".values";
    const std::vector<std::string>& chain = model.ChainRegisters();
    for (const NcEntry& entry : values.Entries()) {
        if (std::find(chain.begin(), chain.end(), entry.register_name) == chain.end()) {
            Fail(context, entry.register_name + " is not on the kinematic chain",
                 spec.line);
        }
    }
    NcValues nc;
    for (const std::string& register_name : chain) {
        nc.Set(register_name, values.GetOr(register_name, 0.0));
    }
    const std::vector<igesio::Matrix4d> frames =
            Forward(model, JointsFromNc(model, nc, resolver.base_q));
    igesio::Vector3d p_from = igesio::Vector3d::Zero();
    if (spec.from == WorkOffsetFrom::kToolMount) {
        p_from = TranslationPart(frames[model.ToolMountIndex()]
                                 * model.MountPlacement(MountKind::kToolMount));
    }
    return RigidInverse(frames[carrier]) * Translation(p_from);
}

/// @brief ワークオフセットのワーク座標系を解決する
/// @param resolver 名前解決の状態
/// @param spec ワークオフセット
/// @return (work_mountの添字, ワーク座標→ゼロポーズ機械座標の同次変換W_0)
/// @throw igesio::DataFormatError `attach`がwork_mountに至らない場合
///        (名前の不在・閉路は`Resolve`から伝播)
AttachFrame ResolveWorkOffset(Resolver& resolver, const WorkOffsetSpec& spec) {
    const std::string context = WorkOffsetContext(spec.id);
    const AttachFrame attach = Resolve(resolver, spec.attach, context, spec.line);
    if (attach.first != resolver.model.WorkMountIndex()) {
        Fail(context, "attach does not lead to work_mount", spec.line);
    }
    if (const auto* values = std::get_if<NcValues>(&spec.placement); values != nullptr) {
        return {attach.first, RegisteredWorkFrame(resolver, spec, *values, attach.first)};
    }
    const GeometricPlacement& placement = std::get<GeometricPlacement>(spec.placement);
    return {attach.first, attach.second * PlacementMatrix(placement)};
}

/// @brief 予約語・コンポーネント名として解決する
/// @param model 運動学モデル
/// @param name 取り付け先の名前 (`work_mount`/`tool_mount` or コンポーネント名)
/// @return 該当しなければ`std::nullopt`
/// @note 予約語、および`mount_placement`を持つコンポーネントの場合は,
///       変換は取り付け先座標系のH、それ以外はコンポーネント座標系のC_c
std::optional<AttachFrame> ResolveMachineName(const MachineModel& model,
                                              const std::string& name) {
    if (name == kWorkMountAttach) {
        return AttachFrame{model.WorkMountIndex(),
                           model.MountPlacement(MountKind::kWorkMount)};
    }
    if (name == kToolMountAttach) {
        return AttachFrame{model.ToolMountIndex(),
                           model.MountPlacement(MountKind::kToolMount)};
    }
    const std::optional<std::size_t> index = model.FindComponent(name);
    if (!index.has_value()) return std::nullopt;
    const ComponentInfo& component = model.Component(*index);
    if (component.mount_placement.has_value()) {
        return AttachFrame{*index, *component.mount_placement};
    }
    return AttachFrame{*index, component.local_frame};
}

/// @brief 取り付け先の名前を解決する (メモ化および閉路検出も行う)
/// @param resolver 名前解決の状態 (メモと解決中の名前列を更新する)
/// @param name 取り付け先の名前 (予約語/コンポーネント名/モデル名/ワークオフセットID)
/// @param context 参照元の文脈 (エラー文言用)
/// @param line 参照元の行番号
/// @return 解決結果 (所属コンポーネント, 取り付け先座標系→ゼロポーズ機械座標の同次変換)
/// @throw igesio::DataFormatError 名前が無い、または閉路の場合
AttachFrame Resolve(Resolver& resolver, const std::string& name,
                    const std::string& context, const int line) {
    if (const auto it = resolver.frames.find(name); it != resolver.frames.end()) {
        return it->second;
    }
    if (std::find(resolver.visiting.begin(), resolver.visiting.end(), name)
        != resolver.visiting.end()) {
        std::string chain;
        for (const std::string& visited : resolver.visiting) chain += visited + " -> ";
        Fail(context, "attach forms a cycle: " + chain + name, line);
    }
    std::optional<AttachFrame> frame = ResolveMachineName(resolver.model, name);
    if (!frame.has_value()) {
        resolver.visiting.push_back(name);
        if (const ModelSpec* model = FindModel(resolver.project, name); model != nullptr) {
            const AttachFrame parent = Resolve(resolver, model->attach,
                                               ModelContext(name), model->line);
            frame = AttachFrame{parent.first,
                                parent.second * PlacementMatrix(model->placement)};
        } else if (const WorkOffsetSpec* offset = FindWorkOffset(resolver.project, name);
                   offset != nullptr) {
            frame = ResolveWorkOffset(resolver, *offset);
        } else {
            Fail(context, "attach does not exist: " + name, line);
        }
        resolver.visiting.pop_back();
    }
    resolver.frames.emplace(name, *frame);
    return *frame;
}



/**
 * ---- 干渉ペアの規則 ----
 */

/// @brief 干渉ペアの対象1つをコンポーネント集合へ展開する
/// @param model 運動学モデル
/// @param target 対象の名前 (コンポーネント名、または予約名`tool`・`work`)
/// @param subtree 子孫コンポーネントも含めるか
/// @return コンポーネント名 (および仮想メンバ) の集合
/// @note 予約名は仮想メンバ`@tool`・`@work`で表し、グループにマウントが
///       含まれる場合も対応する仮想メンバを加える (`machine_io`と同じ規則)
std::set<std::string> CollisionGroup(
        const MachineModel& model, const std::string& target,
        const bool subtree) {
    if (target == kToolCollisionTarget) return {"@tool"};
    if (target == kWorkCollisionTarget) return {"@work"};
    std::set<std::string> group = {target};
    if (subtree) {
        std::vector<std::size_t> stack = {*model.FindComponent(target)};
        while (!stack.empty()) {
            const std::size_t index = stack.back();
            stack.pop_back();
            for (const std::size_t child : model.Component(index).children) {
                group.insert(model.Component(child).name);
                stack.push_back(child);
            }
        }
    }
    if (group.count(model.Component(model.ToolMountIndex()).name)) {
        group.insert("@tool");
    }
    if (group.count(model.Component(model.WorkMountIndex()).name)) {
        group.insert("@work");
    }
    return group;
}

/// @brief `[[collision.machine_pair]]`のペア規則
///        (予約名への`subtree`、重複、グループ交差) を検証する
/// @param project プロジェクト定義
/// @param model 運動学モデル
/// @throw igesio::DataFormatError 規則に反するペアがある場合
void ValidateMachinePairs(const ProjectDefinition& project,
                          const MachineModel& model) {
    if (!project.collision.has_value()) return;
    const std::string context = "[[collision.machine_pair]]";
    std::set<std::set<std::pair<std::string, bool>>> seen;
    for (const MachinePairOverride& pair : project.collision->machine_pairs) {
        std::array<std::set<std::string>, 2> groups;
        for (std::size_t k = 0; k < 2; ++k) {
            if (IsReservedCollisionTarget(pair.targets[k]) && pair.subtree[k]) {
                Fail(context, "subtree = true cannot be specified "
                              "for the reserved name " + pair.targets[k], 0);
            }
            groups[k] = CollisionGroup(model, pair.targets[k], pair.subtree[k]);
        }
        const std::string names = "[" + pair.targets[0] + ", "
                                + pair.targets[1] + "]";
        const std::set<std::pair<std::string, bool>> key = {
                {pair.targets[0], pair.subtree[0]},
                {pair.targets[1], pair.subtree[1]}};
        if (!seen.insert(key).second) {
            Fail(context, "duplicate collision pair: " + names, 0);
        }
        std::vector<std::string> intersection;
        std::set_intersection(groups[0].begin(), groups[0].end(),
                              groups[1].begin(), groups[1].end(),
                              std::back_inserter(intersection));
        if (!intersection.empty()) {
            Fail(context, "the two groups of the collision pair intersect: " +
                          names + " common=[" + intersection.front() + "]", 0);
        }
    }
}



/**
 * ---- 工具オフセット ----
 */

/// @brief 工具オフセットの実効値を計算する (省略値は工具のゲージ長・最大半径)
/// @param project プロジェクト定義
/// @param tools 解決済みの工具表 (`ResolveTools`の結果)
/// @param warnings 警告の集計先
/// @return オフセット番号→実効値
std::map<int, ResolvedToolOffset> ResolveToolOffsets(
        const ProjectDefinition& project,
        const std::map<int, ToolAssemblySpec>& tools,
        std::vector<Diagnostic>& warnings) {
    std::map<int, ResolvedToolOffset> offsets;
    for (const ToolOffsetEntry& entry : project.tool_offsets) {
        ResolvedToolOffset resolved;
        resolved.number = entry.number;
        resolved.tool = entry.tool;
        resolved.length_wear = entry.length_wear;
        resolved.radius_wear = entry.radius_wear;
        const ToolAssemblySpec* tool = nullptr;
        if (entry.tool.has_value()) {
            const auto it = tools.find(*entry.tool);
            tool = it == tools.end() ? nullptr : &it->second;
            const bool needs_tool = !entry.length.has_value() ||
                                    !entry.radius.has_value();
            if (tool == nullptr && needs_tool) {
                Warn(warnings,
                     "[[tool_offset]](#" + std::to_string(entry.number) + ")",
                     "tool #" + std::to_string(*entry.tool)
                     + " is unresolved; length defaults to 0", entry.line);
            }
        }
        resolved.length = entry.length.has_value()
                ? *entry.length
                : (tool == nullptr ? 0.0 : tool->profile.GaugeLength(nullptr));
        resolved.radius = entry.radius.has_value()
                ? *entry.radius
                : (tool == nullptr ? 0.0 : tool->profile.MaxRadius());
        offsets.emplace(entry.number, resolved);
    }
    return offsets;
}



/**
 * ---- 形状のゼロポーズ機械座標への同次変換 ----
 */

/// @brief 機械コンポーネントの形状とモデルをゼロポーズに置いた並びを作る
/// @param model 運動学モデル
/// @param models 取り付け先を解決済みのモデル
/// @return 機械部品 (コンポーネント順、各コンポーネント内は定義順)、
///         続いてモデル (`models`の順)
/// @note 機械部品は`C_c · T(origin) · R`、モデルは`PlacedModel::placement`
std::vector<GeometryInstance> PlaceGeometries(
        const MachineModel& model, const std::vector<PlacedModel>& models) {
    std::vector<GeometryInstance> instances;
    for (std::size_t i = 0; i < model.ComponentCount(); ++i) {
        const ComponentInfo& component = model.Component(i);
        for (const GeometryEntry& entry : model.Spec(i).geometries) {
            GeometryInstance instance;
            instance.kind = GeometryInstance::Kind::kMachinePart;
            instance.owner = component.name;
            instance.carrier = i;
            instance.geometry = entry.geometry;
            instance.placement = component.local_frame * PlacementMatrix(entry.placement);
            instance.collision = entry.collision;
            instance.visible = entry.visible;
            instances.push_back(std::move(instance));
        }
    }
    for (const PlacedModel& placed : models) {
        GeometryInstance instance;
        instance.kind = GeometryInstance::Kind::kModel;
        instance.owner = placed.spec.name;
        instance.carrier = placed.carrier;
        instance.role = placed.spec.role;
        instance.geometry = placed.spec.geometry;
        instance.placement = placed.placement;
        instance.collision = placed.spec.collision;
        instance.visible = placed.spec.visible;
        instances.push_back(std::move(instance));
    }
    return instances;
}

}  // namespace



MachiningSetup::MachiningSetup(const ProjectDefinition& project,
                               const SetupOptions& options)
    : project_(project), model_(project.machine) {
    base_q_ = JointsFromNc(model_, project_.initial_axes, InitialJoints(model_));
    tools_ = ResolveTools(project_, options, warnings_);

    Resolver resolver{project_, model_, base_q_, attach_frames_, {}};
    if (project_.work_offsets.empty()) {
        WorkOffsetSpec implicit;
        implicit.id = std::string(kImplicitWorkOffsetId);
        const AttachFrame frame = ResolveWorkOffset(resolver, implicit);
        work_frames_.push_back(WorkFrame{implicit.id, frame.first, frame.second});
    }
    for (const WorkOffsetSpec& spec : project_.work_offsets) {
        const AttachFrame frame = Resolve(resolver, spec.id, WorkOffsetContext(spec.id),
                                          spec.line);
        work_frames_.push_back(WorkFrame{spec.id, frame.first, frame.second});
    }
    for (const ModelSpec& spec : project_.models) {
        const AttachFrame frame =
                Resolve(resolver, spec.name, ModelContext(spec.name), spec.line);
        models_.push_back(PlacedModel{spec, frame.first, frame.second});
        if (spec.role != ModelRole::kDisplay &&
            frame.first != model_.WorkMountIndex()) {
            Warn(warnings_, ModelContext(spec.name),
                 "the " + std::string(ModelRoleName(spec.role))
                 + " model is not attached under work_mount", spec.line);
        }
    }
    geometries_ = PlaceGeometries(model_, models_);
    initial_work_offset_ =
            project_.initial_work_offset.value_or(work_frames_.front().id);
    tool_offsets_ = ResolveToolOffsets(project_, tools_, warnings_);
    ValidateMachinePairs(project_, model_);
}

const WorkFrame* MachiningSetup::FindWorkFrame(const std::string_view id) const {
    for (const WorkFrame& frame : work_frames_) {
        if (frame.id == id) return &frame;
    }
    return nullptr;
}

std::pair<std::size_t, igesio::Matrix4d> MachiningSetup::ResolveAttach(
        const std::string_view name) const {
    const auto it = attach_frames_.find(std::string(name));
    if (it != attach_frames_.end()) return it->second;
    // コンポーネント名は取り付け先として参照されていなければメモに無いので、別途解く
    const std::optional<AttachFrame> frame =
            ResolveMachineName(model_, std::string(name));
    if (!frame.has_value()) {
        throw std::invalid_argument("MachiningSetup::ResolveAttach: "
                                    "unknown name: " + std::string(name));
    }
    return *frame;
}

}  // namespace igesio::extensions::machines
