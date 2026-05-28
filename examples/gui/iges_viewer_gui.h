/**
 * @file examples/gui/iges_viewer_gui.h
 * @brief IGESのエンティティを表示するGUIクラス
 * @author Yayoi Habami
 * @date 2025-08-05
 * @copyright 2025 Yayoi Habami
 * @note このファイルは'glad/gl.h'をインクルードしているため、
 *       このファイルをインクルードする際は、他のOpenGL/GLFWヘッダを
 *       インクルードする前にこのファイルをインクルードすること
 */
#ifndef EXAMPLES_GUI_IGES_VIEWER_GUI_H_
#define EXAMPLES_GUI_IGES_VIEWER_GUI_H_

#include <iostream>
#include <string>
#include <unordered_map>

#include "igesio/graphics/core/open_gl.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <igesio/graphics/renderer.h>

#include "./input_config.h"



namespace igesio::graphics {

/// @brief IGESデータを表示する簡易的なGUIクラス
/// @note GLFWとImGuiを使用してIGESデータを表示する
class IgesViewerGUI {
 protected:
    /// @brief ウィンドウ
    GLFWwindow* window_ = nullptr;
    /// @brief レンダー
    EntityRenderer renderer_;

    /// @brief MSAA (マルチサンプリング) のサンプル数 (0で無効)
    int msaa_samples_ = 0;

    /// @brief ドラッグ中のカメラ操作モード
    enum class DragMode {
        /// @brief ドラッグによるカメラ操作なし
        kNone,
        /// @brief 回転
        kRotate,
        /// @brief 移動 (パン)
        kPan,
        /// @brief ズーム
        kZoom,
    };

    /// @brief 現在のドラッグ操作モード (ボタン押下時に確定する)
    DragMode drag_mode_ = DragMode::kNone;
    /// @brief マウス操作のキー割り当て設定
    InputConfig input_config_;
    /// @brief 画面上における最後のマウス位置 (x, y)
    double last_x_ = 0.0, last_y_ = 0.0;
    /// @brief 左クリック押下時のマウス位置 (x, y)
    /// @note ドラッグ (回転) とクリック (ピッキング) の判別に使用する
    double press_x_ = 0.0, press_y_ = 0.0;

    /// @brief 範囲選択 (左ドラッグ) 中か
    /// @note 選択ボタン押下で立ち、解放で倒す。ラバーバンド描画と、
    ///       クリック/範囲選択の判別・カメラ操作の抑止に使用する
    bool is_box_selecting_ = false;

    /// @brief 選択中エンティティの交差座標 (表示用; キーはエンティティID)
    /// @note ピッキング時の3D交点座標を保持し、Controlsパネルに表示する
    std::unordered_map<ObjectID, Vector3d> selected_hit_positions_;

    /// @brief 再描画が必要か
    bool needs_redraw_ = true;



 public:
    /// @brief コンストラクタ
    /// @param width ウィンドウの幅の初期値 [px]
    /// @param height ウィンドウの高さの初期値 [px]
    /// @param msaa_samples マルチサンプリングのサンプル数 (0で無効)
    /// @throw std::runtime_error ウィンドウの初期化に失敗した場合
    explicit IgesViewerGUI(const int = kDefaultDisplayWidth,
                           const int = kDefaultDisplayHeight,
                           const int = 0);

    /// @brief デストラクタ
    ~IgesViewerGUI();



    /// @brief レンダラーを取得する (const版)
    /// @return レンダラーの参照
    const EntityRenderer& Renderer() const { return renderer_; }
    /// @brief レンダラーを取得する (非const版)
    /// @return レンダラーの参照
    EntityRenderer& Renderer() { return renderer_; }



    /// @brief マウス操作のキー割り当て設定を取得する
    /// @return 現在のキー割り当て設定
    const InputConfig& GetInputConfig() const { return input_config_; }
    /// @brief マウス操作のキー割り当て設定を注入する
    /// @param config 新しいキー割り当て設定
    void SetInputConfig(const InputConfig& config) { input_config_ = config; }



    /**
     * 描画関連の関数 (public)
     */

    /// @brief 実行
    /// @param vsync 垂直同期を有効にするか
    /// @note イベントループを開始し、描画を行う
    void Run(const bool = true);

    /// @brief 現在のEntityRendererの状態をキャプチャする
    /// @param filename 画像ファイルのパス
    /// @throw igesio::FileError 画像の保存に失敗した場合
    void CaptureScreenshot(const std::string&);



 protected:
    /**
     * 描画関連の関数 (protected)
     */

    /// @brief Controls GUIの描画
    virtual void RenderControls();



    /**
     * コールバック関数
     */

    /// @brief エラーハンドラ
    /// @param error エラーコード
    /// @param description エラーメッセージ
    static void ErrorCallback(const int error, const char* description) {
        std::cerr << "GLFW Error " << error << ": " << description << std::endl;
    }

    /// @brief コールバック関数を設定する
    /// @note ErrorCallback以外のコールバック関数を設定する
    virtual void SetupCallbacks();

    /// @brief マウスボタンのコールバック
    /// @param button マウスボタン
    /// @param action アクション (押下/解放)
    /// @param mods 修飾キー
    virtual void MouseButtonCallback(const int, const int, const int);

    /// @brief クリックによるエンティティ選択を処理する
    /// @param x クリックのスクリーンx座標 [px]（フレームバッファ座標系）
    /// @param y クリックのスクリーンy座標 [px]（フレームバッファ座標系）
    /// @param mods 修飾キー (GLFW_MOD_*)
    /// @note Ctrl押下でトグル、修飾なしでid単独へ置換、空クリックで全解除
    virtual void HandleClickSelection(const double, const double, const int);

    /// @brief 範囲選択 (矩形ドラッグ) を処理する
    /// @param x ドラッグ終了のスクリーンx座標 [px]（ウィンドウ座標系）
    /// @param y ドラッグ終了のスクリーンy座標 [px]（ウィンドウ座標系）
    /// @param mods 修飾キー (GLFW_MOD_*)
    /// @note 左→右ドラッグで内包、右→左で交差 (AutoCAD流)。
    ///       multi_select_mod押下で選択集合へ追加、なしで置換
    virtual void HandleBoxSelection(const double, const double, const int);

    /// @brief 範囲選択中のラバーバンド矩形を描画する
    /// @note ImGuiのforegroundドローリストに描画する。
    ///       内包=青系、交差=緑系で塗り分ける (AutoCAD流)
    virtual void RenderBoxSelectionOverlay();

    /// @brief マウスカーソル位置のコールバック
    /// @param xpos カーソルのX座標
    /// @param ypos カーソルのY座標
    virtual void CursorPositionCallback(const double, const double);

    /// @brief スクロールホイールのコールバック
    /// @param xoffset X方向のオフセット
    /// @param yoffset Y方向のオフセット
    virtual void ScrollCallback(const double, const double);

    /// @brief ウィンドウリサイズのコールバック
    /// @param width 新しい幅
    /// @param height 新しい高さ
    virtual void FramebufferSizeCallback(const int, const int);
};

}  // namespace igesio::graphics

#endif  // EXAMPLES_GUI_IGES_VIEWER_GUI_H_
