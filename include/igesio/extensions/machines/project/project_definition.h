/**
 * @file extensions/machines/project/project_definition.h
 * @brief プロジェクト定義 (TOML) のデータモデル
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note プロジェクトフォーマット (機械定義・制御装置・工具・ワーク・治具・
 *       NCプログラムの組合せ) の記述内容を、ファイルに書かれた値のまま格納する.
 *       読込時に単位換算を行うため、本構造体の値は全て内部単位 (mm, rad).
 * @note 既定値の補完と参照の解決結果 (工具形状・暗黙のG54・工具オフセットの実効値)
 *       は持たない. デフォルト値が同一テーブルの他のキーの値から決定できるもの
 *       (`ModelSpec`の`collision`, `ToolPairSpec::enabled`) は実効値で持ち,
 *       それ以外の省略可能なメンバは`std::optional`、または空文字列として保持する.
 *       デフォルト値は派生関数 (`OutputDir`等) か`MachiningSetup`で設定する.
 * @note 本拡張が読むセクションは`[format]`・`[project]`・`[units]`・`[machine]`・
 *       `[controller]`・`[[tool_library]]`・`[[tool]]`・`[[tool_offset]]`・
 *       `[[work_offset]]`・`[[model]]`・`[[program]]`・`[initial]`・`[collision]`・
 *       `[run]`. これ以外のトップレベルのキーは名前によらず`retained`に保持する.
 * @note 取り付け先の名前は以下の4種類に分類される。4種類すべてを通して一意であること.
 *       (1) 予約語: `"work_mount"`/`"tool_mount"`
 *       (2) コンポーネント名
 *       (3) モデル名: `[[model]]`の`name`
 *       (4) ワークオフセットid: `[[work_offset]]`の`id`
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_PROJECT_PROJECT_DEFINITION_H_
#define IGESIO_EXTENSIONS_MACHINES_PROJECT_PROJECT_DEFINITION_H_

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/opaque_toml.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"

namespace igesio::extensions::machines {

/// @brief プロジェクトフォーマットの名称
/// @note TOMLの`[format].name`に対応
constexpr std::string_view kProjectFormatName = "machining-project";

/// @brief 対応するプロジェクトフォーマットのバージョン `[major, minor]`
/// @note 読込はmajorが一致するものを受理し、出力時は常にこの値を書く
constexpr std::array<int, 2> kProjectFormatVersion = {1, 0};

/// @brief 取り付け先名の予約語 (ワーク取り付け点)
/// @note `type = "work_mount"`のコンポーネントを名前によらず指す
constexpr std::string_view kWorkMountAttach = "work_mount";

/// @brief 取り付け先名の予約語 (工具取り付け点)
/// @note `type = "tool_mount"`のコンポーネントを名前によらず指す
constexpr std::string_view kToolMountAttach = "tool_mount";

/// @brief `[[work_offset]]`が1つも無いときに暗黙に定義されるワークオフセットのid
/// @note 定義には挿入せず、`MachiningSetup`が補う
constexpr std::string_view kImplicitWorkOffsetId = "G54";

/// @brief 取り付け先の名前が予約語 (`"work_mount"`/`"tool_mount"`) か
/// @note 予約語はモデル名・ワークオフセットidとして使用できない
bool IsReservedAttachName(std::string_view name);



/**
 * ---- 参照 ----
 */

/// @brief ファイル参照 (通常パス`file` / グローバル相対パス`library`)
/// @note 出力時は`raw`と`from_library`から`file =`/`library =`を復元する
struct FileReference {
    /// @brief TOMLから読み込んだパス文字列
    /// @note `from_library`なら必須. `file`形式では空文字列にすることもできるが,
    ///       その場合は`resolved`を基準ディレクトリから相対化して出力する
    std::string raw;
    /// @brief 解決済みのパス
    /// @note 読込時に存在を確認済み (`[run.output].dir`のみ確認しない)
    std::filesystem::path resolved;
    /// @brief `library`キー (ライブラリ検索パスからの相対) で指定されたか
    bool from_library = false;
};

/// @brief 工具ライブラリの参照
/// @note TOMLの`[[tool_library]]`に対応
struct ToolLibrarySpec {
    /// @brief `[[tool]].source`から参照する別名
    /// @note ライブラリが1つのみなら空でもよい
    std::string alias;
    /// @brief ライブラリファイル
    FileReference file;
    /// @brief TOMLの行番号 (診断用)
    int line = 0;
};

/// @brief ライブラリ参照形式の工具
/// @note TOMLの`[[tool]]`で`assembly`を指定した場合に対応. 解決は行わず,
///       `MachiningSetup`が`SetupOptions::tool_resolver`で形状を取得する
struct LibraryToolRef {
    /// @brief 参照する`[[tool_library]]`の`alias`
    /// @note ライブラリが1つのみなら空でもよい
    std::string source;
    /// @brief ライブラリ内のアセンブリ番号
    int assembly = 0;
};

/// @brief 工具番号と工具の対応
/// @note TOMLの`[[tool]]`に対応
struct ToolEntry {
    /// @brief 工具番号 (正の整数、一意)
    int number = 0;
    /// @brief 表示名
    /// @note 省略時は空 (表示名は`MachiningSetup`で補う)
    std::string name;
    /// @brief 簡易アセンブリ形式 (`[tool.simple]`) またはライブラリ参照形式
    std::variant<SimpleToolSpec, LibraryToolRef> source;
    /// @brief 工具先端からゲージラインまでの長さ [mm]
    /// @note 省略時は`std::nullopt` (輪郭側の値は`MachiningSetup`で適用)
    std::optional<double> gauge_length;
    /// @brief 位置IKの制御点
    ControlPoint control_point = ControlPoint::kTip;
    /// @brief TOMLの行番号 (診断用)
    int line = 0;
};

/// @brief 工具オフセットテーブルの1エントリ
/// @note TOMLの`[[tool_offset]]`に対応. 省略した形状値は`std::nullopt`のまま
///       保持し、実効値は`MachiningSetup::ToolOffsets()`が計算する
struct ToolOffsetEntry {
    /// @brief オフセット番号 (正の整数・一意)
    int number = 0;
    /// @brief 既定値の取得元とする工具番号
    std::optional<int> tool;
    /// @brief 工具長補正の形状値 [mm]
    std::optional<double> length;
    /// @brief 工具長摩耗量 [mm]
    double length_wear = 0.0;
    /// @brief 工具径補正の形状値 (半径) [mm]
    std::optional<double> radius;
    /// @brief 工具径摩耗量 [mm]
    double radius_wear = 0.0;
    /// @brief TOMLの行番号 (診断用)
    int line = 0;
};



/**
 * ---- ワークオフセット・モデル ----
 */

/// @brief ワークオフセット登録値形式の基準点
/// @note TOMLの`[[work_offset]].from`に対応
enum class WorkOffsetFrom {
    /// @brief `tool_mount`フレーム原点 (工具軸とゲージラインの交点)
    kToolMount,
    /// @brief 機械座標系原点
    kMachine,
};

/// @brief 基準点の文字列を`WorkOffsetFrom`に変換する
/// @param text `"tool_mount"` / `"machine"` (大文字小文字を区別する)
/// @return 対応する基準点. 未知の文字列なら`std::nullopt`
std::optional<WorkOffsetFrom> ParseWorkOffsetFrom(std::string_view text);

/// @brief 基準点の名称 (TOMLで用いる文字列) を返す
std::string_view WorkOffsetFromName(WorkOffsetFrom from);

/// @brief ワークオフセットとそれが定めるワーク座標系
/// @note TOMLの`[[work_offset]]`に対応. ワーク座標→ゼロポーズ機械座標の同次変換
///       W_0の導出は`MachiningSetup`が行う
struct WorkOffsetSpec {
    /// @brief 識別子
    /// @note `"G54"`など、一意な文字列. NCのワーク座標系選択 (G54～G59)
    ///       である必要はなく、任意の文字列を設定可能.
    std::string id;
    /// @brief 説明
    std::string description;
    /// @brief 登録値形式の基準点
    WorkOffsetFrom from = WorkOffsetFrom::kToolMount;
    /// @brief 追従する取り付け先
    /// @note 予約語・コンポーネント名・モデル名・他のid
    std::string attach = std::string(kWorkMountAttach);
    /// @brief 登録値形式 (mm / rad. 省略した軸は含めない) ,
    ///        またはワーク座標→取り付け先座標系の剛体変換 (`origin`・回転キー)
    std::variant<NcValues, GeometricPlacement> placement;
    /// @brief TOMLの行番号 (診断用)
    int line = 0;
};

/// @brief モデルの役割
/// @note TOMLの`[[model]].role`に対応
enum class ModelRole {
    /// @brief 被削材
    kStock,
    /// @brief 治具
    kFixture,
    /// @brief 完成形状 (既定で干渉・切削の対象外)
    kDesign,
    /// @brief 表示のみ
    kDisplay,
};

/// @brief 役割の文字列を`ModelRole`に変換する
/// @param text `"stock"` / `"fixture"` / `"design"` / `"display"`
///        (大文字小文字を区別する)
/// @return 対応する役割. 未知の文字列なら`std::nullopt`
std::optional<ModelRole> ParseModelRole(std::string_view text);

/// @brief 役割の名称 (TOMLで用いる文字列) を返す
std::string_view ModelRoleName(ModelRole role);

/// @brief 役割ごとの`collision`の既定値
/// @return stock・fixtureなら`true`、design・displayなら`false`
bool DefaultCollisionFor(ModelRole role);

/// @brief ストック・治具・設計形状・表示用のモデル
/// @note `GeometryEntry`と同様に、形状 (`GeometrySpec`) と、モデル座標系→取り付け先
///       座標系の剛体変換、および役割を持つ. モデル座標は`geometry`自身の座標系を指し
///       モデル名による取り付け先の指定 (attach) ではモデル名がこの座標系を指す.
///       モデル座標→ゼロポーズ機械座標の同次変換 `A · T(origin) · R`は
///       `MachiningSetup`が合成する (Aは取り付け先座標系→ゼロポーズ機械座標の同次変換)
/// @note TOMLの`[[model]]`に対応
struct ModelSpec {
    /// @brief 識別名 (取り付け先の名前として一意であること)
    /// @note 同じテーブルの`name`キーを読むため`geometry.name`も同じ値
    std::string name;
    /// @brief 役割
    ModelRole role = ModelRole::kStock;
    /// @brief 取り付け先
    /// @note 予約語・コンポーネント名・他のモデル名・ワークオフセットid
    std::string attach = std::string(kWorkMountAttach);
    /// @brief 形状
    GeometrySpec geometry;
    /// @brief モデル座標→取り付け先座標系の剛体変換 (`T(origin) · R`)
    GeometricPlacement placement;
    /// @brief 干渉計算の対象か
    /// @note 役割の既定 (`DefaultCollisionFor`) を適用した実効値
    bool collision = true;
    /// @brief 描画対象か
    /// @note `false`は干渉専用形状 (読み込んで機械座標には置くが、描画しない)
    bool visible = true;
    /// @brief TOMLの行番号 (診断用)
    int line = 0;
};



/**
 * ---- プログラム・制御装置 ----
 */

/// @brief プログラムの種別
/// @note TOMLの`[[program]].type`に対応. CLファイル形式との対応は工具経路側に置く
enum class ProgramType {
    /// @brief NCプログラム (Gコード)
    kGcode,
    /// @brief CLデータ
    kCl,
    /// @brief APTソース
    kApt,
};

/// @brief 種別の文字列を`ProgramType`に変換する
/// @param text `"gcode"` / `"cl"` / `"apt"` (大文字小文字を区別する)
/// @return 対応する種別. 未知の文字列なら`std::nullopt`
std::optional<ProgramType> ParseProgramType(std::string_view text);

/// @brief 種別の名称 (TOMLで用いる文字列) を返す
std::string_view ProgramTypeName(ProgramType type);

/// @brief NCプログラム・CLデータ
/// @note TOMLの`[[program]]`に対応
struct ProgramSpec {
    /// @brief プログラムファイル (通常パスのみ)
    FileReference file;
    /// @brief 表示名 (省略時は空. 既定はファイル名 (`DisplayName`))
    std::string name;
    /// @brief 実行対象か
    bool enabled = true;
    /// @brief 種別
    ProgramType type = ProgramType::kGcode;
    /// @brief 文字コード
    TextEncoding encoding = TextEncoding::kUtf8;
    /// @brief 改行 (出力時に用いる. 保持のみ)
    std::optional<NewlineStyle> newline;
    /// @brief 長さ単位 (cl・aptのみ)
    /// @note 省略時は`[units].length` (係数は`UnitScale`)
    std::optional<LengthUnit> unit;
    /// @brief 開始行 (1始まり)
    int start_line = 1;
    /// @brief 終了行. 省略時は最終行
    std::optional<int> end_line;
    /// @brief ONにするブロックスキップスイッチ番号 (1〜9. 昇順・重複なし)
    std::vector<int> block_skip;
    /// @brief 開始時に選択する工具番号 (`kNoTool` = 工具なし)
    /// @note 省略時は直前の状態を引き継ぐ
    std::optional<int> tool;
    /// @brief 開始時に選択するワークオフセットid
    /// @note 省略時は直前の状態を引き継ぐ
    std::optional<std::string> work_offset;
    /// @brief TOMLの行番号 (診断用)
    int line = 0;
};

/// @brief プログラムの長さの換算係数 (ファイル値→mm)
/// @param program プログラム
/// @param units プロジェクトの単位
/// @return cl・apt: `unit`または`units.length`の係数. gcode: 1 (G20/G21による)
double UnitScale(const ProgramSpec& program, const UnitScales& units);

/// @brief プログラムの表示名
/// @return `name`. 空ならファイル名 (`file.resolved`の末尾要素)
std::string DisplayName(const ProgramSpec& program);

/// @brief 制御装置定義の参照
/// @note TOMLの`[controller]`に対応. 内容は検証しない
struct ControllerSpec {
    /// @brief 制御装置定義ファイル
    FileReference file;
    /// @brief 本プロジェクトで無効化するコード (`"G68.2"`等. 要素は検証しない)
    std::vector<std::string> disabled_codes;
};



/**
 * ---- 干渉判定・実行制御 ----
 */

/// @brief 工具の部位とワーク側実体の組合せごとの干渉判定設定
/// @note TOMLの`[[collision.tool_pair]]`に対応
struct ToolPairSpec {
    /// @brief 工具の部位
    ToolPart part = ToolPart::kCutter;
    /// @brief 対象の役割 (`kStock`/`kFixture`のみ)
    ModelRole target = ModelRole::kStock;
    /// @brief 判定するか (実効値. 既定は`DefaultToolPairEnabled`)
    bool enabled = true;
    /// @brief クリアランス [mm]
    /// @note 省略時は`std::nullopt` (= `default_clearance`)
    std::optional<double> clearance;
};

/// @brief 部位と役割の組の`enabled`の既定値
/// @return cutter×stock (切削接触) のみ`false`、他は`true`
bool DefaultToolPairEnabled(ToolPart part, ModelRole target);

/// @brief 機械定義の干渉ペアの上書き
/// @note TOMLの`[[collision.machine_pair]]`に対応. `targets`と`subtree`が
///       一致するペアが機械定義にあればその値を指定キーで置き換え、なければ追加する
struct MachinePairOverride {
    /// @brief 対象の2コンポーネント名 (予約名`"tool"`・`"work"`を含む)
    std::array<std::string, 2> targets;
    /// @brief 各対象の子要素を含めるか
    std::array<bool, 2> subtree{false, false};
    /// @brief クリアランス [mm] (指定時のみ上書き)
    std::optional<double> clearance;
    /// @brief 判定するか (指定時のみ上書き)
    std::optional<bool> enabled;
};

/// @brief 実体側の干渉判定設定
/// @note TOMLの`[collision]`に対応
struct ProjectCollisionSettings {
    /// @brief 干渉判定全体の有効/無効
    bool enabled = true;
    /// @brief `[[collision.tool_pair]]`の既定クリアランス [mm]
    double default_clearance = 0.0;
    /// @brief 工具の部位とワーク側実体の組み合わせ
    std::vector<ToolPairSpec> tool_pairs;
    /// @brief 機械定義のペアの上書き
    std::vector<MachinePairOverride> machine_pairs;
};

/// @brief ストローク超過時の扱い
/// @note TOMLの`[run].overtravel`に対応
enum class OvertravelPolicy {
    /// @brief 停止
    kError,
    /// @brief 記録して続行
    kWarning,
    /// @brief 無視
    kIgnore,
};

/// @brief 干渉検出時の扱い
/// @note TOMLの`[run].collision`に対応
enum class CollisionPolicy {
    /// @brief 記録して続行
    kWarning,
    /// @brief 停止
    kError,
};

/// @brief ストローク超過時の扱いの文字列を`OvertravelPolicy`に変換する
/// @param text `"error"` / `"warning"` / `"ignore"` (大文字小文字を区別する)
/// @return 対応する扱い. 未知の文字列なら`std::nullopt`
std::optional<OvertravelPolicy> ParseOvertravelPolicy(std::string_view text);

/// @brief ストローク超過時の扱いの名称 (TOMLで用いる文字列) を返す
std::string_view OvertravelPolicyName(OvertravelPolicy policy);

/// @brief 干渉検出時の扱いの文字列を`CollisionPolicy`に変換する
/// @param text `"warning"` / `"error"` (大文字小文字を区別する)
/// @return 対応する扱い. 未知の文字列なら`std::nullopt`
std::optional<CollisionPolicy> ParseCollisionPolicy(std::string_view text);

/// @brief 干渉検出時の扱いの名称 (TOMLで用いる文字列) を返す
std::string_view CollisionPolicyName(CollisionPolicy policy);

/// @brief 出力先
/// @note TOMLの`[run.output]`に対応
struct RunOutput {
    /// @brief 出力ディレクトリ (存在は確認しない)
    /// @note 省略時は`std::nullopt` (既定`<source_dir>/output`は`OutputDir`)
    std::optional<FileReference> dir;
    /// @brief ログファイル名 (`dir`相対)
    std::optional<std::string> log;
    /// @brief レポートファイル名 (`dir`相対)
    std::optional<std::string> report;
    /// @brief 切削後形状のファイル名 (`dir`相対. 拡張子`.stl`・`.obj`)
    std::optional<std::string> cut_stock;
};

/// @brief 実行制御
/// @note TOMLの`[run]`に対応
struct RunSettings {
    /// @brief この工具が最初に選択される時点から実行を開始する
    std::optional<int> start_tool;
    /// @brief この工具の使用区間の終了で停止する
    std::optional<int> stop_tool;
    /// @brief ストローク超過時の扱い
    OvertravelPolicy overtravel = OvertravelPolicy::kError;
    /// @brief 干渉検出時の扱い
    CollisionPolicy collision = CollisionPolicy::kWarning;
    /// @brief 出力先
    RunOutput output;
};



/**
 * ---- プロジェクト定義 ----
 */

/// @brief プロジェクト定義
/// @note 各配列はファイルで定義された順序で格納する
struct ProjectDefinition {
    /// @brief フォーマットバージョン `[major, minor]`
    std::array<int, 2> format_version{0, 0};
    /// @brief プロジェクト名
    std::string name;
    /// @brief 説明
    std::string description;
    /// @brief 作成者
    std::string author;
    /// @brief 最終更新日時
    /// @note TOMLの日時 (`2026-09-02T10:00:00+09:00`等) または文字列の表記
    std::string modified;
    /// @brief 宣言された単位と換算係数
    UnitScales units;

    /// @brief `[machine]`の記載
    FileReference machine_ref;
    /// @brief 機械定義 (参照先を読込済み)
    /// @note 機械定義側の警告は`context = "machine"`で`warnings`へ転記する
    MachineDefinition machine;
    /// @brief 制御装置定義の参照
    std::optional<ControllerSpec> controller;
    /// @brief 工具ライブラリの参照
    std::vector<ToolLibrarySpec> tool_libraries;
    /// @brief 工具番号と工具の対応
    std::vector<ToolEntry> tools;
    /// @brief 工具オフセットテーブル
    std::vector<ToolOffsetEntry> tool_offsets;
    /// @brief ワークオフセット
    /// @note 暗黙のG54は入れない (`MachiningSetup::WorkFrames()`で補う)
    std::vector<WorkOffsetSpec> work_offsets;
    /// @brief モデル
    std::vector<ModelSpec> models;
    /// @brief プログラム (実行順)
    std::vector<ProgramSpec> programs;

    /// @brief 初期工具番号 (`kNoTool` = 工具なし)
    int initial_tool = kNoTool;
    /// @brief 初期ワークオフセットid
    /// @note 省略時は`std::nullopt` (先頭または暗黙のG54は
    ///       `MachiningSetup::InitialWorkOffset()`)
    std::optional<std::string> initial_work_offset;
    /// @brief 機械定義の`initial`を上書きする軸値 (`[initial.axes]`. mm / rad)
    NcValues initial_axes;
    /// @brief 実体側の干渉判定設定
    std::optional<ProjectCollisionSettings> collision;
    /// @brief 実行制御
    RunSettings run;
    /// @brief 本拡張が読まないセクションのキー (テーブル・値) を出現順に保持する
    /// @note 単位換算は行わない (`units`を差し替えて書き出しても元の単位のまま残る)
    std::vector<std::pair<std::string, OpaqueToml>> retained;

    /// @brief 相対パス解決の基準ディレクトリ
    std::filesystem::path source_dir;
    /// @brief 入力の表示名 (ファイル名、または文字列入力の`source_name`)
    std::string source_name;
    /// @brief 読込時の警告 (機械定義側の警告を含む)
    std::vector<Diagnostic> warnings;
};

/// @brief `[[tool]]`を探す
/// @param project プロジェクト定義
/// @param number 工具番号
/// @return 見つからなければ`nullptr`
const ToolEntry* FindTool(const ProjectDefinition& project, int number);

/// @brief `[[tool_library]]`を探す
/// @param project プロジェクト定義
/// @param alias 参照する工具ライブラリの`alias`
/// @return 見つからなければ`nullptr`
const ToolLibrarySpec* FindToolLibrary(const ProjectDefinition& project,
                                       std::string_view alias);

/// @brief `[[work_offset]]`を探す
/// @param project プロジェクト定義
/// @param id ワークオフセットの識別子
/// @return 見つからなければ`nullptr`
const WorkOffsetSpec* FindWorkOffset(const ProjectDefinition& project,
                                     std::string_view id);

/// @brief `[[model]]`を探す
/// @param project プロジェクト定義
/// @param name モデルの識別名
/// @return 見つからなければ`nullptr`
const ModelSpec* FindModel(const ProjectDefinition& project,
                           std::string_view name);

/// @brief 出力ディレクトリ
/// @param project プロジェクト定義
/// @return `run.output.dir`の解決済みパス. 無ければ`source_dir / "output"`
std::filesystem::path OutputDir(const ProjectDefinition& project);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_PROJECT_PROJECT_DEFINITION_H_
