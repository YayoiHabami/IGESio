/**
 * @file extensions/machines/machine/machine_model.h
 * @brief 機械定義から構築する運動学モデル (木構造・チェーン・軸一覧・派生量)
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note `MachineDefinition`をもとに、順/逆運動学の計算に必要な事前計算や
 *       データ構造の整理などを済ませた構造体である`MachineModel`を提供する.
 *       `MachineDefinition`では機械の仕様をファイルでの定義通りに保持するため,
 *       baseを先頭とする親が先に並ぶコンポーネント列、ゼロポーズ機械座標での軸一覧,
 *       工具側・ワーク側チェーン、および運動学が使う派生量を構築時に計算する.
 * @note 機械の軸の値の表現 (`JointVector`/`NcValues`) およびσの設定については
 *       `axis_values.h`を参照のこと.
 * @note コンポーネント (MachineModel::Component(i)) は、baseを先頭として
 *       深さ優先で親コンポーネントが子よりも前に来るように並べる.
 * @note 本クラスは`MachineDefinition`のコピーを所有し、構築後は変更しない.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_MODEL_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_MODEL_H_

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/machine_definition.h"

namespace igesio::extensions::machines {

/// @brief 軸の種別
enum class AxisKind {
    /// @brief 直進軸
    kLinear,
    /// @brief 回転軸
    kRotary,
};

/// @brief 取り付けフレームの種別
enum class MountKind {
    /// @brief 工具取り付け点 (`tool_mount`)
    kToolMount,
    /// @brief ワーク取り付け点 (`work_mount`)
    kWorkMount,
};

/// @brief 可動軸1本の情報 (ゼロポーズ機械座標)
/// @note `AxisSpec`について、順運動学/逆運動学の計算に必要な事前計算を行った結果.
/// @note `limits`/`initial`/`wrap_start`/`dynamics`は`AxisSpec`の値の複製.
///       逆運動学側で`MachineDefinition`を参照せずに済むようにする
struct AxisInfo {
    /// @brief 軸名 (NCのレジスタ名; "X","A"等)
    std::string register_name;
    /// @brief 所属コンポーネントのインデックス
    /// @note `MachineModel::Component()`におけるインデックス
    std::size_t component_index = 0;

    /// @brief 軸の種別 (直進/回転)
    AxisKind kind = AxisKind::kLinear;
    /// @brief 軸方向 (機械座標系. `C_c.R · direction`を正規化したもの)
    /// @note kLinearの場合は軸の正方向、kRotaryの場合は回転軸 (右ねじ).
    igesio::Vector3d direction_world = igesio::Vector3d::UnitZ();
    /// @brief 軸上の1点 (機械座標系. `C_c · point`)
    /// @note 回転軸の場合のみ有効. 直進軸の場合はゼロ
    igesio::Vector3d point_world = igesio::Vector3d::Zero();

    /// @brief 可動範囲 `[min, max]` (NC指令値の下限/上限) [mm] or [rad]
    std::optional<std::array<double, 2>> limits;
    /// @brief 可動範囲が無制限か
    bool unlimited = false;
    /// @brief NC指令値の正規化区間の下端 [rad] (回転軸かつ無制限のみ)
    std::optional<double> wrap_start;
    /// @brief NC指令値の既定値
    double initial = 0.0;
    /// @brief NC指令値の正規化区間 [mm] or [rad]
    /// @note `limits`があればその範囲,
    ///       制限がなく`wrap_start`があれば[wrap_start, wrap_start + 2π),
    ///       どちらも無ければ`std::nullopt`
    std::optional<std::array<double, 2>> nc_range;

    /// @brief チェーン側符号 (ワーク側チェーン上の場合は-1、それ以外なら+1)
    /// @note 軸変位量 = sigma × NC指令値
    double sigma = 1.0;
    /// @brief 工具側チェーン上にあるか
    bool on_tool_chain = false;
    /// @brief ワーク側チェーン上にあるか
    bool on_work_chain = false;

    /// @brief 軸の動特性
    AxisDynamics dynamics;

    /// @brief 逆運動学の対象か
    /// @return 一方のチェーンにのみ属する軸なら`true`
    /// @note 工具側・ワーク側のどちらか一方のチェーン上にある軸のみ.
    ///       両チェーンに共通する軸は相対運動で相殺されるため対象外、
    ///       チェーン外の軸も対象外
    bool IsIkTarget() const { return on_tool_chain != on_work_chain; }
};

/// @brief NC指令値が軸の可動範囲内に入っているか
/// @param axis 対象の軸
/// @param nc NC指令値 [mm] or [rad]
/// @return `limits`に`kLimitTolerance`の許容誤差込みで収まっていれば`true`.
///         `limits`を持たない軸 (無制限) は常に`true`
/// @note 可動範囲の検査はこの関数で行う. 回転軸について2πの周期性を用いて
///       指令値を範囲内に収める場合は`WrapAngleIntoLimits`を用いる
bool IsWithinLimits(const AxisInfo& axis, double nc);

/// @brief 回転軸のNC指令値を軸の正規化区間内に収める
/// @param nc_rad NC指令値 [rad]
/// @param axis 対象の回転軸
/// @return 可動範囲の制限がない場合、`wrap_start`があれば
///         [wrap_start, wrap_start + 2π) の範囲内に収めた値、無ければそのまま.
///         `limits`が指定されていれば 0, ±2π を加えて範囲内に収まる最初の値を返す.
///         いずれも入らなければ`std::nullopt`
/// @throw std::invalid_argument `axis.kind`が`AxisKind::kRotary`でない場合
/// @note 直進軸の可動範囲には2πの周期がないため、`IsWithinLimits`を用いること
std::optional<double> WrapAngleIntoLimits(double nc_rad, const AxisInfo& axis);

/// @brief 運動学ツリーの1節点
/// @note `ComponentSpec`について、順運動学/逆運動学の計算に必要な事前計算を行った結果.
struct ComponentInfo {
    /// @brief 名前
    std::string name;
    /// @brief コンポーネントの種別
    ComponentType type = ComponentType::kFixed;
    /// @brief 親コンポーネントのインデックス
    /// @note `MachineModel::Component()`におけるインデックス. baseの場合はnullopt
    std::optional<std::size_t> parent;
    /// @brief 子コンポーネントのインデックス (宣言順)
    /// @note 各要素は`MachineModel::Component()`におけるインデックス
    std::vector<std::size_t> children;
    /// @brief 読込元の機械定義ファイルにおける記載順序
    /// @note `MachineModel::Definition().components`におけるインデックス
    std::size_t definition_index = 0;
    /// @brief 可動軸のインデックス
    /// @note `MachineModel::Axes()`におけるインデックス.　軸を持たない場合はnullopt
    std::optional<std::size_t> axis;

    /// @brief ローカル座標系の配置C_c
    /// @note このコンポーネント内の座標値の基準である、ローカル座標系の定義.
    ///       単位行列の場合は、ローカル座標はゼロポーズ機械座標系と一致する.
    ///       axisやgeometryの座標値はこのローカル座標系で表現される.
    /// @note ローカル座標の点pcと機械座標の点pm (同次座標) の関係は
    ///       `pm = C_c · pc`, `pc = C_c⁻¹ · pm`.
    igesio::Matrix4d local_frame = igesio::Matrix4d::Identity();
    /// @brief 取り付け先座標系→ゼロポーズ機械座標への剛体変換H (typeがkMountの場合のみ)
    /// @note 工具やワークの取り付け座標系上の点は、この行列のみを掛けて
    ///       ゼロポーズ機械座標系に変換できる.
    std::optional<igesio::Matrix4d> mount_placement;
};

/// @brief 機械の運動学モデル
/// @note 構築後は不変. コピー可能で、元の`MachineDefinition`には依存しない
/// @note 順/逆運動学の計算は`forward_kinematics.h`/`inverse_kinematics.h`で行う.
///       本クラスは構築後に計算を行わないため、全軸の変位量`q`を引数としない
class MachineModel {
 public:
    /// @brief 機械定義からモデルを構築する
    /// @param definition 機械定義
    /// @throw std::invalid_argument 構造が矛盾している場合 (
    ///        名前が空/重複, baseが1つでない, 親のないコンポーネントが存在する,
    ///        閉路が存在する, typeと`axis`/`frame_placement`が不整合,
    ///        rotaryの`point`が欠落, 各mountの1つでない,
    ///        registerが空/重複、`direction`がゼロ).
    /// @note 幾何的な検証 (可動範囲、rank、右手系等) は行わない. 読込側で行うこと.
    explicit MachineModel(MachineDefinition definition);

    /// @brief 構築元の機械定義 (コンポーネントはファイルでの定義順と同じ)
    /// @return 構築時に受け取った機械定義
    const MachineDefinition& Definition() const { return definition_; }

    /// @brief コンポーネント数 (暗黙のbaseを含む)
    /// @return `Component()`で参照できるコンポーネントの数
    std::size_t ComponentCount() const { return components_.size(); }
    /// @brief コンポーネント
    /// @param index コンポーネントのインデックス
    /// @return 指定したコンポーネントの情報
    /// @throw std::out_of_range インデックスが範囲外の場合
    /// @note baseを先頭として、深さ優先で親が子要素よりも前に来るように並ぶ
    const ComponentInfo& Component(std::size_t index) const;
    /// @brief コンポーネントの定義 (形状・spindle等の参照用)
    /// @param index コンポーネントのインデックス (`Component()`と同じ順序)
    /// @return 対応する`MachineDefinition::components`の要素
    /// @throw std::out_of_range インデックスが範囲外の場合
    const ComponentSpec& Spec(std::size_t index) const;
    /// @brief コンポーネントのインデックスを取得する
    /// @param name コンポーネント名
    /// @return 見つからなければ`std::nullopt`
    std::optional<std::size_t> FindComponent(std::string_view name) const;

    /// @brief 可動軸の一覧
    /// @return 全軸の情報 (`JointVector`のインデックスと対応する)
    /// @note 先頭から`MachineModel::Component(i)`を見て現れた順に並ぶ.
    const std::vector<AxisInfo>& Axes() const { return axes_; }
    /// @brief 軸名から軸のインデックスを取得する
    /// @param register_name 軸名 ("X","A"等)
    /// @return `Axes()`におけるインデックス. 見つからなければ`std::nullopt`
    std::optional<std::size_t> FindAxis(std::string_view register_name) const;
    /// @brief `tool_mount`コンポーネントのインデックス
    /// @return `Component()`におけるインデックス
    std::size_t ToolMountIndex() const { return tool_mount_; }
    /// @brief `work_mount`コンポーネントのインデックス
    /// @return `Component()`におけるインデックス
    std::size_t WorkMountIndex() const { return work_mount_; }
    /// @brief 工具側チェーンのコンポーネントインデックス
    /// @return `Component()`におけるインデックス列
    /// @note baseから`tool_mount`まで. 両端を含む
    const std::vector<std::size_t>& ToolChain() const { return tool_chain_; }
    /// @brief ワーク側チェーンのコンポーネントインデックス
    /// @return `Component()`におけるインデックス列
    /// @note baseから`work_mount`まで. 両端を含む
    const std::vector<std::size_t>& WorkChain() const { return work_chain_; }
    /// @brief チェーン上の可動軸の軸名 ("X", "A"等. 構築時に確定)
    /// @return チェーン上の軸名の列
    /// @note 工具側を根元から並べ、続けて連続してワーク側を並べる. 重複なし
    const std::vector<std::string>& ChainRegisters() const {
        return chain_registers_;
    }

    /// @brief 取り付け先座標系→ゼロポーズ機械座標への剛体変換H
    /// @param kind 取り付けフレームの種別 (工具側/ワーク側)
    /// @return 指定した取り付け点の剛体変換H
    /// @note 工具やワークの取り付け座標系上の点は、この行列のみを掛けて
    ///       ゼロポーズ機械座標系に変換できる.
    igesio::Matrix4d MountPlacement(MountKind kind) const;
    /// @brief 工具の向きを決める回転軸のインデックス列 (先頭が外側)
    /// @return 姿勢に寄与する回転軸のインデックス列 (0〜2本)
    /// @note 各要素は`MachineModel::Axes()`におけるインデックス
    /// @note 姿勢IKは工具軸方向を T = R(v, θ_R) R(u, θ_I) z_s の形で解く.
    ///       一般の5軸機械であれば、先頭が外側の軸v (旋回)、2番目が内側の軸u (傾斜)
    ///       に対応する. ワーク側チェーン上の回転軸を末端側から (σ=-1) 並べ,
    ///       続いて工具側チェーン上の回転軸を根元側から (σ=+1) 並べる.
    /// @note 工具側とワーク側のチェーンで共通する回転軸 (相対運動で相殺される) や,
    ///       どちらのチェーンにも含まれない回転軸は、工具姿勢の決定に寄与しないため
    ///       含めない.
    const std::vector<std::size_t>& OrientationAxes() const { return orientation_axes_; }
    /// @brief ゼロポーズでの工具軸方向 (`tool_mount`フレームのz軸. 機械座標)
    /// @note 構築時に確定する
    const igesio::Vector3d& ToolAxisHome() const { return tool_axis_home_; }
    /// @brief ワーク側回転軸の回転中心の参照点 (ゼロポーズ機械座標. 構築時に確定)
    /// @return 回転軸が1本ならその軸上の点. 2本以上なら末端側の軸上で
    ///         もう一方の軸線に最も近い点 (両軸が交わる理想機では交点).
    ///         ワーク側チェーンにIK対象の回転軸が無ければ`std::nullopt`
    const std::optional<igesio::Vector3d>& WorkPivotReference() const {
        return work_pivot_reference_;
    }

 private:
    /// @brief 構築元の機械定義
    MachineDefinition definition_;
    /// @brief コンポーネント
    /// @note baseを先頭として、深さ優先で親が子要素よりも前に来るように並べる
    std::vector<ComponentInfo> components_;
    /// @brief 可動軸 (components_の先頭から現れた順)
    std::vector<AxisInfo> axes_;
    /// @brief 工具側チェーンのコンポーネントインデックス (baseから)
    std::vector<std::size_t> tool_chain_;
    /// @brief ワーク側チェーンのコンポーネントインデックス (baseから)
    std::vector<std::size_t> work_chain_;
    /// @brief 工具の向きを決める回転軸 (axes_におけるインデックス. 先頭が外側)
    /// @note 工具側とワーク側のチェーンで共通する回転軸 (相対運動で相殺される) や,
    ///       どちらのチェーンにも含まれない回転軸は、工具姿勢の決定に寄与しないため
    ///       含めない.
    std::vector<std::size_t> orientation_axes_;
    /// @brief チェーン上の可動軸の軸名 ("X", "A"等)
    /// @note 工具側を根元から並べ、続いてワーク側を根元から並べる.
    std::vector<std::string> chain_registers_;
    /// @brief ゼロポーズでの工具軸方向 (`tool_mount`フレームのz軸)
    igesio::Vector3d tool_axis_home_ = igesio::Vector3d::UnitZ();
    /// @brief ワーク側回転軸の回転中心の参照点 (回転軸が無ければ`std::nullopt`)
    std::optional<igesio::Vector3d> work_pivot_reference_;
    /// @brief `tool_mount`コンポーネントのインデックス
    std::size_t tool_mount_ = 0;
    /// @brief `work_mount`コンポーネントのインデックス
    std::size_t work_mount_ = 0;
    /// @brief 名前 → コンポーネントのインデックス
    std::unordered_map<std::string, std::size_t> by_name_;
    /// @brief 軸名 ("X", "A"等) → 軸のインデックス
    std::unordered_map<std::string, std::size_t> by_register_;
};

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_MODEL_H_
