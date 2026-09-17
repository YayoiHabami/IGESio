/**
 * @file extensions/machines/scene/machine_scene.h
 * @brief 加工セットアップからのシーン構築 (機械、工具、モデル、ワーク座標系、
 *        工具経路（の線描画）のアセンブリ木)
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note `MachiningSetup`をもとに、呼び出し側のルートアセンブリの子として次の
 *       アセンブリ木を作る. 括弧内は親に対する変換.
 *       ```text
 *       <caller root>
 *       └─ machine:<name>                 (I)
 *          ├─ triad:machine               (I. 機械座標系の3軸)
 *          ├─ trace:machine               (I. 動作軌跡の配置先. 中身は
 *          │                               simulation/motion_sceneで作る)
 *          ├─ <component>                 (F_c(q). 全コンポーネントを同じ階層に並べる)
 *          │   └─ geometry:<name>         (機械部品座標→ゼロポーズ機械座標の同次変換)
 *          ├─ <tool_mount component>
 *          │   ├─ triad:tool_mount        (H_tm. 取り付けフレームの3軸)
 *          │   └─ tool:<n>                (H_tm·T(0,0,-ゲージ長). 工具表の全工具を
 *          │       │                       置き、選択中の工具のみ表示)
 *          │       ├─ axis                (I. 工具軸線)
 *          │       └─ control_point       (I. 制御点マーカー)
 *          ├─ <carrier component>
 *          │   ├─ model:<name>            (モデル座標→ゼロポーズ機械座標の同次変換)
 *          │   ├─ workframe:<id>          (W_0. ワーク座標系の3軸)
 *          │   ├─ paths:<id>              (W_0. 子rapid/cutに経路線)
 *          │   └─ attach:<id>             (W_0. 呼び出し側の追加物の取り付け先)
 *          └─ <work_mount component>
 *              ├─ trajectory:             (I. 工具軌跡の配置先)
 *              └─ trace:work              (I. 動作軌跡 (work_mount座標) の配置先)
 *       ```
 *       コンポーネントの形状はゼロポーズ機械座標で定義されているため、姿勢の適用は
 *       順運動学の結果F_c(q)をそのまま各コンポーネントの大域変換に設定する.
 *       取り付けられるもの (工具、モデル、ワーク座標系、経路線) は所属コンポーネント
 *       の子として固定の相対変換を持ち、累積変換で自動的に追従する.
 * @note `trajectory:`/`trace:machine`/`trace:work`/`attach:<id>`は`Build`で
 *       空のアセンブリとして作り、`Clear`以外では作り直さない. 表示用の同じIDを
 *       (レンダラの表示フィルタ等に) 保持したまま中身だけを変更できる.
 * @note 座標系の記法は`project/setup.h`と同じ (F_c(q)はゼロポーズ機械座標→
 *       軸変位量qでの機械座標の同次変換、H_tmは取り付けフレーム→ゼロポーズ機械
 *       座標の同次変換、W_0はワーク座標→ゼロポーズ機械座標の同次変換).
 * @note 本ヘッダはmodels層までに依存し、GLには依存しない. ワークビュー
 *       (ワーク座標系に固定した表示) に関してはIDと同次変換を返すだけであるため,
 *       レンダラの操作は呼び出し側が行うこと.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SCENE_MACHINE_SCENE_H_
#define IGESIO_EXTENSIONS_MACHINES_SCENE_MACHINE_SCENE_H_

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "igesio/common/color.h"
#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/scene/geometry_loader.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines {

/// @brief 機械全体のアセンブリの名前の接頭辞 (`"machine:<name>"`)
constexpr std::string_view kMachineAssemblyPrefix = "machine:";
/// @brief 機械部品の形状のアセンブリの名前の接頭辞 (`"geometry:<name>"`)
constexpr std::string_view kGeometryAssemblyPrefix = "geometry:";
/// @brief モデルのアセンブリの名前の接頭辞 (`"model:<name>"`)
constexpr std::string_view kModelAssemblyPrefix = "model:";
/// @brief ワーク座標系のアセンブリの名前の接頭辞 (`"workframe:<id>"`)
constexpr std::string_view kWorkFrameAssemblyPrefix = "workframe:";
/// @brief 経路線のアセンブリの名前の接頭辞 (`"paths:<id>"`)
constexpr std::string_view kPathsAssemblyPrefix = "paths:";
/// @brief 呼び出し側の追加物の取り付け先の名前の接頭辞 (`"attach:<id>"`)
constexpr std::string_view kAttachAssemblyPrefix = "attach:";
/// @brief 機械座標系の3軸のアセンブリの名前
constexpr std::string_view kMachineTriadName = "triad:machine";
/// @brief 取り付けフレームの3軸のアセンブリの名前
constexpr std::string_view kToolMountTriadName = "triad:tool_mount";
/// @brief 工具軌跡の配置先のアセンブリの名前 (`work_mount`の子)
constexpr std::string_view kTrajectoryAssemblyName = "trajectory:";
/// @brief 動作軌跡 (機械座標) の配置先のアセンブリの名前 (`machine:<name>`の子)
constexpr std::string_view kMachineTraceAssemblyName = "trace:machine";
/// @brief 動作軌跡 (`work_mount`座標) の配置先のアセンブリの名前 (`work_mount`の子)
constexpr std::string_view kWorkTraceAssemblyName = "trace:work";
/// @brief 経路線のうち早送りのアセンブリの名前 (`paths:<id>`の子)
constexpr std::string_view kRapidPathsName = "rapid";
/// @brief 経路線のうち切削のアセンブリの名前 (`paths:<id>`の子)
constexpr std::string_view kCutPathsName = "cut";
/// @brief 工具軸線のアセンブリの名前 (`tool:<n>`の子)
constexpr std::string_view kToolAxisLineName = "axis";
/// @brief 制御点マーカーのアセンブリの名前 (`tool:<n>`の子)
constexpr std::string_view kControlPointName = "control_point";
/// @brief 機械部品の形状の`AssemblyMetadata::role_tag`
/// @note モデルの`role_tag`は`ModelRoleName(role)`
constexpr std::string_view kMachinePartRoleTag = "machine";

/// @brief 3軸 (トライアド) の各軸の長さ [mm]
constexpr double kTriadLength = 40.0;
/// @brief 3軸の色 (X/Y/Zの順)
constexpr std::array<Color, 3> kTriadColors = {
        Color::FromRGB255(214, 39, 40), Color::FromRGB255(44, 160, 44),
        Color::FromRGB255(31, 119, 180)};
/// @brief 早送りの経路線の色
constexpr Color kRapidPathColor = Color::FromRGB255(159, 197, 232);
/// @brief 切削の経路線の色
constexpr Color kCutPathColor = Color::FromRGB255(184, 184, 184);
/// @brief 工具軸線と制御点マーカーの色
constexpr Color kToolAxisColor = Color::FromRGB255(242, 32, 0);
/// @brief 工具軸線を輪郭の最上端から延ばす長さ [mm]
constexpr double kToolAxisExtraLength = 30.0;
/// @brief 工具の金属材質の金属度 (`MetallicSurfaceIds`の面に設定する値)
/// @note 描画側の材質パラメータに対応する. GUI間で値を共有するために置く
constexpr float kToolMetallic = 0.2f;
/// @brief 工具の金属材質の粗さ (`MetallicSurfaceIds`の面に設定する値)
constexpr float kToolRoughness = 0.4f;

/// @brief シーン構築の設定
struct SceneBuildOptions {
    /// @brief 形状読込の設定
    GeometryLoadOptions geometry;
    /// @brief 経路線の円弧の折れ線化の弦誤差 [mm]
    double arc_chord_tolerance = 0.05;
    /// @brief ワーク座標系の3軸 (`workframe:<id>`) を作るか
    bool build_work_frames = true;
    /// @brief 工具 (`tool:<n>`) を作るか
    bool build_tools = true;
    /// @brief 機械座標系と取り付けフレームの3軸 (`triad:*`) を作るか
    bool build_triads = true;
    /// @brief 工具軸線を輪郭の最上端から延ばす長さ [mm]
    /// @note 0以下なら工具軸線と制御点マーカーを作らない
    double tool_axis_extra = kToolAxisExtraLength;
    /// @brief 形状のアセンブリを呼び出し側から供給する関数
    /// @note `MachiningSetup::Geometries()`の各形状について`LoadGeometry`の前に
    ///       呼ぶ. `nullptr`以外を返した場合はそれを形状のアセンブリとして用い,
    ///       ファイルは読まない. `nullptr`を返した場合、または未設定の場合は
    ///       `LoadGeometry`で読む. 呼び出し側で読み込み済みのアセンブリ
    ///       (CAM側が所有するワーク等) を二重に読まずに木に組み込む用途
    /// @note 供給されたアセンブリの名前 (`Metadata().name`)、大域変換、可視性は
    ///       `Build`で上書きする. 既に他の親を持つ場合はその親から取り除いて
    ///       (`RemovalPolicy::kOrphan`) 所属コンポーネントの子にする.
    ///       `Clear`で`machine:<name>`ごと木から取り除かれるため、供給側は
    ///       `shared_ptr`を保持し続けること
    std::function<std::shared_ptr<models::Assembly>(const GeometryInstance&)>
            geometry_provider;
    /// @brief 機械部品と表示専用の要素を選択不可 (`lock.selectable = false`) にするか
    /// @note 対象は機械部品の形状 (`geometry:<name>`)、工具 (`tool:<n>`)、3軸
    ///       (`triad:*`/`workframe:<id>`)、経路線 (`paths:<id>`)、工具軌跡
    ///       (`trajectory:`)、動作軌跡 (`trace:*`). モデル (`model:<name>`) と
    ///       `attach:<id>`は対象外で、CAM側の面選択等の対象のまま残る.
    ///       アセンブリの選択可否は祖先方向にANDで合成されるため部分木全体に効く
    bool lock_selection = false;
};

/// @brief ワークビュー (ワーク座標系に固定した表示) の設定
struct WorkViewOptions {
    /// @brief `work_mount`コンポーネント自身の形状 (テーブル等) を表示するか
    bool show_work_mount_parts = false;
};

/// @brief 名前を持つ子アセンブリを作成し、親に追加する
/// @param parent 親
/// @param name アセンブリの名前
/// @param transform 親に対する変換 (省略時は単位行列)
/// @return 追加した子
std::shared_ptr<models::Assembly> MakeChildAssembly(
        models::Assembly& parent, std::string_view name,
        const igesio::Matrix4d& transform = igesio::Matrix4d::Identity());

/// @brief 直接の子アセンブリを名前で探す
/// @param parent 親
/// @param name アセンブリの名前
/// @return 最初に見つかった子. 無ければ`nullptr`
std::shared_ptr<models::Assembly> FindChildAssembly(
        const models::Assembly& parent, std::string_view name);

/// @brief 加工セットアップから作るシーン (アセンブリ木とその操作)
/// @note `Build`で木を作り、`ApplyPose`/`SetActiveTool`/各`Set*Visible`で共有状態
///       (大域変換と可視性) を更新する. 可視性は全ビューで共有され、ビューごとの
///       表示範囲はレンダラの表示フィルタで設定する (`WorkViewHiddenIds`等).
/// @note `Build`後は`MachiningSetup`を参照しない (運動学モデル、工具表、ワーク座標系
///       のコピーを持つ). 姿勢、工具、経路線の操作は`Build`前には`std::logic_error`
///       を送出し、可視性の切り替えは`Build`前には何もしない.
class MachineScene {
 public:
    /// @brief セットアップからアセンブリ木を作る
    /// @param setup 加工セットアップ
    /// @param root 木を追加するルートアセンブリ (`machine:<name>`をその子にする)
    /// @param options 構築の設定
    /// @throw std::invalid_argument `root`が`nullptr`、工具輪郭が不正,
    ///        またはプリミティブの寸法が正しくない場合 (`MakeToolAssembly`/
    ///        `LoadGeometry`から伝播)
    /// @note 既に構築済みなら先に`Clear`する. 形状の読込に失敗した形状
    ///       (ファイルの不在、不正なファイル、対応外の形式等) は警告を`Warnings`に
    ///       追加し、その形状のアセンブリは作らない. 構築後の姿勢は全コンポーネントが
    ///       単位行列 (ゼロポーズ)、表示中の工具は`setup.InitialTool()`
    void Build(const MachiningSetup& setup,
               const std::shared_ptr<models::Assembly>& root,
               const SceneBuildOptions& options = {});

    /// @brief `machine:<name>`をルートから取り除き、未構築の状態に戻す
    /// @note 未構築なら何もしない
    void Clear();

    /// @brief 構築済みか
    bool IsBuilt() const { return machine_ != nullptr; }

    /**
     * 構築元の情報
     */

    /// @brief 運動学モデル (セットアップからのコピー)
    /// @throw std::logic_error 未構築の場合
    const MachineModel& Model() const;

    /// @brief 工具表 (工具番号→定義)
    /// @note セットアップからのコピー
    const std::map<int, ToolAssemblySpec>& Tools() const { return tools_; }

    /// @brief ワーク座標系 (定義順)
    /// @note セットアップからのコピー
    const std::vector<WorkFrame>& WorkFrames() const { return work_frames_; }

    /// @brief 初期ワークオフセットid
    /// @note `ClState::work_offset`が空 (未選択) の場合のデフォルト値
    const std::string& InitialWorkOffset() const { return initial_work_offset_; }

    /// @brief 構築時の警告 (形状の読込等)
    const std::vector<Diagnostic>& Warnings() const { return warnings_; }

    /**
     * 姿勢と工具
     */

    /// @brief 軸変位量の姿勢を全コンポーネントに適用する
    /// @param q 全軸の軸変位量
    /// @throw std::logic_error 未構築の場合
    /// @throw std::invalid_argument `q`の長さが軸数と異なる場合
    void ApplyPose(const JointVector& q);

    /// @brief 全コンポーネントをゼロポーズ (単位行列) に戻す
    /// @throw std::logic_error 未構築の場合
    void ResetToZeroPose();

    /// @brief 表示する工具を選択する
    /// @param number 工具番号 (`kNoTool`なら全工具を非表示)
    /// @throw std::logic_error 未構築の場合
    /// @note 該当する`tool:<n>`のみ表示し、工具表に無い番号は全工具を非表示にする
    void SetActiveTool(int number);

    /// @brief 表示中の工具番号
    /// @return 全工具が非表示なら`kNoTool`
    int ActiveTool() const { return active_tool_; }

    /**
     * 可視性の切り替え (共有状態. 未構築なら何もしない)
     */

    /// @brief 全工具のホルダ部の可視性を設定する
    void SetHolderVisible(bool visible);
    /// @brief 全ワークオフセットの経路線 (`paths:<id>`) の可視性を設定する
    void SetPathsVisible(bool visible);
    /// @brief 経路線のうち早送り (`paths:<id>/rapid`) の可視性を設定する
    void SetRapidPathsVisible(bool visible);
    /// @brief ワーク座標系の3軸 (`workframe:<id>`) の可視性を設定する
    void SetWorkFramesVisible(bool visible);
    /// @brief 機械座標系と取り付けフレームの3軸 (`triad:*`) の可視性を設定する
    void SetTriadsVisible(bool visible);
    /// @brief 全工具の工具軸線と制御点マーカーの可視性を設定する
    void SetToolAxisVisible(bool visible);
    /// @brief 機械部品の形状 (`geometry:<name>`) の可視性を設定する
    /// @note 干渉専用 (`GeometryInstance::visible == false`) の形状は変更しない
    void SetMachinePartsVisible(bool visible);
    /// @brief 同じ役割のモデル (`model:<name>`) の可視性を設定する
    /// @param role モデルの役割
    /// @param visible 設定する可視性
    /// @note 干渉専用 (`GeometryInstance::visible == false`) のモデルは変更しない
    void SetModelRoleVisible(ModelRole role, bool visible);

    /**
     * 経路線
     */

    /// @brief 経路線 (`paths:<id>/rapid`と`cut`) を作り直す
    /// @param program CLプログラム
    /// @param[out] warnings 警告の追加先 (`nullptr`なら追加しない)
    /// @throw std::logic_error 未構築の場合
    /// @throw std::invalid_argument 構築時の`arc_chord_tolerance`が正でない,
    ///        または法線がゼロベクトルの円弧がある場合 (`DiscretizeArc`から伝播)
    /// @note 同じワークオフセット・同じ種別 (早送りか否か) の連続区間ごとに1本の
    ///       3次元折れ線 (Type 106 Form 12) を作る. 円弧は弦誤差で折れ線化する.
    ///       次のレコードは経路線に含めず、その前後で区間を分ける.
    ///       (1) 機械座標の移動、(2) 軸の指令のみの移動, (3) 始点の分からない円弧,
    ///       (4) ワーク座標系に無いidを選択中の移動 (idごとに1回警告する).
    ///       状態レコード、ドウェル、区切りは区間を分けない.
    ///       ワークオフセット未選択の間は`InitialWorkOffset()`に従う
    void RebuildPaths(const ClProgram& program,
                      std::vector<Diagnostic>* warnings = nullptr);

    /**
     * アセンブリの取得 (未構築、または該当が無ければ`nullptr`)
     */

    /// @brief `machine:<name>`を取得する
    std::shared_ptr<models::Assembly> MachineAssembly() const { return machine_; }
    /// @brief コンポーネントのアセンブリを名前で取得する
    /// @param name コンポーネント名
    std::shared_ptr<models::Assembly> ComponentAssembly(std::string_view name) const;
    /// @brief `tool_mount`コンポーネントのアセンブリを取得する
    std::shared_ptr<models::Assembly> ToolMountAssembly() const;
    /// @brief `work_mount`コンポーネントのアセンブリを取得する
    std::shared_ptr<models::Assembly> WorkMountAssembly() const;
    /// @brief 工具のアセンブリ (`tool:<n>`) を番号で取得する
    /// @param number 工具番号
    std::shared_ptr<models::Assembly> ToolAssembly(int number) const;
    /// @brief 全工具のアセンブリのID (工具番号→`tool:<n>`のID)
    /// @note アニメーションの可視性トラックの対象に用いる
    std::map<int, ObjectID> ToolAssemblyIds() const;
    /// @brief 形状のアセンブリをインデックスで取得する
    /// @param index `MachiningSetup::Geometries()`のインデックス
    /// @return `geometry:<name>`または`model:<name>`. 読めなかった形状は`nullptr`
    std::shared_ptr<models::Assembly> GeometryAssembly(std::size_t index) const;
    /// @brief 形状の数 (`Build`時の`MachiningSetup::Geometries().size()`)
    std::size_t GeometryCount() const { return geometries_.size(); }
    /// @brief モデルのアセンブリ (`model:<name>`) を名前で取得する
    /// @param name モデル名
    std::shared_ptr<models::Assembly> ModelAssembly(std::string_view name) const;
    /// @brief ワーク座標系の3軸 (`workframe:<id>`) をidで取得する
    /// @param id ワークオフセットid
    std::shared_ptr<models::Assembly> WorkFrameAssembly(std::string_view id) const;
    /// @brief 経路線 (`paths:<id>`) をidで取得する
    /// @param id ワークオフセットid
    std::shared_ptr<models::Assembly> PathsAssembly(std::string_view id) const;
    /// @brief 呼び出し側の追加物の取り付け先 (`attach:<id>`) をidで取得する
    /// @param id ワークオフセットid
    std::shared_ptr<models::Assembly> AttachAssembly(std::string_view id) const;
    /// @brief 工具軌跡の配置先 (`trajectory:`) を取得する
    std::shared_ptr<models::Assembly> TrajectoryAssembly() const;
    /// @brief 動作軌跡 (機械座標) の配置先 (`trace:machine`) を取得する
    std::shared_ptr<models::Assembly> MachineTraceAssembly() const;
    /// @brief 動作軌跡 (`work_mount`座標) の配置先 (`trace:work`) を取得する
    std::shared_ptr<models::Assembly> WorkTraceAssembly() const;

    /// @brief 金属材質を設定する面 (全工具の切れ刃部/シャンク部の回転曲面) のIDを集める
    /// @return Type 120のIDの一覧 (ホルダ部は含まない)
    std::vector<ObjectID> MetallicSurfaceIds() const;

    /**
     * ワークビューの支援
     */

    /// @brief `work_mount`コンポーネントの現在の累積変換を取得する
    /// @return ルートまでの大域変換の積. ルートが単位行列なら`work_mount`
    ///         コンポーネントの運動F_wm(q) (F_c(q)の`work_mount`に対するもの)
    /// @throw std::logic_error 未構築の場合
    /// @note レンダラの表示座標系 (`EntityRenderer::SetViewFrame`) に設定すると,
    ///       ワークに固定したビューになる. 再生中も現在の姿勢を反映する
    igesio::Matrix4d WorkMountWorldTransform() const;

    /// @brief ワークビューのレンダラで隠すアセンブリのIDを集める
    /// @param options ワークビューの設定
    /// @return 機械部品の形状 (`geometry:<name>`)、`triad:machine`,
    ///         `trace:machine`のID. 呼び出し側の追加物は含めない
    std::vector<ObjectID>
    WorkViewHiddenIds(const WorkViewOptions& options = {}) const;

    /// @brief 機械ビューのレンダラで隠すアセンブリのIDを集める
    /// @return `trajectory:`のID. 呼び出し側の追加物は含めない
    std::vector<ObjectID> MachineViewHiddenIds() const;

 private:
    /// @brief 形状1つ分のアセンブリと、可視性の切り替えに必要な属性
    struct GeometryNode {
        /// @brief 形状のアセンブリ (読めなかった形状は`nullptr`)
        std::shared_ptr<models::Assembly> assembly;
        /// @brief 形状の由来 (機械部品/モデル)
        GeometryInstance::Kind kind = GeometryInstance::Kind::kMachinePart;
        /// @brief 所有者の名前 (コンポーネント名、またはモデル名)
        std::string owner;
        /// @brief 所属コンポーネントのインデックス
        std::size_t carrier = 0;
        /// @brief モデルの役割 (モデルのときのみ)
        std::optional<ModelRole> role;
        /// @brief 描画対象か (`false`は干渉専用形状で、可視性の切り替えの対象外)
        bool visible = true;
    };

    /// @brief 未構築なら`std::logic_error`を送出する
    /// @param operation 例外の文言に含める操作名
    /// @throw std::logic_error 未構築の場合
    void RequireBuilt(std::string_view operation) const;
    /// @brief 名前付きアセンブリを取得する
    /// @param name 接頭辞付きの名前
    /// @return 無ければ`nullptr`
    std::shared_ptr<models::Assembly> FindNode(std::string_view name) const;
    /// @brief コンポーネントのアセンブリを全て作る
    void BuildComponents();
    /// @brief 機械座標系と取り付けフレームの3軸を作る
    void BuildTriads();
    /// @brief 形状を読み込み、所属コンポーネントの子に置く
    /// @param setup 加工セットアップ
    /// @param options 構築の設定
    void BuildGeometries(const MachiningSetup& setup,
                         const SceneBuildOptions& options);
    /// @brief 工具表の全工具を`tool_mount`コンポーネントの子に置く
    /// @param options 構築の設定
    void BuildTools(const SceneBuildOptions& options);
    /// @brief ワーク座標系ごとに3軸、経路線の配置先、取り付け先を作る
    /// @param options 構築の設定
    void BuildWorkFrames(const SceneBuildOptions& options);
    /// @brief 機械部品と表示専用の要素を選択不可にする (`lock_selection`)
    /// @note モデル (`model:<name>`) と`attach:<id>`は変更しない
    void LockSelection();

    /// @brief 運動学モデル (未構築なら`std::nullopt`)
    std::optional<MachineModel> model_;
    /// @brief 工具表
    std::map<int, ToolAssemblySpec> tools_;
    /// @brief ワーク座標系 (定義順)
    std::vector<WorkFrame> work_frames_;
    /// @brief 初期ワークオフセットid
    std::string initial_work_offset_;
    /// @brief 経路線の円弧の折れ線化の弦誤差 [mm]
    double arc_chord_tolerance_ = 0.05;
    /// @brief 木を追加したルート (非所有)
    std::weak_ptr<models::Assembly> root_;
    /// @brief `machine:<name>` (未構築なら`nullptr`)
    std::shared_ptr<models::Assembly> machine_;
    /// @brief コンポーネントのアセンブリ (`MachineModel::Component()`の順)
    std::vector<std::shared_ptr<models::Assembly>> components_;
    /// @brief 形状 (`MachiningSetup::Geometries()`の順)
    std::vector<GeometryNode> geometries_;
    /// @brief 工具のアセンブリ (工具番号→`tool:<n>`)
    std::map<int, std::shared_ptr<models::Assembly>> tool_assemblies_;
    /// @brief 名前付きアセンブリ (接頭辞付きの名前→アセンブリ)
    /// @note `triad:*`/`trace:*`/`trajectory:`/`workframe:<id>`/`paths:<id>`/
    ///       `attach:<id>`. コンポーネント、形状、工具は含まない
    std::map<std::string, std::shared_ptr<models::Assembly>> nodes_;
    /// @brief 表示中の工具番号
    int active_tool_ = kNoTool;
    /// @brief 順運動学の出力バッファ (毎フレームの再確保を避ける)
    std::vector<igesio::Matrix4d> forward_buffer_;
    /// @brief 構築時の警告
    std::vector<Diagnostic> warnings_;
};

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SCENE_MACHINE_SCENE_H_
