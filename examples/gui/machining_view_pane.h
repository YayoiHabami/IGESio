/**
 * @file examples/gui/machining_view_pane.h
 * @brief 簡易CAM GUIの1ビュー (レンダラ、カメラ、FBO、マウス操作)
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 * @note 機械ビューとワークビューを1つのウィンドウに並べるため、各ビューは
 *       独自のフレームバッファ (FBO) に描画し、その色テクスチャをImGuiの画像
 *       として表示する. `EntityRenderer::Draw()`はバインド中のフレームバッファ
 *       全体をクリアして描画するので、FBOにバインドしてから呼ぶ.
 * @note FBOの管理には独自の`GladGLContext`を用いる. 本ライブラリのgladはMX
 *       (マルチコンテキスト) 生成でグローバルな`glXxx`関数を持たず、レンダラの
 *       `IOpenGL`も`BlitFramebuffer`等を持たないため.
 * @note 機械はZ-upなので、標準ビューは本体の`Camera::SetStandardView` (Y-up)
 *       ではなく、本クラスがZ-upの視線と上方向を計算して設定する.
 */
#ifndef EXAMPLES_GUI_MACHINING_VIEW_PANE_H_
#define EXAMPLES_GUI_MACHINING_VIEW_PANE_H_

#include <memory>
#include <optional>
#include <string>

#include <glad/gl.h>
#include <imgui.h>

#include <igesio/graphics/core/gl_types.h>
#include <igesio/graphics/core/i_open_gl.h>
#include <igesio/graphics/renderer.h>
#include <igesio/models/scene.h>

#include "./input_config.h"

namespace igesio::graphics {

/// @brief Z-upの標準ビュー
enum class ZUpView {
    /// @brief 上面 (+Zから見下ろす. 上方向は+Y)
    kTop,
    /// @brief 底面 (-Zから見上げる. 上方向は+Y)
    kBottom,
    /// @brief 正面 (-Y側から+Y方向を見る. 上方向は+Z)
    kFront,
    /// @brief 背面 (+Y側から-Y方向を見る. 上方向は+Z)
    kBack,
    /// @brief 右面 (+X側から-X方向を見る. 上方向は+Z)
    kRight,
    /// @brief 左面 (-X側から+X方向を見る. 上方向は+Z)
    kLeft,
    /// @brief 等角 (+X, -Y, +Z側から見る. 上方向は+Z)
    kIso,
};

/// @brief 標準ビューの表示名
/// @param view 標準ビュー
/// @return 表示名 (`"Top"`等)
const char* ZUpViewName(ZUpView view);

/// @brief ビュー上のクリック
struct ClickInfo {
    /// @brief FBO左上原点のx座標 [px]
    double x = 0.0;
    /// @brief FBO左上原点のy座標 [px]
    double y = 0.0;
    /// @brief クリック時の修飾キー
    ModifierKey mods = ModifierKey::kNone;
};

/// @brief 1つのビュー (レンダラ、カメラ、FBO、マウス操作)
/// @note `Render`を毎フレーム呼ぶ. 表示サイズが変わったときだけFBOを作り直す.
///       マウス操作はカーソルがこのビューの画像の上にあるときだけ受け取る.
class ViewPane {
 public:
    /// @brief コンストラクタ
    /// @param name ビュー名 (オーバーレイの表示とImGuiのIDに用いる)
    /// @param gl レンダラに渡すGLバックエンド (ビュー間で共有してよい)
    /// @param loader `gladLoadGLContext`に渡すローダ (`glfwGetProcAddress`)
    /// @param msaa_samples マルチサンプリングのサンプル数 (0で無効)
    /// @throw std::runtime_error gladの関数ポインタのロードに失敗した場合
    /// @note GLコンテキストをカレントにしてから呼ぶこと
    ViewPane(std::string name, std::shared_ptr<IOpenGL> gl,
             GLProcLoader loader, int msaa_samples);
    /// @brief デストラクタ (FBOを破棄する)
    /// @note GLコンテキストが有効な間に呼ぶこと
    ~ViewPane();
    ViewPane(const ViewPane&) = delete;
    ViewPane& operator=(const ViewPane&) = delete;

    /// @brief 描画するシーンを設定する (`EntityRenderer::SetScene`)
    /// @param scene シーン (非所有. `nullptr`で解除)
    void SetScene(const models::Scene* scene) { renderer_.SetScene(scene); }
    /// @brief レンダラ
    EntityRenderer& Renderer() { return renderer_; }
    /// @brief レンダラ (const)
    const EntityRenderer& Renderer() const { return renderer_; }
    /// @brief ビュー名
    const std::string& Name() const { return name_; }

    /// @brief FBOに描画し、ImGuiの画像として表示し、マウス操作を処理する
    /// @param size 表示サイズ [px] (ImGui座標. 変われば FBO を作り直す)
    /// @param input マウス操作の割り当て
    /// @return ビュー上で選択のクリックがあれば、その位置と修飾キー
    /// @note ImGuiのウィンドウ内 (`Begin`〜`End`の間) で呼ぶこと.
    ///       画像の左上にオーバーレイ (Fit、Iso、標準ビュー、スクリーンショット)
    ///       を重ねる
    std::optional<ClickInfo> Render(const ImVec2& size, const InputConfig& input);

    /// @brief 直前の`Render`でカーソルが画像の上にあったか
    bool IsHovered() const { return hovered_; }
    /// @brief ドラッグ操作中か (連続再描画の判定に用いる)
    bool IsDragging() const { return drag_mode_ != DragMode::kNone; }
    /// @brief 次の`Render`の描画後に`FitView`を行うよう予約する
    /// @note 読込直後など、走査前で表示サイズも未確定な時点で呼ぶ
    void RequestFit() { fit_pending_ = true; }
    /// @brief オーバーレイのスクリーンショットボタンが押されたか (読み取りで解除)
    bool TakeScreenshotRequest();
    /// @brief 背景色のUI値 (RGB. レンダラは背景色のgetterを持たないため、GUIが保持する)
    /// @return 3要素の配列の先頭 (`ImGui::ColorEdit3`に渡す)
    float* BackgroundUi() { return background_ui_; }

    /// @brief 表示中の要素全体が収まるようにカメラを設定する
    /// @note 表示座標系と表示フィルタを反映した走査結果を用いる
    void FitView() { renderer_.FitView(); }
    /// @brief Z-upの標準ビューにカメラの向きを設定する
    /// @param view 標準ビュー
    /// @note ターゲットとターゲットまでの距離は保つ
    void SetStandardView(ZUpView view);
    /// @brief 現在の表示をPNGに保存する
    /// @param path 保存先
    /// @throw std::exception 保存に失敗した場合 (`SaveTextureToFile`から伝播)
    void CaptureScreenshot(const std::string& path);

 private:
    /// @brief ドラッグ操作の種類
    enum class DragMode {
        /// @brief 操作なし
        kNone,
        /// @brief カメラ回転
        kRotate,
        /// @brief 平行移動
        kPan,
        /// @brief ドラッグによるズーム
        kZoom,
        /// @brief 選択のクリック待ち (移動量が閾値を超えるまで)
        kSelect,
    };

    /// @brief 表示サイズに合わせてFBOを用意する (サイズが同じなら何もしない)
    /// @param width 幅 [px]
    /// @param height 高さ [px]
    void EnsureFramebuffer(int width, int height);
    /// @brief FBOとテクスチャを破棄する
    void DestroyFramebuffer();
    /// @brief FBOにバインドしてレンダラで描画し、MSAAなら表示用テクスチャに解決する
    void DrawToFramebuffer();
    /// @brief 画像の上のマウス操作を処理する
    /// @param origin 画像の左上のスクリーン座標
    /// @param size 画像の表示サイズ
    /// @param input マウス操作の割り当て
    /// @return 選択のクリックがあれば、その位置と修飾キー
    std::optional<ClickInfo> HandleMouse(const ImVec2& origin, const ImVec2& size,
                                         const InputConfig& input);
    /// @brief 押下されたボタンと修飾キーからドラッグ操作を決める
    /// @param button 押下されたボタン
    /// @param mods 修飾キー
    /// @param input マウス操作の割り当て
    DragMode ResolveDragMode(MouseButton button, ModifierKey mods,
                             const InputConfig& input) const;
    /// @brief 左上のオーバーレイ (ボタン列) を描画する
    /// @param origin 画像の左上のスクリーン座標
    void RenderOverlay(const ImVec2& origin);

    /// @brief ビュー名
    std::string name_;
    /// @brief FBO管理用のGL関数ポインタ
    GladGLContext gl_ = {};
    /// @brief レンダラ
    EntityRenderer renderer_;
    /// @brief マルチサンプリングのサンプル数 (0で無効)
    int msaa_samples_ = 0;
    /// @brief 表示用の色テクスチャ
    GLuint color_texture_ = 0;
    /// @brief 表示用テクスチャを持つFBO (MSAA無効時は描画先を兼ねる)
    GLuint resolve_fbo_ = 0;
    /// @brief 描画先のFBO (MSAA有効時のみ`resolve_fbo_`と別)
    GLuint draw_fbo_ = 0;
    /// @brief 描画先の色レンダーバッファ (MSAA有効時のみ)
    GLuint color_renderbuffer_ = 0;
    /// @brief 深度ステンシルのレンダーバッファ
    GLuint depth_renderbuffer_ = 0;
    /// @brief FBOの幅 [px]
    int fb_width_ = 0;
    /// @brief FBOの高さ [px]
    int fb_height_ = 0;
    /// @brief ImGui座標からFBOピクセルへの倍率 (HiDPI)
    float pixel_scale_ = 1.0f;
    /// @brief 直前の`Render`でカーソルが画像の上にあったか
    bool hovered_ = false;
    /// @brief 進行中のドラッグ操作
    DragMode drag_mode_ = DragMode::kNone;
    /// @brief ドラッグ中のボタン
    MouseButton drag_button_ = MouseButton::kNone;
    /// @brief 押下時の修飾キー
    ModifierKey press_mods_ = ModifierKey::kNone;
    /// @brief 押下位置 (スクリーン座標)
    ImVec2 press_pos_ = ImVec2(0.0f, 0.0f);
    /// @brief 次の描画後に`FitView`を行うか
    bool fit_pending_ = false;
    /// @brief スクリーンショットの要求 (オーバーレイのボタン)
    bool screenshot_requested_ = false;
    /// @brief オーバーレイの標準ビューのコンボの選択
    int overlay_view_index_ = static_cast<int>(ZUpView::kIso);
    /// @brief 背景色のUI値 (RGB. 初期値はレンダラのデフォルトの白)
    float background_ui_[3] = {1.0f, 1.0f, 1.0f};
};

}  // namespace igesio::graphics

#endif  // EXAMPLES_GUI_MACHINING_VIEW_PANE_H_
