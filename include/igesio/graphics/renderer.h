/**
 * @file graphics/renderer.h
 * @brief IGESエンティティの描画を行うレンダラークラスを定義する
 * @author Yayoi Habami
 * @date 2025-08-06
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_RENDERER_H_
#define IGESIO_GRAPHICS_RENDERER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/graphics/core/i_open_gl.h"
#include "igesio/graphics/core/camera.h"
#include "igesio/graphics/core/i_entity_graphics.h"
#include "igesio/graphics/factory.h"



namespace igesio::graphics {

/// @brief 描画対象の幅 [px] (デフォルト)
constexpr int kDefaultDisplayWidth = 1280;
/// @brief 描画対象の高さ [px] (デフォルト)
constexpr int kDefaultDisplayHeight = 720;

/// @brief エンティティの描画を担当するクラス
/// @note OpenGLを使用してエンティティの曲線を描画する
class EntityRenderer {
    /// @brief OpenGLラッパー
    std::shared_ptr<IOpenGL> gl_;

    /// @brief シェーダープログラム
    /// @note キーはShaderType、値はシェーダープログラムのID
    /// @note コンパイルとリンクが成功した、各ShaderTypeに対応する
    ///       シェーダープログラムを保持する
    std::unordered_map<ShaderType, GLuint> shader_programs_;

    /// @brief 描画オブジェクト
    /// @note 1階層目のキーはShaderType、2階層目のキーはエンティティのID
    ///       値はIEntityGraphicsオブジェクト
    /// @note 基本的に描画オブジェクトは外側には公開しない
    std::unordered_map<ShaderType, std::unordered_map<
            uint64_t, std::unique_ptr<IEntityGraphics>>> draw_objects_;

    /// @brief 描画対象のサイズ [px]
    /// @note 次回の描画時に反映される (すなわち、glGetIntegrevで取得できるサイズ
    ///       とは異なる場合は、次回のDraw呼び出し時に以下のサイズに更新される)
    int display_width_ = kDefaultDisplayWidth,
        display_height_ = kDefaultDisplayHeight;

    /// @brief 背景色
    std::array<float, 4> background_color_ = {1.0f, 1.0f, 1.0f, 1.0f};  // 白色

    /// @brief カメラクラス
    graphics::Camera camera_;

    /// @brief 描画に関するグローバルパラメータ (デフォルト)
    std::shared_ptr<const models::GraphicsGlobalParam> default_global_param_;

 public:
    /// @brief コンストラクタ
    /// @param gl OpenGLラッパー
    /// @param width ウィンドウの幅の初期値 [px]
    /// @param height ウィンドウの高さの初期値 [px]
    /// @note オブジェクトを作成した時点では描画可能な状態ではない.
    ///       描画を行う前にInitialize()を呼び出す必要がある
    explicit EntityRenderer(std::shared_ptr<IOpenGL>,
                            const int = kDefaultDisplayWidth,
                            const int = kDefaultDisplayHeight);

    /// @brief デストラクタ
    ~EntityRenderer() { Cleanup(); }

    // コピーコンストラクタとコピー代入演算子を禁止
    EntityRenderer(const EntityRenderer&) = delete;
    EntityRenderer& operator=(const EntityRenderer&) = delete;

    /// @brief 初期化されているか
    bool IsInitialized() const;

    /// @brief 初期化する
    /// @note シェーダープログラムのコンパイルとリンクなどを行う
    /// @throw igesio::ImplementationError シェーダーの初期化に失敗した場合
    void Initialize();

    /// @brief OpenGLリソースを解放する
    /// @note 子要素も含めた、全てのOpenGLリソースを解放する
    void Cleanup();



    /**
     * エンティティの取得/設定
     */

    /// @brief エンティティの描画オブジェクトを作成する
    /// @param entity 描画するエンティティのポインタ (const)
    /// @param global_param 描画に関するグローバルパラメータ
    /// @return エンティティの描画オブジェクトを作成できた場合はtrue
    /// @note すでに同じIDのエンティティが存在する場合は何もしない
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<
            entities::IEntityIdentifier, T>>>
    bool AddEntity(std::shared_ptr<const T> entity,
                   const std::shared_ptr<const models::GraphicsGlobalParam>
                   global_param = nullptr) {
        if (!entity) return false;

        if (HasEntity(entity->GetID())) return false;

        // 新しいエンティティを追加
        auto ptr = std::static_pointer_cast<const entities::IEntityIdentifier>(entity);
        if (auto graphics = CreateEntityGraphics(ptr, gl_)) {
            // グローバルパラメータを設定
            if (global_param) {
                graphics->SetGlobalParam(global_param);
            } else {
                graphics->SetGlobalParam(default_global_param_);
            }

            AddGraphicsObject(std::move(graphics));
            return true;
        }
        return false;
    }

    /// @brief エンティティの描画オブジェクトを作成する
    /// @param entity 描画するエンティティのポインタ (非const)
    /// @param global_param 描画に関するグローバルパラメータ
    /// @return エンティティの描画オブジェクトを作成できた場合はtrue
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<
            entities::IEntityIdentifier, T>>>
    bool AddEntity(std::shared_ptr<T> entity,
                   const std::shared_ptr<const models::GraphicsGlobalParam>
                   global_param = nullptr) {
        if (!entity) return false;

        // constポインタに変換してAddEntityを呼び出す
        return AddEntity(std::const_pointer_cast<const T>(entity), global_param);
    }

    /// @brief 指定されたIDのエンティティの描画オブジェクトを削除する
    /// @param id エンティティのID
    /// @note 存在しない場合は何もしない
    void RemoveEntity(const uint64_t);

    /// @brief 指定されたIDのエンティティが登録済みか
    /// @param id エンティティのID
    /// @return 登録済みの場合は`true`, そうでない場合は`false`
    bool HasEntity(const uint64_t) const;

    /// @brief 指定されたIDのエンティティの描画オブジェクトの型の取得
    /// @param id エンティティのID
    /// @return 描画オブジェクトを保持している場合はそのエンティティのShaderTypeを、
    ///         存在しない場合はShaderType::kNoneを返す
    ShaderType GetEntityShaderType(const uint64_t) const;



    /**
     * エンティティ以外の要素の取得/設定
     */

    /// @brief 描画対象のサイズを取得する
    /// @return 描画対象のサイズ (幅, 高さ) [px]
    /// @note この値とOpenGLのviewportのサイズは異なる場合がある.
    ///       その場合、次回描画時 (Draw呼び出し時) にOpenGLのviewportの
    ///       サイズがここで設定されたサイズに更新される
    std::pair<int, int> GetDisplaySize() const;

    /// @brief 描画対象のサイズを設定する
    /// @param width 幅 [px]
    /// @param height 高さ [px]
    /// @note この関数でリサイズしても再描画はされないため、
    ///       明示的に`Draw()`を呼び出す必要がある
    void SetDisplaySize(const int, const int);

    /// @brief 背景色を取得する
    /// @return 背景色 (RGBA) [0.0 - 1.0]
    std::array<float, 4> GetBackgroundColor() const;

    /// @brief 背景色の参照を取得する
    /// @return 背景色の参照 (RGBA) [0.0 - 1.0]
    std::array<float, 4>& GetBackgroundColorRef();

    /// @brief 背景色を設定する
    /// @param red 赤成分 [0.0 - 1.0]
    /// @param green 緑成分 [0.0 - 1.0]
    /// @param blue 青成分 [0.0 - 1.0]
    /// @param alpha 不透明度 [0.0 - 1.0] (デフォルト: 1.0f)
    void SetBackgroundColor(const float, const float,
                            const float, const float = 1.0f);

    /// @brief カメラの参照を取得する (const)
    /// @return カメラの参照
    const Camera& Camera() const { return camera_; }
    /// @brief カメラの参照を取得する (非const)
    /// @return カメラの参照
    /// @note カメラの各変数などはこの参照を通じて設定する
    graphics::Camera& Camera() { return camera_; }



    /**
     * 描画
     */

    /// @brief エンティティを描画する
    void Draw() const;

    /// @brief 現在の描画状態をキャプチャする
    /// @return 各ピクセルのRGB値を格納したバッファ
    ///         サイズは`width * height * 3` (R, G, B).
    /// @note 画像の幅と高さはGetDisplaySize()で取得できる.
    ///       戻り値は`glReadPixels`で取得した値と同じ形式で格納され、
    ///       左下が原点で、右方向にx座標が、上方向にy座標が増加する形式に
    ///       なっている. 例えば、(x, y)のピクセルのR成分は
    ///       `pixels[(y * width + x) * 3 + 0]`で取得できる.
    /// @note stb_image_writeなどを使用してPNGなどの画像ファイルとして
    ///       保存することも可能. `examples/gui/iges_viewer_gui.cpp`の
    ///       `IgesViewerGUI::CaptureScreenshot`関数を参照のこと
    std::vector<unsigned char> CaptureScreenshot() const;



 protected:
    /// @brief 現在のviewportを取得する
    /// @return viewportの4要素 (x, y, width, height)
    /// @note この値とdisplay_width_, display_height_ (すなわち`GetDisplaySize()`
    ///       で取得される値) は異なる場合がある.
    ///       その場合、次回描画時にdisplay_width_, display_height_に更新される
    std::array<int, 4> GetCurrentViewport() const;


 private:
    /// @brief シェーダープログラムの初期化
    /// @note シェーダーのコンパイルとリンクを行う
    /// @throw igesio::ImplementationError シェーダーの初期化に失敗した場合
    void InitShaders();

    /// @brief 頂点シェーダーをコンパイルする
    /// @param vertex_source 頂点シェーダーのソースコード
    /// @return コンパイルされたシェーダーのID
    /// @throw igesio::ImplementationError コンパイルに失敗した場合、
    ///         vertex_sourceが空の場合
    GLuint CompileVertexShader(const std::string&);

    /// @brief ジオメトリシェーダーをコンパイルする
    /// @param geometry_source ジオメトリシェーダーのソースコード
    /// @return コンパイルされたシェーダーのID、
    ///         geometry_sourceが空の場合は0を返す
    /// @throw igesio::ImplementationError コンパイルに失敗した場合
    GLuint CompileGeometryShader(const std::string&);

    /// @brief TCS & TESシェーダーをコンパイルする
    /// @param tcs_source TCSシェーダーのソースコード
    /// @param tes_source TESシェーダーのソースコード
    /// @return std::pair<TCSシェーダーのID, TESシェーダーのID>、
    ///         TCSまたはTESが空の場合は両方とも0を返す
    /// @throw igesio::ImplementationError コンパイルに失敗した場合
    std::pair<float, float> CompileTCSAndTES(const std::string&,
                                             const std::string&);

    /// @brief フラグメントシェーダーをコンパイルする
    /// @param fragment_source フラグメントシェーダーのソースコード
    /// @return コンパイルされたシェーダーのID
    /// @throw igesio::ImplementationError コンパイルに失敗した場合、
    ///         fragment_sourceが空の場合
    GLuint CompileFragmentShader(const std::string&);

    /// @brief シェーダープログラム作成する
    /// @param vertex_shader 頂点シェーダーのID
    /// @param fragment_shader フラグメントシェーダーのID
    /// @param geometry_shader ジオメトリシェーダーのID
    /// @param tcs_shader TCSシェーダーのID
    /// @param tes_shader TESシェーダーのID
    /// @return 作成されたシェーダープログラムのID
    /// @throw igesio::ImplementationError リンクに失敗した場合、
    ///        頂点シェーダーまたはフラグメントシェーダーが提供されていない場合
    /// @note IDに0を指定した場合は、そのシェーダーはリンクされない
    GLuint CreateShaderProgram(GLuint, GLuint,
                               GLuint = 0, GLuint = 0, GLuint = 0);

    /// @brief エンティティが設定されているか
    bool IsEmpty() const;

    /// @brief 描画オブジェクトを追加する
    /// @param graphics 描画オブジェクト
    /// @note すでに同じIDのエンティティが存在する場合は何もしない
    void AddGraphicsObject(std::unique_ptr<IEntityGraphics>&&);

    /// @brief 指定されたシェーダータイプに対応する描画オブジェクトを持つか
    /// @param shader_type シェーダータイプ
    /// @return kCompositeの子要素も含めて、指定されたシェーダータイプに対応する
    ///         描画オブジェクトを持つ場合は`true`, そうでない場合は`false`.
    bool HasGraphicsObject(const ShaderType shader_type) const;

    /// @brief 全ての子要素のDrawメンバを呼び出す
    /// @param program_id シェーダープログラムのID
    /// @param shader_type シェーダータイプ
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawChildren(GLuint, const ShaderType, const std::pair<float, float>&) const;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_RENDERER_H_
