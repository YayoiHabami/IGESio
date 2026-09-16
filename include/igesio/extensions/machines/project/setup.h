/**
 * @file extensions/machines/project/setup.h
 * @brief 加工セットアップ (プロジェクト定義をもとに構築する実行時情報)
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note `ProjectDefinition`は定義ファイルで記述された値をそのまま格納する
 *       構造体であり、以下は構築時に本クラスが計算する.
 *       (i) 運動学モデル、初期姿勢,
 *       (i) ワーク座標, モデル座標, 機械部品座標等からゼロポーズ機械座標への同次変換,
 *       (i) 工具の解決、工具オフセットの実効値.
 *       運動学の計算を要する検証 (取り付け先の到達/閉路、`values`のチェーン所属,
 *       干渉ペアの規則) もここで行う.
 * @note 取り付け先の名前と対応する座標系は以下の4種類に分類される。命名の際は、
 *       4種類すべてを通して一意であること.
 *       (1) 予約語: `"work_mount"`/`"tool_mount"` (同typeのコンポーネント名も同じ),
 *       (2) コンポーネント名 (コンポーネント座標系C_c),
 *       (3) モデル名: `[[model]]`の`name` (モデル座標系),
 *       (4) ワークオフセットid: `[[work_offset]]`の`id` (ワーク座標系).
 *       いずれも取り付け先座標系→ゼロポーズ機械座標の同次変換Aとして評価し,
 *       所属コンポーネント (取り付け先を辿って最終的に属するコンポーネント) の
 *       運動F_a(q) (ゼロポーズ機械座標→軸変位量qでの機械座標) に追従する.
 * @note ワーク座標→ゼロポーズ機械座標の同次変換W_0は、登録値形式では
 *       `W_0 = F_wm(q*)⁻¹ · T(p_from)` (F_wmは`work_mount`コンポーネントの運動,
 *       q*は`values`の軸変位量、p_fromはq*における基準点の機械座標),
 *       幾何形式では`W_0 = A · T(o) · R`.
 *       軸変位量qでのワーク座標→機械座標の同次変換は`W(q) = F_a(q) · W_0`.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_PROJECT_SETUP_H_
#define IGESIO_EXTENSIONS_MACHINES_PROJECT_SETUP_H_

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"

namespace igesio::extensions::machines {

/// @brief セットアップの構築設定
struct SetupOptions {
    /// @brief ライブラリ参照工具の解決用関数
    /// @note 引数は参照先のライブラリと参照 (別名・アセンブリ番号). 工具形状を
    ///       定義する `ToolAssemblySpec` (`number`/`control_point`はエントリの
    ///       値で上書きする) を返す. 省略時、または`std::nullopt`を返した場合は
    ///       未解決のまま警告し、`Tools()`に含めない
    std::function<std::optional<ToolAssemblySpec>(
            const ToolLibrarySpec&, const LibraryToolRef&)> tool_resolver;
};

/// @brief ワーク座標系
struct WorkFrame {
    /// @brief ワークオフセットid
    /// @note `[[work_offset]]`の`id`と同一. 省略時は暗黙の`"G54"`
    std::string id;
    /// @brief 所属コンポーネント (`MachineModel::Component()`のインデックス)
    std::size_t carrier = 0;
    /// @brief ゼロポーズでの同次変換W_0 (ワーク座標→ゼロポーズ機械座標)
    igesio::Matrix4d w0 = igesio::Matrix4d::Identity();
    /// @brief 登録値 (`[[work_offset]].values`)
    /// @note 単位はmm/rad. 省略した軸は含めない. 登録値形式のみ持ち、幾何形式では
    ///       `std::nullopt`. 暗黙のG54は空の登録値. TCP無効時の座標語 (登録値相対の
    ///       NC指令値) を機械のNC指令値に変換する動作生成で用いる
    std::optional<NcValues> registered;
};

/// @brief ゼロポーズ機械座標系で定義されたモデル
struct PlacedModel {
    /// @brief モデルの定義 (`Project().models`の要素の複製)
    ModelSpec spec;
    /// @brief 所属コンポーネント (`MachineModel::Component()`のインデックス)
    std::size_t carrier = 0;
    /// @brief モデル座標→ゼロポーズ機械座標の同次変換 (A·T(origin)·R)
    igesio::Matrix4d placement = igesio::Matrix4d::Identity();
};

/// @brief ゼロポーズ機械座標で定義された形状1つ (機械部品・モデルの両方)
/// @note 何を、どこに、どの目的で置くか. 機械定義の`[[component.geometry]]`と
///       プロジェクトの`[[model]]`を統一的に扱うための構造体.
///       シーン構築と利用側の干渉判定はこの並びだけを見ればよく、形状の読込は
///       `geometry`を`LoadGeometry`/`LoadGeometryMesh`に渡して行う
struct GeometryInstance {
    /// @brief 形状の由来
    enum class Kind {
        /// @brief 機械コンポーネントの形状 (`[[component.geometry]]`)
        kMachinePart,
        /// @brief プロジェクトのモデル (`[[model]]`)
        kModel,
    };
    /// @brief 形状の由来
    Kind kind = Kind::kMachinePart;
    /// @brief 所有者の名前 (コンポーネント名、またはモデル名)
    std::string owner;
    /// @brief 所属コンポーネント (`MachineModel::Component()`のインデックス)
    /// @note この形状は所属コンポーネントの運動F_c(q)に追従する
    std::size_t carrier = 0;
    /// @brief モデルの役割 (`kModel`のときのみ)
    std::optional<ModelRole> role;
    /// @brief 形状
    GeometrySpec geometry;
    /// @brief 機械部品座標またはモデル座標→ゼロポーズ機械座標の同次変換
    /// @note 機械部品は`C_c · T(origin) · R`、モデルは`A · T(origin) · R`
    igesio::Matrix4d placement = igesio::Matrix4d::Identity();
    /// @brief 干渉計算の対象か
    bool collision = true;
    /// @brief 描画対象か (`false`は干渉専用形状)
    bool visible = true;
};

/// @brief 工具オフセットの実効値
/// @note 省略値を解決したもの
struct ResolvedToolOffset {
    /// @brief オフセット番号
    int number = 0;
    /// @brief 既定値の取得元とした工具番号
    std::optional<int> tool;
    /// @brief 工具長補正の形状値 [mm]
    double length = 0.0;
    /// @brief 工具長摩耗量 [mm]
    double length_wear = 0.0;
    /// @brief 工具径補正の形状値 (半径) [mm]
    double radius = 0.0;
    /// @brief 工具径摩耗量 [mm]
    double radius_wear = 0.0;
};

/// @brief 加工セットアップ
/// @note プロジェクト定義`ProjectDefinition`をもとに構築する実行時情報.
///       構築後は不変. `ProjectDefinition`と`MachineModel`のコピーを所有する
class MachiningSetup {
 public:
    /// @brief プロジェクト定義から構築する
    /// @param project プロジェクト定義 (読込済、機械定義を含む)
    /// @param options 構築の設定 (工具解決)
    /// @throw igesio::DataFormatError 取り付け先の不在・閉路、`values`のキーが
    ///        チェーン外、ワークオフセットの取り付け先が`work_mount`に至らない,
    ///        `[[collision.machine_pair]]`のペア規則違反
    ///        (文脈と行番号は定義側の値を用いる)
    /// @throw std::invalid_argument `MachineModel`の構築失敗 (機械定義の構造矛盾)
    explicit MachiningSetup(const ProjectDefinition& project,
                            const SetupOptions& options = {});

    /// @brief 運動学モデル
    const MachineModel& Model() const { return model_; }
    /// @brief 構築元のプロジェクト定義 (コピー)
    const ProjectDefinition& Project() const { return project_; }
    /// @brief 初期姿勢の軸変位量
    /// @note 機械定義の`initial`をもとに、`[initial.axes]`で指定された値を
    ///       上書きしたもの.
    /// @note 表示用のNC指令値は`NcFromJoints(Model(), BaseQ())`で得る
    const JointVector& BaseQ() const { return base_q_; }
    /// @brief 解決済みの工具表 (工具番号→定義)
    /// @note 未解決のライブラリ参照工具は含まない
    const std::map<int, ToolAssemblySpec>& Tools() const { return tools_; }
    /// @brief ワーク座標系 (定義順. 定義が無ければ暗黙の`G54`のみ)
    const std::vector<WorkFrame>& WorkFrames() const { return work_frames_; }
    /// @brief idでワーク座標系を引く
    /// @return 見つからなければ`nullptr`
    const WorkFrame* FindWorkFrame(std::string_view id) const;
    /// @brief ゼロポーズ機械座標で定義されたモデル (定義順)
    const std::vector<PlacedModel>& Models() const { return models_; }
    /// @brief ゼロポーズに置かれた全形状
    /// @note 機械コンポーネントの形状 (`Model().Component()`の順,
    ///       各コンポーネント内は定義順)、続いてモデル (`Models()`の順) の並び
    const std::vector<GeometryInstance>& Geometries() const { return geometries_; }
    /// @brief 工具オフセットの実効値 (オフセット番号→実効値)
    const std::map<int, ResolvedToolOffset>& ToolOffsets() const {
        return tool_offsets_;
    }
    /// @brief 初期工具番号
    /// @note `kNoTool`の場合は工具なし
    int InitialTool() const { return project_.initial_tool; }
    /// @brief 初期ワークオフセットid
    /// @note 省略時は先頭のワークオフセット. 定義が無ければ`"G54"`
    const std::string& InitialWorkOffset() const {
        return initial_work_offset_;
    }
    /// @brief 構築時の警告 (プロジェクト定義側の警告は含まない)
    const std::vector<Diagnostic>& Warnings() const { return warnings_; }

    /// @brief 取り付け先の名前を解決する
    /// @param name 予約語/コンポーネント名/モデル名/ワークオフセットid
    /// @return (所属コンポーネントのインデックス,
    ///          取り付け先座標系→ゼロポーズ機械座標の同次変換A) のペア
    /// @throw std::invalid_argument 指定された名前が取り付け先として存在しない場合
    std::pair<std::size_t, igesio::Matrix4d>
    ResolveAttach(std::string_view name) const;

 private:
    /// @brief 構築元のプロジェクト定義
    ProjectDefinition project_;
    /// @brief 運動学モデル
    MachineModel model_;
    /// @brief 初期姿勢の軸変位量
    JointVector base_q_;
    /// @brief 解決済みの工具表
    std::map<int, ToolAssemblySpec> tools_;
    /// @brief ワーク座標系 (定義順)
    std::vector<WorkFrame> work_frames_;
    /// @brief ゼロポーズ機械座標で定義されたモデル (定義順)
    std::vector<PlacedModel> models_;
    /// @brief ゼロポーズに置かれた全形状 (機械部品→モデル)
    std::vector<GeometryInstance> geometries_;
    /// @brief 工具オフセットの実効値
    std::map<int, ResolvedToolOffset> tool_offsets_;
    /// @brief 初期ワークオフセットid
    std::string initial_work_offset_;
    /// @brief 取り付け先の名前→(所属コンポーネント,
    ///        取り付け先座標系→ゼロポーズ機械座標の同次変換A)
    std::map<std::string, std::pair<std::size_t, igesio::Matrix4d>> attach_frames_;
    /// @brief 構築時の警告
    std::vector<Diagnostic> warnings_;
};

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_PROJECT_SETUP_H_
