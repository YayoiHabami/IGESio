/**
 * @file examples/gui/iges_viewer_gui.h
 * @brief IGESのエンティティを表示・編集する単一GUIクラス
 * @author Yayoi Habami
 * @date 2025-08-05
 * @copyright 2025 Yayoi Habami
 * @note GUIは「メニューバー + 左Outliner + 右Inspector + 下StatusBar + 中央Viewport」
 *       の固定レイアウト(ビューポート端にアンカー)で構成する.
 */
#ifndef EXAMPLES_GUI_IGES_VIEWER_GUI_H_
#define EXAMPLES_GUI_IGES_VIEWER_GUI_H_

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "igesio/graphics/core/gl_backend.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <igesio/entities/entity_base.h>
#include <igesio/models/scene.h>
#include <igesio/graphics/renderer.h>

#include "./input_config.h"



namespace igesio::graphics {

/// @brief IGESデータを表示・編集する単一GUIクラス
/// @note GLFWとImGuiを使用する. ファイルの読み込み(IGES、およびSTL拡張有効時は
///       STL)、Sceneによる選択/編集、Assemblyツリーの操作までを一手に担う.
///       読み込んだファイルは固定のRoot直下の子Assemblyとして保持し、複数ファイル
///       の同時表示(追加読込)に対応する(起動はiges_viewer.cppのmainが行う).
class IgesViewerGUI {
 private:
    /// @brief ウィンドウ
    GLFWwindow* window_ = nullptr;
    /// @brief レンダラ
    EntityRenderer renderer_;
    /// @brief セッション状態(root + 選択セット群 + ピックフィルタ)
    ///        を一元管理するオブジェクト
    /// @note Rootは構築時に固定し、以後差し替えない. 読み込んだファイルは
    ///       Root直下の子Assemblyとして追加・置き換えする.
    std::unique_ptr<models::Scene> scene_;
    /// @brief MSAA (マルチサンプリング) のサンプル数 (0で無効)
    int msaa_samples_ = 0;

    /// @brief ドラッグ中のカメラ操作モード
    enum class DragMode { kNone, kRotate, kPan, kZoom };
    /// @brief 現在のドラッグ操作モード (ボタン押下時に確定する)
    DragMode drag_mode_ = DragMode::kNone;
    /// @brief マウス操作のキー割り当て設定
    InputConfig input_config_;
    /// @brief 画面上における最後のマウス位置 (x, y)
    double last_x_ = 0.0, last_y_ = 0.0;
    /// @brief 選択ボタン押下時のマウス位置 (x, y) (クリック/ドラッグ判別用)
    double press_x_ = 0.0, press_y_ = 0.0;
    /// @brief 範囲選択 (ドラッグ) 中か
    bool is_box_selecting_ = false;
    /// @brief 選択中エンティティの交差座標 (表示用; キーはエンティティID)
    std::unordered_map<ObjectID, Vector3d> selected_hit_positions_;
    /// @brief 再描画が必要か
    bool needs_redraw_ = true;
    /// @brief 起動後の初回ファイル読み込みでfit view済みか
    bool initial_fit_done_ = false;

    /// @brief 削除系操作のポリシー (0:Reject, 1:Cascade, 2:Orphan; ImGui用)
    int removal_policy_ = 0;
    /// @brief 選択中の色の系統(物体色＋材質プリセット)のインデックス
    int color_scheme_index_ = 0;
    /// @brief Outlinerで操作対象としてフォーカス中のAssemblyノードID
    std::optional<ObjectID> focused_assembly_id_;
    /// @brief 直近の編集操作の結果メッセージ (ステータスバー表示用)
    std::string last_edit_status_;
    /// @brief ファイル読込ポップアップを開く要求 (メニューから立てる)
    bool request_load_popup_ = false;
    /// @brief 遅延実行する構造編集 (ツリー走査中の変更によるダングリングを避ける)
    /// @note コンテキストメニュー等で設定し、全パネル描画後にフレーム末尾で1回実行する.
    std::function<void()> pending_action_;

    /// @brief エンティティタイプごとの(表示対象)エンティティリスト
    std::map<entities::EntityType,
             std::vector<std::shared_ptr<entities::IEntityIdentifier>>> entities_;
    /// @brief エンティティタイプごとの表示フラグ
    std::map<entities::EntityType, bool> show_entity_;
    /// @brief 全エンティティを表示するフラグ
    bool show_all_ = true;
    /// @brief 起動時に一度だけ読み込むファイル (IGES/STL)
    std::string initial_iges_file_;

 public:
    /// @brief コンストラクタ
    /// @param width ウィンドウ幅の初期値 [px]
    /// @param height ウィンドウ高さの初期値 [px]
    /// @param msaa_samples マルチサンプリングのサンプル数 (0で無効)
    /// @param initial_iges_file 起動時に読み込むファイル (空で無し)
    /// @throw std::runtime_error ウィンドウの初期化に失敗した場合
    explicit IgesViewerGUI(const int = kDefaultDisplayWidth,
                           const int = kDefaultDisplayHeight,
                           const int = 0,
                           const std::string& = "");

    /// @brief デストラクタ
    ~IgesViewerGUI();

    // コピー禁止 (renderer_が非コピー)
    IgesViewerGUI(const IgesViewerGUI&) = delete;
    IgesViewerGUI& operator=(const IgesViewerGUI&) = delete;

    /// @brief レンダラを取得する (const)
    const EntityRenderer& Renderer() const { return renderer_; }
    /// @brief レンダラを取得する (非const)
    EntityRenderer& Renderer() { return renderer_; }

    /// @brief マウス操作のキー割り当て設定を取得する
    const InputConfig& GetInputConfig() const { return input_config_; }
    /// @brief マウス操作のキー割り当て設定を注入する
    void SetInputConfig(const InputConfig& config) { input_config_ = config; }

    /// @brief 実行 (イベントループを開始する)
    /// @param vsync 垂直同期を有効にするか
    void Run(const bool = true);

    /// @brief 現在の描画状態を画像へ保存する
    /// @param filename 画像ファイルのパス
    void CaptureScreenshot(const std::string&);

 private:
    /**
     * モデルの読み込み・同期
     */

    /// @brief ファイルを拡張子で判別して読み込む (読込経路のエントリポイント)
    /// @param filename 読み込むファイルのパス
    /// @param replace trueの場合は読込済みモデルを全て置き換える
    /// @note STL拡張有効時は.stl(大文字小文字無視)をSTLとして読み込み、
    ///       それ以外はIGESとして読み込む
    void LoadFile(const std::string& filename, bool replace);

    /// @brief 指定された型のファイルを読み込み、Root直下の子Assemblyとして組み込む
    /// @param filename 読み込むファイルのパス
    /// @param replace trueの場合は読込済みモデルを全て置き換える
    /// @param type_name ファイルの型を表す文字列 (例: "IGES", "STL")
    /// @param loader ファイルの型に応じた読み込み関数 (LoadIgesFileやLoadStlFileなど)
    void LoadFileByType(
        const std::string& filename, bool replace,
        const std::string& type_name,
        const std::function<std::shared_ptr<models::Assembly>(const std::string&)>& loader);

    /// @brief 読み込んだ子Assemblyをシーンへ組み込む (読込経路共通の後処理)
    /// @param child 追加する子Assembly (Metadata().nameは設定済みであること)
    /// @param replace trueの場合は既存の全エンティティ・子Assemblyを先に除去する
    /// @note Rootは固定のまま、子Assemblyの追加/一掃のみを行う
    ///       (SceneとRootの束ねを維持し、選択状態や色オーバーライドを保つ)
    void AttachLoadedAssembly(const std::shared_ptr<models::Assembly>& child,
                              bool replace);

    /// @brief 型別フィルタUI用のキャッシュへ登録する (検証診断の表示を含む)
    /// @note 描画オブジェクト自体はレンダラがSceneツリーとの突き合わせで
    ///       遅延生成するため、ここではUI状態のみを構築する
    void CacheEntityType(const std::shared_ptr<entities::IEntityIdentifier>& entity);

    /// @brief 型別表示フラグ(show_entity_)をレンダラの表示フィルタへ反映する
    void ApplyDisplayFilter();

    /// @brief 選択中の色の系統(物体色＋材質プリセット)をモデル全体へ適用する
    /// @note 物体色はrootのColorOverrideとして設定し(個別ノード色は最近接優先で温存)、
    ///       材質は表示対象の面エンティティへ個別にSetMaterialPropertyする
    void ApplyColorScheme();

    /// @brief モデル(Assemblyツリー)編集後の同期処理
    /// @note 消えたIDを選択/hit座標と型別キャッシュ (UI用) から除去する.
    ///       描画オブジェクトの破棄はレンダラのSweepが自動で行う.
    void OnModelEdited();

    /**
     * パネル描画 (固定レイアウト)
     */

    /// @brief 上部メニューバー (File/View/Select/Help) を描画する
    void RenderMenuBar();
    /// @brief ファイル読込ポップアップを描画する (メニューから要求された場合)
    void RenderLoadPopup();
    /// @brief 左Outliner (Assemblyツリー) を描画する
    void RenderOutliner();
    /// @brief 右Inspector (選択サマリ + 文脈依存プロパティ) を描画する
    void RenderInspector();
    /// @brief 下部ステータスバーを描画する
    void RenderStatusBar();
    /// @brief 範囲選択中のラバーバンドを描画する
    void RenderViewportOverlay();

    /// @brief Assemblyノードを再帰的にツリー表示する
    void RenderAssemblyNode(models::Assembly& node);
    /// @brief エンティティとその参照先を再帰的にツリー表示する
    /// @param id 表示するエンティティのID
    /// @param path 現在の再帰経路上のID集合 (循環参照による無限再帰のガード用)
    void RenderEntityNode(const ObjectID& id,
                          std::unordered_set<ObjectID>& path);
    /// @brief エンティティ行のクリックによる選択を処理する (Ctrlでトグル)
    void HandleEntityRowClick(const ObjectID& id);
    /// @brief ツリー内で解決できる参照先のIDを取得する (重複除去済み)
    /// @param id 参照元エンティティのID
    /// @return Outlinerで子階層として表示する参照先IDのリスト
    std::vector<ObjectID> ResolvedReferences(const ObjectID& id) const;
    /// @brief Assemblyノードの右クリックコンテキストメニューを描画する
    void RenderNodeContextMenu(models::Assembly& node);
    /// @brief Inspectorの選択サマリ部を描画する
    void RenderSelectionSection();
    /// @brief Inspectorのプロパティ部 (フォーカス対象に応じて切替) を描画する
    void RenderPropertiesSection();
    /// @brief フォーカス中Assemblyノードのプロパティ編集を描画する
    void RenderAssemblyProperties(models::Assembly& node);

    /**
     * 編集操作
     */

    /// @brief 選択中エンティティを削除する (removal_policy_に従う)
    void DeleteSelectedEntities();
    /// @brief 選択中エンティティを新規子Assemblyへまとめる
    /// @note フォーカス中ノードがあればその直下、無ければルート直下に作成する
    void GroupSelectionIntoNewAssembly();
    /// @brief フォーカス中のAssemblyノードを取得する (無ければnullptr)
    models::Assembly* FocusedNode();
    /// @brief removal_policy_ を RemovalPolicy へ変換する
    models::RemovalPolicy CurrentRemovalPolicy() const {
        switch (removal_policy_) {
            case 1: return models::RemovalPolicy::kCascade;
            case 2: return models::RemovalPolicy::kOrphan;
            default: return models::RemovalPolicy::kReject;
        }
    }

    /**
     * コールバック・入力
     */

    /// @brief GLFWエラーハンドラ
    static void ErrorCallback(const int error, const char* description) {
        std::cerr << "GLFW Error " << error << ": " << description << std::endl;
    }

    /// @brief コールバック関数を設定する
    void SetupCallbacks();
    /// @brief マウスボタンのコールバック
    void MouseButtonCallback(const int, const int, const int);
    /// @brief クリックによるエンティティ選択を処理する
    void HandleClickSelection(const double, const double, const int);
    /// @brief 範囲選択 (矩形ドラッグ) を処理する
    void HandleBoxSelection(const double, const double, const int);
    /// @brief キーボードショートカット (Del/Ctrl+G/Esc/F) を処理する
    void HandleHotkeys();
    /// @brief 標準軸ビューへ切り替え、モデル全体を画面に収める
    /// @param view 標準軸ビューの種別
    /// @note Camera::SetStandardViewで向きを定型化した後、FitViewで枠に収める
    void ApplyStandardView(const StandardView);
    /// @brief マウスカーソル位置のコールバック
    void CursorPositionCallback(const double, const double);
    /// @brief スクロールホイールのコールバック
    void ScrollCallback(const double, const double);
    /// @brief ウィンドウリサイズのコールバック
    void FramebufferSizeCallback(const int, const int);
    /// @brief キー入力のコールバック (再描画要求のみ; キー状態はImGuiが処理)
    void KeyCallback(const int, const int, const int) { needs_redraw_ = true; }
};

}  // namespace igesio::graphics

#endif  // EXAMPLES_GUI_IGES_VIEWER_GUI_H_
