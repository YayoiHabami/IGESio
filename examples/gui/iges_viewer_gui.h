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

#include "igesio/graphics/core/open_gl.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <igesio/graphics/renderer.h>



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

    /// @brief ドラッグ中か
    bool is_dragging_ = false;
    /// @brief パン (平行移動) 中か
    bool is_panning_ = false;
    /// @brief 画面上における最後のマウス位置 (x, y)
    double last_x_ = 0.0, last_y_ = 0.0;

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
