/**
 * @file extensions/machines/toolpath/cl_program.h
 * @brief 制御装置に依存しない工具経路 (CLデータ) の公開データモデル
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note レコード列を`std::variant`で表す. 動作レコード (移動/円弧/ドウェル) と
 *       状態レコード (送り/主軸/工具/ワーク座標系/長さ補正/コメント/区切り) を
 *       一列に持ち、経路の構造は`ClMarker`で表現する. NC/APT/3行1組CLの読込結果,
 *       呼び出し側が組み立てるもの、NC/CLの出力 (`nc_writer.h`/`cl_io.h`) と
 *       動作生成の入力はすべてこのモデルで表す.
 * @note 純粋なCLデータだけでなく、NC由来の軸の指令 (G43.4の回転軸の指令、G43/G49の
 *       登録値相対の直進軸の指令、G53/G28の機械座標) も`ClGoto::axis_words`と
 *       `MotionFrame`で同じ型で表す. 制御点/工具軸/軸の指令の組み合わせとその意味は
 *       `ClGoto`の説明を参照.
 * @note 用語は以下のとおり.
 *       (1) 制御点: 工具上の指令点 (ワーク座標 [mm])
 *       (2) 工具軸方向: 工具先端から主軸に向かう単位ベクトル (ワーク座標)
 *       (3) 軸の指令: 軸アドレスと値の組 (`NcValues`. 軸名は機械定義の軸名,
 *           値は直進軸 [mm] / 回転軸 [rad])
 *       (4) 登録値相対のNC指令値: ワークオフセットの登録値
 *           (`[[work_offset]].values`) を基準にした直進軸のNC指令値.
 *           TCP無効時 (G43/G49) の座標語が表す値
 * @note 単位は内部単位 (mm、rad、s. 主軸回転数のみmin⁻¹). 呼び出し側が直接
 *       組み立てる場合も同じで、degのまま渡せるオーバーロードは設けない.
 * @note レコード種別の追加は`ClRecord`への追加で行い、`std::visit`を使う各所
 *       (`ClState::Apply`/出力/動作生成) の対応漏れをコンパイル時に検出する.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_PROGRAM_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_PROGRAM_H_

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"

namespace igesio::extensions::machines {

/// @brief 動作の種類
enum class MotionKind {
    /// @brief 早送り (G00 / CLの`7f` / APTの`RAPID`)
    kRapid,
    /// @brief 直線切削 (G01)
    kLinear,
    /// @brief 時計回りの円弧 (G02. 法線に対する右ねじと逆向き)
    kArcCw,
    /// @brief 反時計回りの円弧 (G03. 法線に対する右ねじ)
    kArcCcw,
};

/// @brief 移動レコードの座標系
enum class MotionFrame {
    /// @brief ワーク座標 (制御点) または登録値相対のNC指令値 (軸の指令)
    kWork,
    /// @brief 機械座標のNC指令値 (G53 / G28. 軸の指令のみ)
    kMachine,
};

/// @brief 経路上での役割 (1レコードの指令の目的)
/// @note 主に表示とコメント出力用
enum class PathRole {
    /// @brief 切削
    kCut,
    /// @brief アプローチ (切削開始点への接近)
    kApproach,
    /// @brief リトラクト (切削終了点からの退避)
    kRetract,
    /// @brief 経路間の移動
    kLink,
};

/// @brief レコードのソース位置 (診断/GUI表示用)
struct SourceLocation {
    /// @brief 呼び出し側が設定する`[[program]]`のインデックス (0始まり)
    int program_index = 0;
    /// @brief 元ファイルの行番号 (1始まり. 0 = 不明)
    int line = 0;
};



/**
 * ---- 動作レコード ----
 */

/// @brief 直線移動 (GOTO)
/// @note 制御点/工具軸/軸の指令の組み合わせでTCPの各形式を表す.
///       (1) `point` + `tool_axis` (直前の値をそのまま使用する場合を含む):
///             制御点+工具軸. 姿勢IK+位置IK (G43.5/CL/G68.2+G53.1)
///       (2) `point` + 回転軸の`axis_words`、`tool_axis`なし:
///             制御点+回転軸のNC指令値. 位置IKのみ (G43.4)
///       (3) 直進軸+回転軸の`axis_words` (`kWork`)、`point`なし:
///             登録値相対のNC指令値. IKなし (G43 (TCPなし)/G49)
///       (4) `axis_words` (`kMachine`)、`point`なし:
///             機械座標系でのNC指令値. 未指定軸は直前値 (G53/G28)
/// @note `tool_axis`も回転軸の指令も持たない`point`のみのレコードは,
///       直前の`ClState::tool_axis`があればそれを引き続き使用し、無ければ直前の
///       回転軸の指令値を使用する. どちらも無い場合は動作生成で警告し、+Z (ワーク座標)
///       を仮定する.
struct ClGoto {
    /// @brief 動作の種類 (`kRapid` / `kLinear`のみ)
    MotionKind kind = MotionKind::kLinear;
    /// @brief 制御点 (ワーク座標 [mm])
    /// @note 無い場合は軸の指令のみの移動
    std::optional<igesio::Vector3d> point;
    /// @brief 工具軸方向 (ワーク座標. 正規化済み)
    /// @note 無い場合は直前の値を引き続き使用する.
    std::optional<igesio::Vector3d> tool_axis;
    /// @brief 明示された軸の指令 (回転軸 [rad] / 直進軸 [mm])
    /// @note 未指定の軸は直前の値を引き続き使用する.
    NcValues axis_words;
    /// @brief 座標系 (`kMachine`では`point`を持たない)
    MotionFrame frame = MotionFrame::kWork;
    /// @brief 経路上の役割
    PathRole role = PathRole::kCut;
};

/// @brief 円弧 (始点は直前の位置)
struct ClArc {
    /// @brief 動作の種類 (`kArcCw` / `kArcCcw`のみ)
    /// @note 法線に対する右ねじで向きを定める
    MotionKind kind = MotionKind::kArcCcw;
    /// @brief 終点 (ワーク座標 [mm])
    igesio::Vector3d end = igesio::Vector3d::Zero();
    /// @brief 中心 (ワーク座標 [mm])
    igesio::Vector3d center = igesio::Vector3d::Zero();
    /// @brief 円弧平面の法線 (ワーク座標. 正規化済み)
    igesio::Vector3d normal = igesio::Vector3d::UnitZ();
    /// @brief 全周の数
    /// @note 始点と終点が一致する全円のとき1. 通常は0
    int full_turns = 0;
    /// @brief 終点の工具軸方向 (ワーク座標. 正規化済み)
    /// @note 始点の工具軸方向から球面線形補間する. 無い場合は不変
    std::optional<igesio::Vector3d> tool_axis;
    /// @brief 経路上の役割
    PathRole role = PathRole::kCut;
};

/// @brief ドウェル (G04)
struct ClDwell {
    /// @brief 停止時間 [s]
    double seconds = 0.0;
};



/**
 * ---- 状態レコード ----
 */

/// @brief 送り速度の変更
struct ClFeed {
    /// @brief 送り速度 [mm/s]
    /// @note NCのF指令は毎分なので、読込時に`kSecondsPerMinute`で割った値を格納する
    double mm_per_s = 0.0;
};

/// @brief 主軸の回転
struct ClSpindle {
    /// @brief 回転の向き
    enum class Mode {
        /// @brief 停止 (M05)
        kOff,
        /// @brief 正転 (M03)
        kCw,
        /// @brief 逆転 (M04)
        kCcw,
    } mode = Mode::kOff;
    /// @brief 回転数 [min⁻¹] (S指令)
    /// @note 無い場合は直前の値をそのまま使用する.
    std::optional<double> rpm;
};

/// @brief クーラントの入切 (M08 / M09)
struct ClCoolant {
    /// @brief 入なら`true`
    bool on = false;
};

/// @brief 工具の選択 (M06 / LOADTL)
struct ClLoadTool {
    /// @brief 工具番号 (`kNoTool`の場合は工具なし)
    int number = kNoTool;
};

/// @brief ワーク座標系の選択 (G54等)
struct ClSelectWorkOffset {
    /// @brief ワークオフセットid
    /// @note `"G54"`や`"G54.1P2"`等、またはプロジェクト定義の`id`
    std::string id;
};

/// @brief 工具長補正の選択 (G43系のH)
struct ClLengthOffset {
    /// @brief 補正番号 (`std::nullopt`の場合は解除に相当 (G49))
    std::optional<int> number;
};

/// @brief コメント
struct ClComment {
    /// @brief 本文
    std::string text;
};

/// @brief 経路の区切り
struct ClMarker {
    /// @brief 区切りの種類
    enum class Kind {
        /// @brief 経路の開始
        kPathBegin,
        /// @brief 経路の終了
        kPathEnd,
        /// @brief 工程の区切り (経路の入れ子にはしない)
        kOperation,
    };
    /// @brief 区切りの種類
    Kind kind = Kind::kPathBegin;
    /// @brief 名前 (経路名/工程名)
    std::string name;
};

/// @brief 変換せずに保持する元の文字列 (未対応のG/Mコード、APT文等)
/// @note 出力では、同じdialectならそのまま出力し、違えばコメント化して警告する
struct ClPassThrough {
    /// @brief ソースの種別 (`"nc"` / `"apt"`)
    std::string dialect;
    /// @brief 元の文字列 (ブロック本文/APT文)
    std::string text;
};

/// @brief プログラムの終了 (M30 / M02 / FINI)
struct ClEnd {};

/// @brief レコード (動作レコードまたは状態レコード)
using ClRecord = std::variant<
        ClGoto, ClArc, ClDwell,
        ClFeed, ClSpindle, ClCoolant, ClLoadTool,
        ClSelectWorkOffset, ClLengthOffset,
        ClComment, ClMarker, ClPassThrough, ClEnd>;



/**
 * ---- プログラムと状態 ----
 */

/// @brief CLプログラム (レコード列)
struct ClProgram {
    /// @brief 表示名 (`ProgramSpec::name` / APTの`PARTNO`)
    std::string name;
    /// @brief レコード列
    std::vector<ClRecord> records;
    /// @brief 各レコードのソース位置
    /// @note `records`と同じ長さにする. 空なら参照不可
    std::vector<SourceLocation> sources;
    /// @brief 読込時の警告 (`context`は空、行番号は`line`)
    std::vector<Diagnostic> warnings;

    /// @brief 各レコードのソース位置を参照できるか
    /// @return `sources`が空でなく`records`と同じ長さなら`true`
    bool HasSources() const;
};

/// @brief レコードを順に適用して得るモーダル状態
/// @note 制御装置が内部に保持している、現在の工具番号や送り速度等の状態. `ClProgram`とは
///       別に保持する. ワンショット (ドウェル等) の状態や、履歴などは保持しない.
/// @note 出力/動作生成/GUIで共用する. 状態レコードは対応するメンバを上書きし,
///       `kWork`の`ClGoto`と`ClArc`は`position`/`tool_axis`/`axis_words`を更新する.
///       `kMachine`の`ClGoto`は機械座標の値で、登録値相対の`axis_words`と同じ表には
///       格納できないので何も更新しない
struct ClState {
    /// @brief 選択中の工具番号 (`kNoTool`の場合は工具なし)
    int tool = kNoTool;
    /// @brief 選択中のワークオフセットid
    /// @note 空なら未選択で、呼び出し側のデフォルトに従う
    std::string work_offset;
    /// @brief 送り速度 [mm/s] (未指定なら`std::nullopt`)
    std::optional<double> feed;
    /// @brief 工具長補正番号 (`std::nullopt` = 解除)
    std::optional<int> length_offset;
    /// @brief 主軸の状態
    ClSpindle spindle;
    /// @brief クーラントの状態
    bool coolant = false;
    /// @brief 直前の制御点 (ワーク座標)
    std::optional<igesio::Vector3d> position;
    /// @brief 直前の工具軸方向 (ワーク座標)
    std::optional<igesio::Vector3d> tool_axis;
    /// @brief 直前までに明示された`kWork`の軸の指令
    /// @note `Merge`で累積する. `kMachine`の軸の指令は含めない
    NcValues axis_words;
    /// @brief `ClEnd`を適用済みか
    bool ended = false;

    /// @brief レコードを1つ適用する
    /// @param record 適用するレコード
    void Apply(const ClRecord& record);
};

/// @brief 動作レコード (`ClGoto` / `ClArc` / `ClDwell`) か
/// @param record 判定するレコード
bool IsMotion(const ClRecord& record);

/// @brief レコード種別の名称を取得する (診断/GUI用)
/// @param record 対象のレコード
/// @return `"goto"`, `"arc"`, `"dwell"`, `"feed"`, `"spindle"`, `"coolant"`,
///         `"load_tool"`, `"select_work_offset"`, `"length_offset"`, `"comment"`,
///         `"marker"`, `"pass_through"`, `"end"`
std::string_view ClRecordKindName(const ClRecord& record);

/// @brief CLプログラムの整合性を検査する
/// @param program 検査するプログラム
/// @return 警告の一覧 (`context`は空、`line`は`sources`があればその行番号)
/// @note 例外にはせず警告として返す. 検査項目は以下.
///       (1) `ClEnd`の後にレコードがある
///       (2) `kMachine`の`ClGoto`が`point`を持つ
///       (3) `ClArc`の`normal`がゼロベクトル
///       (4) `kArcCw`/`kArcCcw`以外の`ClArc`
///       (5) `kRapid`/`kLinear`以外の`ClGoto`
///       (6) `sources`の長さが`records`と異なる (空は可)
std::vector<Diagnostic> ValidateClProgram(const ClProgram& program);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_PROGRAM_H_
