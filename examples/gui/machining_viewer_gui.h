/**
 * @file examples/gui/machining_viewer_gui.h
 * @brief 簡易CAM GUI (機械ビューとワークビューの2ビューで、プロジェクトの読込,
 *        ジョグ、動作生成、再生を行う)
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 * @note machines拡張の実用例. `IgesViewerGUI`からは継承せず、GLFW/ImGui/
 *       `EntityRenderer`の起動手順を独自に持つ. 1つの`Scene`を2つの
 *       `ViewPane` (機械ビュー、ワークビュー) で共有し、ワークビューのレンダラ
 *       には表示座標系 (`SetViewFrame`) とアセンブリ単位の表示フィルタを設定する.
 * @note 画面は端に固定したパネル (左: Project/Programs/Source、右: Machine/
 *       Tools/Display/Log)、中央の2ビューと再生バー、下のステータスバーから成る.
 *       状態は「未読込 → 読込済み → バインド済み → 再生中」の4つで、各パネルの
 *       有効/無効はこの状態で決まる.
 */
#ifndef EXAMPLES_GUI_MACHINING_VIEWER_GUI_H_
#define EXAMPLES_GUI_MACHINING_VIEWER_GUI_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <igesio/graphics/core/i_open_gl.h>
#include <igesio/models/assembly.h>
#include <igesio/models/scene.h>
#include <igesio/extensions/animation/animation_player.h>
#include <igesio/extensions/machines.h>

#include "./input_config.h"
#include "./machining_view_pane.h"

namespace igesio::graphics {

/// @brief GUIの状態
enum class SessionState {
    /// @brief プロジェクト未読込
    kEmpty,
    /// @brief 読込済み (ジョグと工具の切り替えができる)
    kReady,
    /// @brief 動作生成済み (アニメーションをバインド済み. 再生できる)
    kBound,
    /// @brief 再生中
    kPlaying,
};

/// @brief 中央のビューのレイアウト
enum class ViewLayout {
    /// @brief 機械ビューとワークビューを並べる
    kDual,
    /// @brief 機械ビューのみ
    kMachineOnly,
    /// @brief ワークビューのみ
    kWorkOnly,
};

/// @brief 左パネルのタブ
enum class LeftTab {
    /// @brief プロジェクト
    kProject,
    /// @brief プログラムと動作生成
    kPrograms,
    /// @brief NC/CLの行
    kSource,
};

/// @brief 右パネルのタブ
enum class RightTab {
    /// @brief 機械構造ツリーとジョグ
    kMachine,
    /// @brief 登録工具
    kTools,
    /// @brief 表示の切り替え
    kDisplay,
    /// @brief 診断の一覧
    kLog,
};

/// @brief 診断の発生段階
enum class LogStage {
    /// @brief プロジェクト定義の読込
    kProject,
    /// @brief 加工セットアップの構築
    kSetup,
    /// @brief シーンの構築 (形状の読込)
    kScene,
    /// @brief プログラムの読込と経路線
    kProgram,
    /// @brief 動作生成
    kMotion,
    /// @brief クリップの生成
    kClip,
    /// @brief 工具軌跡の生成
    kTrajectory,
    /// @brief GUIの操作 (ジョグ等)
    kGui,
};

/// @brief 発生段階の表示名
/// @param stage 発生段階
/// @return 表示名 (`"project"`等)
const char* LogStageName(LogStage stage);

/// @brief Logタブの1行
struct LogEntry {
    /// @brief 発生段階
    LogStage stage = LogStage::kGui;
    /// @brief 診断
    extensions::machines::Diagnostic diagnostic;
};

/// @brief プログラムファイルの行の一覧 (Sourceタブと現在行の表示に用いる)
struct SourceFile {
    /// @brief `[[program]]`のインデックス (`SourceLocation::program_index`)
    int program_index = 0;
    /// @brief 表示名 (`DisplayName(spec)`)
    std::string name;
    /// @brief 行 (0始まりのインデックス = 行番号 - 1)
    std::vector<std::string> lines;
    /// @brief 行番号 (1始まり) → その行から生成された最初のレコードのインデックス
    /// @note 該当が無い行は`std::nullopt`. 長さは`lines.size() + 1`
    std::vector<std::optional<std::size_t>> line_records;
};

/// @brief モーダルの種類
enum class ModalKind {
    /// @brief 表示していない
    kNone,
    /// @brief プロジェクトを開く (パス入力)
    kOpenProject,
    /// @brief 仮想機械でプログラムを開く
    kOpenVirtual,
    /// @brief ライブラリ検索パスの追加
    kAddLibraryDir,
    /// @brief スクリーンショットの保存先
    kScreenshot,
    /// @brief エラーの表示
    kError,
    /// @brief 操作一覧
    kControls,
    /// @brief バージョン情報
    kAbout,
};

/// @brief 仮想機械でプログラムを開くモーダルの入力
struct VirtualProjectInput {
    /// @brief 仮想機械の種類 (`VirtualMachineKind`のインデックス)
    int machine_kind = 2;
    /// @brief プログラムのパス
    std::string program_path;
    /// @brief プログラムの種別 (`ProgramType`のインデックス)
    int program_type = 0;
    /// @brief 工具径 [mm]
    float tool_diameter = 6.0f;
    /// @brief 工具長 [mm]
    float tool_length = 80.0f;
};

/// @brief 動作生成の設定のUI値 (`MotionOptions`に変換して用いる)
struct MotionOptionsUi {
    /// @brief 補間のサンプリングレート [1/s]
    float fps = 30.0f;
    /// @brief サンプリング点総数の上限
    int max_samples = 200000;
    /// @brief 区間を補間するか
    bool interpolate = true;
    /// @brief 送りが不明な切削区間の送り [mm/min] (内部ではmm/sに換算する)
    float fallback_feed_mm_per_min = 1000.0f;
    /// @brief 円弧の分割の弦誤差 [mm]
    float arc_chord_tolerance = 0.05f;
    /// @brief 回転角の解の選択方針 (0: 機械定義のまま、1〜3: `BranchPolicy`の順)
    int branch_index = 0;
    /// @brief 通過点ごとの所要時間を固定するか
    bool fixed_time = false;
    /// @brief 固定する所要時間 [s]
    float fixed_seconds = 0.1f;
};

/// @brief 実行制御 (`[run]`) のUI値
struct RunSettingsUi {
    /// @brief 開始工具 (0で未指定)
    int start_tool = 0;
    /// @brief 停止工具 (0で未指定)
    int stop_tool = 0;
    /// @brief 可動範囲外の扱い (`OvertravelPolicy`の順)
    int overtravel_index = 0;
};

/// @brief Machineタブの「Target」節のUI値 (`ClTarget`に変換して用いる)
struct ClTargetUi {
    /// @brief 制御点 (ワーク座標) [mm]
    float point[3] = {0.0f, 0.0f, 0.0f};
    /// @brief 工具軸方向 (ワーク座標)
    float tool_axis[3] = {0.0f, 0.0f, 1.0f};
    /// @brief ワークオフセット (`MachineScene::WorkFrames()`のインデックス)
    int work_offset_index = 0;
    /// @brief 工具 (0: 工具なし、1〜: `MachineScene::Tools()`の順)
    int tool_index = 0;
    /// @brief 直近の結果の文言
    std::string result;
};

/// @brief Displayタブの表示/非表示 (両ビューに効く`MachineScene`の表示/非表示)
struct DisplayFlags {
    /// @brief 経路線
    bool paths = false;
    /// @brief 経路線のうち早送り
    bool rapid_paths = false;
    /// @brief 動作軌跡 (機械座標)
    bool trace_machine = false;
    /// @brief 動作軌跡（ワーク取り付け部座標）
    bool trace_work = false;
    /// @brief 現在レコードの強調
    bool current_record = true;
    /// @brief ワーク座標系の3軸
    bool work_frames = true;
    /// @brief 機械座標系と取り付け部座標系の3軸
    bool triads = false;
    /// @brief 工具軸線と制御点マーカー
    bool tool_axis = false;
    /// @brief 工具のホルダ部
    bool holder = true;
    /// @brief 機械部品の形状
    bool machine_parts = true;
    /// @brief モデルの役割別 (`ModelRole`の順)
    std::array<bool, 4> model_roles = {true, true, true, true};
};

/// @brief 読込から動作生成までの段階ごとのデータ
/// @note GL非依存. 読込 (`MachiningViewerGUI::LoadProject`) で`project`〜
///       `sources`を、動作生成 (`GenerateMotion`) で`track`以降を設定する
struct MachiningSession {
    /// @brief プロジェクトファイルのパス (仮想機械のときは空)
    std::filesystem::path project_path;
    /// @brief ライブラリ検索パス
    std::vector<std::filesystem::path> library_dirs;
    /// @brief プロジェクト定義 (GUI上の変更を反映するコピー)
    std::optional<extensions::machines::ProjectDefinition> project;
    /// @brief 加工セットアップ (`project`から作る)
    std::optional<extensions::machines::MachiningSetup> setup;
    /// @brief シーン (アセンブリ木)
    extensions::machines::MachineScene scene;
    /// @brief 読み込んだプログラム (`LoadPrograms`の結果)
    std::vector<extensions::machines::LoadedProgram> loaded;
    /// @brief 連結し、実行範囲を限定したプログラム
    std::optional<extensions::machines::ClProgram> program;
    /// @brief 経路区間 (`EnumeratePaths(program)`)
    std::vector<extensions::machines::ClPathRange> paths;
    /// @brief プログラムファイルの行 (`loaded`と同じ順)
    std::vector<SourceFile> sources;
    /// @brief 動作のサンプル列
    std::optional<extensions::machines::MotionTrack> track;
    /// @brief レコードごとのワークオフセット (`scene.WorkFrames()`のインデックス)
    /// @note 未定義のidは-1. `ClState`の再生で作る
    std::vector<int> record_work_offsets;
    /// @brief レコード → 経路区間 (`paths`のインデックス)
    std::vector<std::size_t> path_of_record;
    /// @brief アニメーションの再生状態機械
    extensions::animation::AnimationPlayer player;
    /// @brief 診断の一覧
    std::vector<LogEntry> log;
};

/// @brief 簡易CAM GUI
/// @note `Run`で描画ループに入る. 描画は既定でイベント待ちで、再生中とドラッグ中
///       のみ連続再描画する
class MachiningViewerGUI {
 public:
    /// @brief コンストラクタ
    /// @param width ウィンドウ幅の初期値 [px]
    /// @param height ウィンドウ高さの初期値 [px]
    /// @param msaa_samples マルチサンプリングのサンプル数 (0で無効)
    /// @param initial_project 起動時に読み込むプロジェクト (空で無し)
    /// @param library_dirs ライブラリ検索パス (先頭優先)
    /// @throw std::runtime_error ウィンドウまたはGLの初期化に失敗した場合
    MachiningViewerGUI(int width, int height, int msaa_samples,
                       std::filesystem::path initial_project,
                       std::vector<std::filesystem::path> library_dirs);
    /// @brief デストラクタ (ImGuiとGLFWを終了する)
    ~MachiningViewerGUI();
    MachiningViewerGUI(const MachiningViewerGUI&) = delete;
    MachiningViewerGUI& operator=(const MachiningViewerGUI&) = delete;

    /// @brief 描画ループを実行する (ウィンドウが閉じられるまで戻らない)
    void Run();

 private:
    /**
     * 初期化と描画ループ
     */

    /// @brief 1フレームの時間駆動の更新 (再生の前進、現在レコードの追従,
    ///        ワークビューの表示座標系)
    /// @param dt_sec 前フレームからの実経過時間 [s]
    void OnFrameUpdate(double dt_sec);
    /// @brief 1フレームのImGuiの描画 (メニュー、パネル、ビュー、モーダル)
    void RenderFrame();
    /// @brief 現在の状態を`player`の状態から更新する
    void UpdateState();
    /// @brief 再描画を要求する
    void RequestRedraw() { needs_redraw_ = true; }

    /**
     * セッション (読込、動作生成、解除)
     */

    /// @brief プロジェクトファイルを読み込み、シーンを作る
    /// @param path プロジェクトファイルのパス
    /// @note 失敗時はエラーを表示して未読込に戻る
    void LoadProject(const std::filesystem::path& path);
    /// @brief 仮想機械の機械定義でプログラムを読み込み、シーンを作る
    /// @param input モーダルの入力
    void LoadVirtualProject(const VirtualProjectInput& input);
    /// @brief `session_.project`からセットアップ、シーン、プログラム、経路線を
    ///        作る (`LoadProject`/`LoadVirtualProject`の共通部)
    /// @throw std::exception 構築に失敗した場合 (呼び出し側で表示する)
    void FinishLoad();
    /// @brief `[[program]]`を読み込み、連結、実行範囲の限定、経路区間の列挙,
    ///        経路線の作り直し、行の一覧の読込を行う
    /// @return 読込に成功したら`true` (失敗は警告としてLogに追加する)
    bool LoadProgramsForScene();
    /// @brief 動作を生成し、アニメーションをバインドする
    /// @note UI値を実行制御に反映してセットアップを作り直し、プログラムの読込,
    ///       動作生成、クリップ生成、バインド、動作軌跡と工具軌跡の作り直しを
    ///       一括で行う. 失敗時はエラーを表示して読込済みの状態に留まる
    void GenerateMotion();
    /// @brief アニメーションを解除する (基準状態に復元. 動作軌跡と工具軌跡は残す)
    void ReleaseMotion();
    /// @brief UI値から動作生成の設定を作る
    extensions::machines::MotionOptions MotionOptionsFromUi() const;
    /// @brief 実行制御のUI値をプロジェクトの値で初期化する
    void InitializeRunSettingsUi();
    /// @brief レコードごとのワークオフセットと経路区間の索引を作る
    void BuildRecordIndices();
    /// @brief 現在レコードの変化を検出し、強調、現在の区間、Sourceの追従を更新する
    void TrackCurrentRecord();
    /// @brief イベントトラックの現在の値を取得する
    /// @param track_name トラック名 (`"record"`/`"tool"`/`"program"`)
    /// @return 現在時刻の値. 未バインド、トラック無し、先頭キーより前なら`std::nullopt`
    std::optional<std::int64_t> CurrentEventValue(std::string_view track_name) const;
    /// @brief 時刻に移動する
    /// @param time_sec 時刻 [s] (0〜総時間に丸める)
    void SeekToTime(double time_sec);
    /// @brief 前後のレコードに移動する (`"record"`イベントのキー列を基準にする)
    /// @param direction -1で前、+1で次
    void StepRecord(int direction);
    /// @brief レコードの先頭のサンプルの時刻に移動する
    /// @param record レコードのインデックス
    void SeekToRecord(std::size_t record);
    /// @brief 再生と一時停止を切り替える
    void TogglePlay();

    /**
     * ジョグ、工具、表示 (段10-c)
     */

    /// @brief ジョグのコンフィギュレーションを設定してシーンに適用する
    /// @param q 全軸の軸変位量
    void SetJogPose(const extensions::machines::JointVector& q);
    /// @brief 表示するコンフィギュレーションを取得する（バインド中は現在時刻の
    ///        サンプル、それ以外はジョグのコンフィギュレーション）
    extensions::machines::JointVector DisplayedPose() const;
    /// @brief 機械構造ツリーのコンポーネントを選択し、その形状を両ビューで強調する
    /// @param index コンポーネントのインデックス
    void SelectComponent(std::size_t index);
    /// @brief Targetの入力からコンフィギュレーションを求めて適用する
    void SolveTarget();
    /// @brief 表示/非表示をシーンに適用し直す (読込と動作生成の後)
    void ApplyDisplayFlags();
    /// @brief 現在レコードの強調の表示/非表示を設定する
    /// @param visible 表示するなら`true` (バインド中は現在レコードで更新し直す)
    void SetCurrentRecordVisible(bool visible);
    /// @brief 工具軌跡の区間単位の表示/非表示をUI値から適用する
    void ApplyTrajectoryVisibility();
    /// @brief 工具軌跡を生成の設定で作り直す
    void RebuildTrajectory();
    /// @brief セッションを空にする (バインドの解除、シーンの除去、Logの消去)
    void ClearSession();
    /// @brief 読込済みのデータを破棄する (Logとライブラリ検索パスは残す)
    void DiscardLoadedData();
    /// @brief プログラムファイルの行を読み、行番号からレコードへの索引を作る
    void ReadSourceFiles();
    /// @brief 工具と工具軌跡の金属材質を両ビューのレンダラに設定する
    void ApplyMaterials();
    /// @brief 両ビューの表示フィルタ (隠すアセンブリ) を設定する
    void ApplyViewFilters();
    /// @brief 診断をLogに追加する
    /// @param stage 発生段階
    /// @param diagnostics 診断
    void AppendLog(LogStage stage,
                   const std::vector<extensions::machines::Diagnostic>& diagnostics);
    /// @brief 指定した発生段階の診断をLogから取り除く
    /// @param stages 取り除く発生段階
    /// @note 段階をやり直す前に呼び、同じ診断が重複して残らないようにする
    void ClearLogStages(std::initializer_list<LogStage> stages);
    /// @brief エラーのモーダルを表示し、Logに警告として残す
    /// @param message 文言
    void ShowError(const std::string& message);
    /// @brief ステータスバーの文言を設定する
    /// @param message 文言
    void SetStatus(std::string message);
    /// @brief ビュー上のクリックで選択する
    /// @param pane クリックされたビュー
    /// @param click クリックの位置と修飾キー
    void SelectByPick(ViewPane& pane, const ClickInfo& click);
    /// @brief ビューのスクリーンショットをファイルに保存する
    /// @param pane 対象のビュー
    /// @param path 保存先
    void SaveScreenshot(ViewPane& pane, const std::string& path);

    /**
     * 画面 (machining_viewer_gui.cpp)
     */

    /// @brief メニューバーを描画する
    void RenderMenuBar();
    /// @brief 中央領域 (2ビュー、分割バー、再生バー) を描画する
    /// @param pos 領域の左上
    /// @param size 領域のサイズ
    void RenderCenter(const ImVec2& pos, const ImVec2& size);
    /// @brief 1ビューを描画し、クリックとスクリーンショットの要求を処理する
    /// @param pane 対象のビュー
    /// @param size 表示サイズ
    void RenderPane(ViewPane& pane, const ImVec2& size);
    /// @brief 再生バーを描画する
    void RenderPlaybackBar();
    /// @brief 再生バーの1行目 (トランスポートと時刻スライダ) を描画する
    void RenderTransportRow();
    /// @brief 再生バーの2行目 (速度、ループ、現在の情報) を描画する
    void RenderCurrentInfoRow();
    /// @brief 再生バーの3行目 (現在のNC指令値) を描画する
    void RenderCurrentNcRow();
    /// @brief ステータスバーを描画する
    void RenderStatusBar();
    /// @brief モーダルを描画する
    void RenderModals();
    /// @brief モーダルを開く
    /// @param kind 種類
    /// @param text 入力欄の初期値、またはエラーの文言
    void OpenModal(ModalKind kind, std::string text = {});
    /// @brief ホットキーを処理する
    void HandleHotkeys();

    /**
     * パネル (machining_viewer_panels.cpp)
     */

    /// @brief 左パネル (Project/Programs/Source) を描画する
    /// @param pos パネルの左上
    /// @param size パネルのサイズ
    void RenderLeftPanel(const ImVec2& pos, const ImVec2& size);
    /// @brief 右パネル (Machine/Tools/Display/Log) を描画する
    /// @param pos パネルの左上
    /// @param size パネルのサイズ
    void RenderRightPanel(const ImVec2& pos, const ImVec2& size);
    /// @brief Projectタブを描画する
    void RenderProjectTab();
    /// @brief Programsタブを描画する
    void RenderProgramsTab();
    /// @brief `[[program]]`の表を描画する (有効/無効の切り替え)
    void RenderProgramTable();
    /// @brief 実行制御と動作生成の設定を描画する
    void RenderMotionSettings();
    /// @brief 動作生成の統計を描画する
    void RenderMotionStatistics();
    /// @brief 経路区間の表を描画する (クリックで移動)
    void RenderPathTable();
    /// @brief Sourceタブを描画する
    void RenderSourceTab();
    /// @brief プログラム1つ分の行を描画する
    /// @param file 行の一覧
    /// @param current_line 現在レコードの行番号 (このプログラムでなければ0)
    void RenderSourceLines(const SourceFile& file, int current_line);
    /// @brief Machineタブを描画する
    void RenderMachineTab();
    /// @brief 機械構造ツリーを描画する
    void RenderKinematicTree();
    /// @brief 機械構造ツリーの1コンポーネント（と子要素）を描画する
    /// @param index コンポーネントのインデックス
    void RenderKinematicNode(std::size_t index);
    /// @brief ジョグ（軸ごとのスライダとコンフィギュレーションのボタン）を描画する
    void RenderJog();
    /// @brief Target節（制御点と工具軸方向からのコンフィギュレーション）を描画する
    void RenderTarget();
    /// @brief Toolsタブを描画する
    void RenderToolsTab();
    /// @brief 工具の部位要素の一覧を描画する
    /// @param spec 工具アセンブリ定義
    void RenderToolDetails(const extensions::machines::ToolAssemblySpec& spec);
    /// @brief Displayタブを描画する
    void RenderDisplayTab();
    /// @brief Displayタブの表示状態を描画する
    void RenderSharedDisplayFlags();
    /// @brief Displayタブのビューごとの設定を描画する
    void RenderViewSettings();
    /// @brief ビュー1つ分の設定 (表示モード、投影、背景、スクリーンショット) を描画する
    /// @param pane 対象のビュー
    void RenderViewColumn(ViewPane& pane);
    /// @brief Displayタブの工具軌跡の設定を描画する
    void RenderTrajectorySettings();
    /// @brief Logタブを描画する
    void RenderLogTab();

    /// @brief GLFWのエラーコールバック
    static void ErrorCallback(int error, const char* description);

    /**
     * メンバ
     */

    /// @brief ウィンドウ
    GLFWwindow* window_ = nullptr;
    /// @brief マルチサンプリングのサンプル数
    int msaa_samples_ = 0;
    /// @brief 起動時に読み込むプロジェクト (読込後に空にする)
    std::filesystem::path initial_project_;
    /// @brief 両ビューで共有するGLバックエンド
    std::shared_ptr<IOpenGL> gl_;
    /// @brief シーンのルート (`MachineScene::Build`の`root`)
    std::shared_ptr<models::Assembly> scene_root_;
    /// @brief 両ビューで共有するシーン (選択状態を含む)
    std::unique_ptr<models::Scene> scene_;
    /// @brief 機械ビュー
    std::unique_ptr<ViewPane> machine_pane_;
    /// @brief ワークビュー
    std::unique_ptr<ViewPane> work_pane_;
    /// @brief 最後にカーソルがあったビュー (ホットキーとメニューの対象)
    ViewPane* last_hovered_pane_ = nullptr;
    /// @brief マウス操作の割り当て
    InputConfig input_config_;
    /// @brief セッション
    MachiningSession session_;
    /// @brief 状態
    SessionState state_ = SessionState::kEmpty;
    /// @brief 中央のレイアウト
    ViewLayout layout_ = ViewLayout::kDual;
    /// @brief 2ビューの分割比 (機械ビューの幅の割合)
    float split_ratio_ = 0.5f;
    /// @brief 左パネルを表示するか
    bool show_left_panel_ = true;
    /// @brief 右パネルを表示するか
    bool show_right_panel_ = true;
    /// @brief 左パネルの選択タブ (`Show log`等でプログラムから切り替える)
    std::optional<LeftTab> requested_left_tab_;
    /// @brief 右パネルの選択タブ
    std::optional<RightTab> requested_right_tab_;
    /// @brief ワークビューの設定
    extensions::machines::WorkViewOptions work_view_options_;
    /// @brief Logタブで情報 (`Severity::kInfo`) も表示するか
    bool log_show_info_ = true;
    /// @brief 表示中のモーダル
    ModalKind modal_ = ModalKind::kNone;
    /// @brief モーダルを開く要求 (次のフレームの`RenderModals`で開く)
    bool modal_open_requested_ = false;
    /// @brief モーダルの表示文 (エラーの文言等)
    std::string modal_text_;
    /// @brief モーダルの入力欄 (パス等)
    std::array<char, 1024> modal_input_ = {};
    /// @brief スクリーンショットの対象のビュー
    ViewPane* screenshot_pane_ = nullptr;
    /// @brief 仮想機械のモーダルの入力
    VirtualProjectInput virtual_input_;
    /// @brief ステータスバーの文言
    std::string status_;
    /// @brief 再描画が必要か
    bool needs_redraw_ = true;
    /// @brief 連続再描画中か (再生中とドラッグ中)
    bool continuous_redraw_ = false;
    /// @brief 前フレームの時刻 [s]
    double last_frame_time_ = 0.0;
    /// @brief 直近の実測フレームレート [1/s] (再生中の表示用)
    double measured_fps_ = 0.0;
    /// @brief 直前のフレームの後にもう1フレーム描画するか
    /// @note ImGuiはポップアップの開閉等を次のフレームで反映するため,
    ///       イベントごとに2フレーム描画する
    bool render_again_ = false;
    /// @brief 読込後に両ビューの`FitView`を行ったか (再読込ではカメラを保つ)
    bool fit_done_ = false;
    /// @brief 動作生成の設定のUI値
    MotionOptionsUi motion_ui_;
    /// @brief 実行制御のUI値
    RunSettingsUi run_ui_;
    /// @brief 動作軌跡の設定
    extensions::machines::MotionTraceOptions trace_options_;
    /// @brief 工具軌跡の生成の設定
    extensions::machines::ToolTrajectoryOptions trajectory_options_;
    /// @brief 工具軌跡の区間単位の表示/非表示 (`always_visible_run`は現在の区間)
    extensions::machines::TrajectoryVisibility trajectory_visibility_;
    /// @brief 前フレームの`"record"`イベントの値
    std::optional<std::int64_t> last_record_;
    /// @brief 現在の経路区間 (`session_.paths`のインデックス)
    std::optional<std::size_t> current_run_;
    /// @brief 前フレームの`"program"`イベントの値 (Sourceのタブ切り替え用)
    std::optional<std::int64_t> last_program_;
    /// @brief Sourceの追従スクロールの要求
    bool source_follow_pending_ = false;
    /// @brief 動作生成の設定が変更され、再生成が必要か
    bool settings_dirty_ = false;
    /// @brief 直近の動作生成の結果
    std::string generate_result_;
    /// @brief 再生速度のUI値
    float speed_ui_ = 1.0f;
    /// @brief ループ再生のUI値
    bool loop_ui_ = false;
    /// @brief Sourceタブで現在行に追従するか
    bool follow_source_ = true;
    /// @brief ジョグのコンフィギュレーション
    /// @note 全軸の軸変位量. 読込済みかつ未バインドのとき有効
    extensions::machines::JointVector jog_q_;
    /// @brief 機械構造ツリーで選択したコンポーネント
    std::optional<std::size_t> selected_component_;
    /// @brief Target節のUI値
    ClTargetUi target_ui_;
    /// @brief 表示/非表示
    DisplayFlags display_flags_;
    /// @brief 工具軌跡の区間の限定 (0: 全区間、1〜: 区間番号+1)
    int trajectory_only_run_ui_ = 0;
    /// @brief 現在の区間を常に表示するか
    bool trajectory_always_current_ = true;
    /// @brief 工具軌跡の不透明度のオーバーライドを有効にするか
    bool trajectory_opacity_enabled_ = false;
    /// @brief 工具軌跡の不透明度のUI値
    float trajectory_opacity_ui_ = 0.5f;
    /// @brief 工具軌跡の生成の設定が変更され、作り直しが必要か
    bool trajectory_dirty_ = false;
};

}  // namespace igesio::graphics

#endif  // EXAMPLES_GUI_MACHINING_VIEWER_GUI_H_
