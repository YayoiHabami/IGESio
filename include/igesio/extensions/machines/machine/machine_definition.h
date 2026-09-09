/**
 * @file extensions/machines/machine/machine_definition.h
 * @brief 機械定義 (TOML) のデータモデル
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 機械定義フォーマットの記述内容を、ゼロポーズ機械座標系のまま格納する.
 *       読込時に単位換算を行うため、本構造体の値は全て内部単位 (mm, rad, s).
 *       回転数のみ例外とし、[min⁻¹] のまま保持する.
 *       順運動学・軸一覧等の派生情報は`MachineModel`が本構造体から構築する.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_DEFINITION_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_DEFINITION_H_

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/units.h"

namespace igesio::extensions::machines {

/// @brief 機械定義フォーマットの名称
/// @note TOMLの`[format].name`に対応
constexpr std::string_view kMachineFormatName = "machine-definition";

/// @brief 対応する機械定義フォーマットのバージョン `[major, minor]`
/// @note 読込はmajorが一致するものを受理し、書き出しは常にこの値を書く
constexpr std::array<int, 2> kMachineFormatVersion = {2, 0};

/// @brief コンポーネントの種別
/// @note TOMLの`[[component]]`の`type`キーに対応
enum class ComponentType {
    /// @brief ルート (名前`"base"`専用)
    kBase,
    /// @brief 直進軸
    kLinear,
    /// @brief 回転軸
    kRotary,
    /// @brief 主軸 (位置決め軸ではない)
    kSpindle,
    /// @brief 工具取り付け点 (機械内にちょうど1つ・末端)
    kToolMount,
    /// @brief ワーク取り付け点 (機械内にちょうど1つ・末端)
    kWorkMount,
    /// @brief 動かない構造物
    /// @note カバーやフレーム、付属の装置など
    kFixed,
};

/// @brief 逆運動学における回転角の解の選択方針
/// @note 回転2軸を持つ機械では、与えられた工具軸方向を実現する回転軸の角度は一般に
///       2つ存在する。コンポーネントの構造から定まる傾斜軸 (多くはAorB) の角度θは,
///       軸構成から定まる基準角δを用いて`θI = δ ± Δ`の2通りの解がある.
///       傾斜角が決まれば旋回角は一意に定まるため、解は`(θI, θR)`の2通りとなる.
///       本方針はこの符号の選び方であり、`kPositive`は`+Δ`を、`kNegative`は`-Δ`を,
///       `kContinuous`は前回の角度に近い方の符号を選ぶ.
/// @note TOMLの`[kinematics]`の`branch`キーに対応
enum class BranchPolicy {
    /// @brief 常に+Δの側 (デフォルト)
    kPositive,
    /// @brief 常に-Δの側
    kNegative,
    /// @brief 直前の角に近い側
    kContinuous,
};

/// @brief 干渉チェックのモード
/// @note TOMLの`[collision]`の`mode`キーに対応
enum class CollisionMode {
    /// @brief 列挙したペアのみ
    /// @note `CollisionSettings::pairs`で指定したペアのみ検証する
    kPairs,
    /// @brief 隣接ペアと`exclude`を除く全ペア
    /// @note 全コンポーネントおよび"tool","work"の全組合せについて,
    ///       隣接ペアと`CollisionSettings::exclude`で指定したペアを除き検証する
    kAllExceptAdjacent,
};

/// @brief コンポーネント種別の文字列をComponentTypeに変換する
/// @return 対応する種別. 未知の文字列なら`std::nullopt`
std::optional<ComponentType> ParseComponentType(std::string_view text);

/// @brief ComponentTypeを文字列 (TOMLで用いるもの) に変換する
std::string_view ComponentTypeName(ComponentType type);

/// @brief 逆運動学における傾斜角の解の選択方針の文字列をBranchPolicyに変換する
/// @return 対応する方針. 未知の文字列なら`std::nullopt`
std::optional<BranchPolicy> ParseBranchPolicy(std::string_view text);

/// @brief BranchPolicyを文字列 (TOMLで用いるもの) に変換する
std::string_view BranchPolicyName(BranchPolicy policy);

/// @brief 干渉チェックのモードの文字列をCollisionModeに変換する
/// @return 対応するモード. 未知の文字列なら`std::nullopt`
std::optional<CollisionMode> ParseCollisionMode(std::string_view text);

/// @brief CollisionModeを文字列 (TOMLで用いるもの) に変換する
std::string_view CollisionModeName(CollisionMode mode);

/// @brief 軸の動特性 (直進軸 or 回転軸)
/// @note TOMLの`[component.axis.dynamics]`に対応
struct AxisDynamics {
    /// @brief 早送り速度 [mm/s] or [rad/s]
    std::optional<double> rapid_feed;
    /// @brief 最大切削送り速度 [mm/s] or [rad/s]
    std::optional<double> max_feed;
    /// @brief 最小切削送り速度 (>=0) [mm/s] or [rad/s]
    std::optional<double> min_feed;
    /// @brief 加速度 [mm/s²] or [rad/s²]
    std::optional<double> accel;
    /// @brief 減速度 [mm/s²] or [rad/s²]
    std::optional<double> decel;
    /// @brief 指令最小分解能 [mm] or [rad]
    std::optional<double> resolution;
};

/// @brief 直進軸/回転軸の定義
/// @note `ComponentSpec::type`が`kLinear`または`kRotary`のコンポーネントで指定
/// @note `direction`・`point`はコンポーネントのローカル座標系による表現であり,
///       機械座標への写像 (`local_frame`の適用) は`MachineModel`が行う.
/// @note `limits`・`initial`・`wrap_start`は機械座標系における物理的な変位の
///       範囲を規定するものではなく、NC指令値の上限/下限を規定するもの.
struct AxisSpec {
    /// @brief 軸名 (機械内で一意)
    /// @note NCでの指令において使用するもの、"X"や"A"等
    std::string register_name;
    /// @brief ゼロポーズにおける軸方向 (ローカル座標・正規化済)
    /// @note 直進軸の場合は軸の正方向を、回転軸の場合は回転軸 (右ねじ) を表す
    igesio::Vector3d direction = igesio::Vector3d::UnitZ();
    /// @brief 回転軸を規定する、directionが通る1点 (kRotaryのみ. ローカル座標)
    std::optional<igesio::Vector3d> point;

    /// @brief 可動範囲 `[min, max]` (NC指令値, [mm] or [rad])
    /// @note unlimitedがtrueの場合はnulloptとすること
    std::optional<std::array<double, 2>> limits;
    /// @brief 可動範囲が無制限か
    /// @note limitsに値を設定した場合はfalseとすること
    bool unlimited = false;
    /// @brief NC値の正規化区間の下端 [rad]
    /// @note kRotaryかつunlimitedの場合のみ有効. 指定時、NC値の範囲は
    ///       [wrap_start, wrap_start+2π)に正規化される.
    ///       省略時は正規化方式を規定せず、呼び出し側の裁量に委ねる.
    std::optional<double> wrap_start;
    /// @brief 軸の既定値 (NC指令値, [mm] or [rad])
    /// @note 省略時は0。limitsの範囲に0が含まれない場合は省略しないこと
    double initial = 0.0;

    /// @brief 動特性
    AxisDynamics dynamics;
};

/// @brief 主軸の属性
/// @note TOMLの`[component.spindle]`に対応
struct SpindleSpec {
    /// @brief 最大回転数 [min⁻¹]
    /// @note 例外的に内部単位 [s] としない
    std::optional<double> max_rpm;
};

/// @brief プリミティブ形状
/// @note TOMLの`[[component.geometry]]`において`primitive`キーを
///       指定した場合の形状データに対応する
struct PrimitiveSpec {
    /// @brief プリミティブの種類
    enum class Kind {
        /// @brief 各辺が座標軸に平行な原点中心の直方体
        kBox,
        /// @brief 高さ方向の中心が原点にあるz軸平行な円柱
        kCylinder,
    } kind = Kind::kBox;

    /// @brief 各軸方向の辺長 [mm] (kBoxのみ)
    igesio::Vector3d size = igesio::Vector3d::Zero();

    /// @brief 半径 [mm] (kCylinderのみ)
    double radius = 0.0;
    /// @brief 高さ [mm] (kCylinderのみ)
    double height = 0.0;
};

/// @brief コンポーネントの部分形状 (1コンポーネントは複数の形状を持つことが可能)
/// @note ファイル参照形式とプリミティブ形式のいずれか.
///       本インスタンス作成時 (読み込み時) にはファイルの存在は確認しない.
/// @note `placement`はモデル座標からゼロポーズ機械座標への変換
///       `C_c · T(origin) · R` (C_cは`ComponentSpec::local_frame`)
/// @note TOMLの`[[component.geometry]]`に対応
struct GeometrySpec {
    /// @brief 表示名 (機械内で一意でなくてよい)
    std::string name;
    /// @brief 形状のソース (解決済みパス、またはプリミティブ形状)
    std::variant<std::filesystem::path, PrimitiveSpec> source;
    /// @brief TOMLに記載されたパス文字列 (sourceがパスの場合のみ)
    /// @note デバッグや警告表示用
    std::string raw_path;
    /// @brief ファイルの長さ単位からmmへの換算係数
    /// @note STL/OBJの`unit`. inchなら25.4
    double file_unit_scale = 1.0;
    /// @brief モデル座標→ゼロポーズ機械座標への変換
    /// @note C_c (`ComponentSpec::local_frame`) 適用済みの剛体変換行列であり,
    ///       モデル座標の点はこの行列のみを掛けてゼロポーズ機械座標に変換できる.
    igesio::Matrix4d placement = igesio::Matrix4d::Identity();

    /// @brief 色 (RGB、0~1). 省略時は`std::nullopt`
    std::optional<std::array<float, 3>> color;
    /// @brief 不透明度 (0=透明〜1=不透明)
    float opacity = 1.0f;
    /// @brief 干渉計算の対象か
    bool collision = true;
    /// @brief 描画対象か
    bool visible = true;
    /// @brief TOMLの行番号 (診断用)
    int line = 0;
};

/// @brief 運動学ツリーの1節点
/// @note TOMLの`[[component]]`に対応
struct ComponentSpec {
    /// @brief 機械内で一意な識別名 (コンポーネントの名称)
    std::string name;
    /// @brief 親コンポーネント名
    /// @note ルート (base) のみ空、それ以外では必須
    std::string parent;
    /// @brief 種別
    ComponentType type = ComponentType::kFixed;

    /// @brief ローカル座標系の配置C_c
    /// @note このコンポーネント内の座標値の基準である、ローカル座標系の定義.
    ///       単位行列の場合は、ローカル座標はゼロポーズ機械座標系と一致する.
    ///       axisやgeometryの座標値はこのローカル座標系で表現される.
    /// @note ローカル座標の点pcと機械座標の点pm (同次座標) の関係は
    ///       `pm = C_c · pc`, `pc = C_c⁻¹ · pm`.
    igesio::Matrix4d local_frame = igesio::Matrix4d::Identity();
    /// @brief 可動軸 (typeがkLinear/kRotaryの場合のみ)
    std::optional<AxisSpec> axis;
    /// @brief 取り付け先座標系→ゼロポーズ機械座標への剛体変換H (typeがkMountの場合のみ)
    /// @note C_c (`::local_frame`) 適用済みの剛体変換行列であり,工具やワークの
    ///       取り付け座標系上の点は、この行列のみを掛けてゼロポーズ機械座標に変換できる.
    std::optional<igesio::Matrix4d> frame_placement;
    /// @brief 主軸の属性
    std::optional<SpindleSpec> spindle;
    /// @brief 形状 (0個以上)
    std::vector<GeometrySpec> geometries;

    /// @brief TOMLの行番号 (診断用; 暗黙のbaseは0)
    int line = 0;
};

/// @brief 干渉チェックのペア
/// @note TOMLの`[[collision.pair]]`に対応
struct CollisionPair {
    /// @brief 対象の2コンポーネントの名称
    /// @note 指定したコンポーネント名、または予約名 (`"tool"`,`"work"`等) を使用
    std::array<std::string, 2> targets;
    /// @brief 各対象の子孫コンポーネントを含めるか (targetsの順序に対応)
    std::array<bool, 2> subtree{false, false};
    /// @brief 許容クリアランス [mm] (省略時は`default_clearance`を適用)
    double clearance = 0.0;
    /// @brief このペアをチェックするか
    /// @note 定義を残したまま一時的に無効化する場合に使用
    bool enabled = true;
};

/// @brief 干渉チェック設定
/// @note TOMLの`[collision]`に対応
struct CollisionSettings {
    /// @brief モード
    CollisionMode mode = CollisionMode::kPairs;
    /// @brief 全ペア共通の許容クリアランス [mm]
    double default_clearance = 0.0;
    /// @brief ペア
    /// @note 格納順序は`[collision]`直下の`pairs`キーの内容 (簡易形),
    ///       続けて`[[collision.pair]]`の内容とする.
    std::vector<CollisionPair> pairs;
    /// @brief `all_except_adjacent`で追加除外するペア
    std::vector<std::array<std::string, 2>> exclude;
};

/// @brief 機械定義
/// @note `components`はファイルで定義された順序で格納する (暗黙の`base`は末尾).
///       親が先に格納される順序への並べ替えは`MachineModel`が行う
struct MachineDefinition {
    /// @brief フォーマットバージョン `[major, minor]`
    std::array<int, 2> format_version{0, 0};
    /// @brief 機械名
    std::string name;
    /// @brief 説明
    std::string description;
    /// @brief 作成者
    std::string author;
    /// @brief 作成・更新日
    /// @note TOMLネイティブ日付 (`YYYY-MM-DD`の文字列)
    std::string date;
    /// @brief 宣言された単位と換算係数
    UnitScales units;

    /// @brief コンポーネント
    std::vector<ComponentSpec> components;
    /// @brief 逆運動学における回転角の解の選択方針
    BranchPolicy branch = BranchPolicy::kPositive;
    /// @brief 干渉チェック設定
    std::optional<CollisionSettings> collision;

    /// @brief 相対パス解決の基準ディレクトリ
    std::filesystem::path source_dir;
    /// @brief 入力の表示名 (ファイル名、または文字列入力の`source_name`)
    std::string source_name;
    /// @brief 読込時の警告
    std::vector<Diagnostic> warnings;
};

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_DEFINITION_H_
