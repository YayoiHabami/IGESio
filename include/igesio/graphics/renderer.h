/**
 * @file graphics/renderer.h
 * @brief IGESエンティティの描画を行うレンダラークラスを定義する
 * @author Yayoi Habami
 * @date 2025-08-06
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_RENDERER_H_
#define IGESIO_GRAPHICS_RENDERER_H_

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/models/selection_set.h"
#include "igesio/models/assembly.h"
#include "igesio/graphics/core/i_open_gl.h"
#include "igesio/graphics/core/camera.h"
#include "igesio/graphics/core/light.h"
#include "igesio/graphics/core/texture.h"
#include "igesio/graphics/core/i_entity_graphics.h"
#include "igesio/graphics/core/ray.h"
#include "igesio/graphics/factory.h"



namespace igesio::graphics {

/// @brief 描画対象の幅 [px] (デフォルト)
constexpr int kDefaultDisplayWidth = 1280;
/// @brief 描画対象の高さ [px] (デフォルト)
constexpr int kDefaultDisplayHeight = 720;

/// @brief 選択中エンティティのハイライト色 (RGBA) [0.0 - 1.0]
constexpr std::array<float, 4> kSelectionColor = {1.0f, 0.6f, 0.0f, 1.0f};

/// @brief ピッキング結果 (ObjectID付きのRayHit)
struct EntityHit {
    /// @brief ヒットしたエンティティのID
    ObjectID id;
    /// @brief 交差結果 (3D交点座標とray.originからの距離)
    RayHit hit;
};

/// @brief 範囲選択の判定種別
enum class BoxSelectionMode {
    /// @brief 内包: エンティティ全体が矩形内に見える場合に選択
    kContained,
    /// @brief 交差: 矩形がエンティティに少しでもかかって見える場合に選択
    kCrossing,
};

/// @brief 描画全般に関する (細かい) 設定
struct GraphicsSettings {
    /// @brief アンチエイリアシングを有効にするか
    bool enable_antialiasing = false;
    /// @brief 半透明のオブジェクトの描画を有効にするか
    /// @note 有効にした場合、ブレンドを行う
    bool enable_transparency = false;
};

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
            ObjectID, std::unique_ptr<IEntityGraphics>>> draw_objects_;

    /// @brief 描画対象のサイズ [px]
    /// @note 次回の描画時に反映される (すなわち、glGetIntegrevで取得できるサイズ
    ///       とは異なる場合は、次回のDraw呼び出し時に以下のサイズに更新される)
    int display_width_ = kDefaultDisplayWidth,
        display_height_ = kDefaultDisplayHeight;

    /// @brief 背景色
    std::array<float, 4> background_color_ = {1.0f, 1.0f, 1.0f, 1.0f};  // 白色

    /// @brief カメラクラス
    graphics::Camera camera_;

    /// @brief 光源クラス
    graphics::Light light_;

    /// @brief 描画に関するグローバルパラメータ (デフォルト)
    std::shared_ptr<const models::GraphicsGlobalParam> default_global_param_;

    /// @brief 描画全般に関する設定
    GraphicsSettings settings_;

    /// @brief 選択状態 (GUI非依存のモデル層オブジェクト)
    /// @note P2ではレンダラが所有する. 将来はセッションラッパー(Scene)が保持し、
    ///       レンダラはその参照を受け取る形へ移す予定 (本クラスは選択色を保持しない).
    models::SelectionSet selection_;

    /// @brief 走査対象のルートAssembly (非所有). nullptrの場合は従来のフラット描画
    /// @note 設定時は描画でツリーを走査し、各エンティティのワールド変換をリフレッシュ
    ///       しつつ可視/抑制サブツリーをスキップする
    const models::Assembly* scene_root_ = nullptr;
    /// @brief 描画リストの再構築 (ツリー走査) が必要か
    /// @note ツリー編集・エンティティ追加削除で立てる. カメラ操作・選択変更だけの
    ///       再描画ではキャッシュした描画リストを再利用し、走査しない
    mutable bool scene_dirty_ = true;
    /// @brief キャッシュした描画リスト (シェーダー別の描画オブジェクト. 非所有ポインタ)
    /// @note scene_root_設定時にツリー走査で構築し、フレームを越えて再利用する.
    ///       draw_objects_の要素を指すため、追加/削除時はdirty化して再構築する
    mutable std::unordered_map<ShaderType, std::vector<IEntityGraphics*>> draw_list_;

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
    /// @param material_property 描画プロパティ (IGESが保持しないデータ)
    /// @return エンティティの描画オブジェクトを作成できた場合はtrue
    /// @note すでに同じIDのエンティティが存在する場合は何もしない
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<
            entities::IEntityIdentifier, T>>>
    bool AddEntity(std::shared_ptr<const T> entity,
                   const std::shared_ptr<const models::GraphicsGlobalParam>
                   global_param = nullptr,
                   const MaterialProperty& material_property = MaterialProperty()) {
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

            // 描画プロパティを設定
            graphics->MaterialProperty() = material_property;
            graphics->SyncTexture();

            AddGraphicsObject(std::move(graphics));
            return true;
        }
        return false;
    }

    /// @brief エンティティの描画オブジェクトを作成する
    /// @param entity 描画するエンティティのポインタ (非const)
    /// @param global_param 描画に関するグローバルパラメータ
    /// @param material_property 描画プロパティ (IGESが保持しないデータ)
    /// @return エンティティの描画オブジェクトを作成できた場合はtrue
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<
            entities::IEntityIdentifier, T>>>
    bool AddEntity(std::shared_ptr<T> entity,
                   const std::shared_ptr<const models::GraphicsGlobalParam>
                   global_param = nullptr,
                   const MaterialProperty& material_property = MaterialProperty()) {
        if (!entity) return false;

        // constポインタに変換してAddEntityを呼び出す
        return AddEntity(std::const_pointer_cast<const T>(entity),
                         global_param, material_property);
    }

    /// @brief 指定されたIDのエンティティの描画オブジェクトを削除する
    /// @param id エンティティのID
    /// @note 存在しない場合は何もしない
    void RemoveEntity(const ObjectID&);

    /// @brief 指定されたIDのエンティティが登録済みか
    /// @param id エンティティのID
    /// @return 登録済みの場合は`true`, そうでない場合は`false`
    bool HasEntity(const ObjectID&) const;

    /// @brief 指定されたIDのエンティティの描画オブジェクトの型の取得
    /// @param id エンティティのID
    /// @return 描画オブジェクトを保持している場合はそのエンティティのShaderTypeを、
    ///         存在しない場合はShaderType::kNoneを返す
    ShaderType GetEntityShaderType(const ObjectID&) const;

    /// @brief 走査対象のルートAssemblyを設定する
    /// @param root ルートAssembly (非所有). nullptrで従来のフラット描画へ戻す
    /// @note 設定後の描画では、ツリーを走査して各エンティティのワールド変換を
    ///       リフレッシュし、可視/抑制サブツリーをスキップする. 呼び出しで再走査を予約する.
    ///       rootは本クラスより長く生存する必要がある (非所有参照).
    void SetSceneRoot(const models::Assembly* root);

    /// @brief シーン(ツリー)が変化したことを通知し、次回描画で再走査を予約する
    /// @note 可視性・抑制・大域変換・構造を編集した後に呼ぶ.
    ///       カメラ操作・選択変更だけの再描画では呼ぶ必要はない.
    void MarkSceneDirty();



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

    /// @brief 光源の参照を取得する (const)
    /// @return 光源の参照
    const Light& Light() const { return light_; }
    /// @brief 光源の参照を取得する (非const)
    /// @return 光源の参照
    /// @note 光源の各変数などはこの参照を通じて設定する
    graphics::Light& Light() { return light_; }

    /// @brief 描画全般に関する設定を取得する (const)
    const GraphicsSettings& Settings() const { return settings_; }
    /// @brief 描画全般に関する設定を上書きする
    /// @param settings 新しい設定
    /// @note 設定を変更するだけであり、反映にはDraw()の呼び出しが必要
    void SetSettings(const GraphicsSettings&);

    /// @brief アンチチエイリアスの有効/無効を設定する
    /// @param enable trueの場合は有効、falseの場合は無効
    /// @note 設定を変更するだけであり、反映にはDraw()の呼び出しが必要
    void EnableAntialiasing(const bool);
    /// @brief アンチエイリアスが有効か
    /// @return 有効な場合はtrue、無効な場合はfalse
    bool IsAntialiasingEnabled() const;

    /// @brief 半透明のオブジェクトの描画の有効/無効を設定する
    /// @param enable trueの場合は有効、falseの場合は無効
    /// @note 設定を変更するだけであり、反映にはDraw()の呼び出しが必要
    void EnableTransparency(const bool);
    /// @brief 半透明のオブジェクトの描画が有効か
    /// @return 有効な場合はtrue、無効な場合はfalse
    bool IsTransparencyEnabled() const;



    /**
     * 描画
     */

    /// @brief エンティティを描画する
    void Draw() const;

    /// @brief 現在の描画状態をキャプチャする
    Texture CaptureScreenshot() const;



    /**
     * ピッキング・選択
     */

    /// @brief スクリーン座標からワールド空間のレイを生成する
    /// @param screen_x スクリーンx座標 [px]（左上原点・GLFW準拠）
    /// @param screen_y スクリーンy座標 [px]（左上原点・GLFW準拠）
    /// @return ワールド空間のレイ
    /// @note kOblique投影モードでは正しいレイが生成されない (TODO: 未対応)
    Ray GetRayFromScreen(double screen_x, double screen_y) const;

    /// @brief 登録済みエンティティとレイの交差判定を行う
    /// @param ray ワールド空間のレイ
    /// @param screen_x 曲線ヒットのピクセル換算に使う基準スクリーンx座標 [px]
    /// @param screen_y 曲線ヒットのピクセル換算に使う基準スクリーンy座標 [px]
    /// @param params 探索制御パラメータ
    /// @return distance昇順のヒットリスト（重複除去済み）
    /// @note 曲線ヒット許容量はエンティティ毎の代表深度でピクセル換算される
    std::vector<EntityHit> PickEntities(
            const Ray&, double screen_x, double screen_y,
            const RayIntersectionParams& = {}) const;

    /// @brief スクリーン矩形領域内のエンティティを取得する
    /// @param rect スクリーン矩形 [px]（左上原点・GLFW準拠）
    /// @param mode 判定種別（内包／交差）
    /// @param params サンプリング制御パラメータ
    /// @return 条件を満たすエンティティIDのリスト（順不同）
    /// @note 粗カリングはBBのスクリーンAABBがrectと重なるかのみで行う（保守的）
    /// @note kOblique投影モードでは正しい結果を返さない (TODO: 未対応)
    std::vector<ObjectID> PickEntitiesInRect(
            const ScreenRect&, BoxSelectionMode,
            const SelectionSampleParams& = {}) const;

    /// @brief 指定IDのエンティティを選択する
    /// @param id エンティティのID
    /// @note 既に選択中、または未登録の場合は何もしない
    void Select(const ObjectID&);

    /// @brief 指定IDのエンティティの選択を解除する
    /// @param id エンティティのID
    /// @note 選択されていない場合は何もしない
    void Deselect(const ObjectID&);

    /// @brief 指定IDのエンティティの選択状態をトグルする
    /// @param id エンティティのID
    void ToggleSelection(const ObjectID&);

    /// @brief すべての選択を解除する
    void ClearSelection();

    /// @brief 指定IDのエンティティが選択中か
    /// @param id エンティティのID
    /// @return 選択中の場合はtrue
    bool IsSelected(const ObjectID&) const;

    /// @brief 選択中のエンティティID群を取得する
    /// @return 選択中のエンティティIDのリスト (順不同)
    /// @note 選択状態は`SelectionSet`(集合)が保持するため、その時点のスナップショットを
    ///       値で返す. 呼び出し側が`const auto&`で受けても寿命延長で安全に走査できる.
    std::vector<ObjectID> GetSelectedIds() const;



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

    /// @brief 指定IDの描画オブジェクトを取得する
    /// @param id エンティティのID
    /// @return 描画オブジェクトのポインタ. 存在しない場合はnullptr
    /// @note 所有権は移譲しない (draw_objects_が保持し続ける)
    IEntityGraphics* FindGraphics(const ObjectID&) const;

    /// @brief 全ての子要素のDrawメンバを呼び出す
    /// @param program_id シェーダープログラムのID
    /// @param shader_type シェーダータイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    void DrawChildren(GLuint, const ShaderType, const std::pair<float, float>&,
                      const DrawContext&) const;

    /// @brief scene_root_を走査して描画リストを再構築し、ワールド変換をリフレッシュする
    /// @note dirty時のみ呼ぶ. ShouldRenderEntityは呼ばず、キャッシュ在席を描画対象条件
    ///       とする (フィルタは投入時に適用済み).
    void RebuildDrawList() const;

    /// @brief Assemblyツリーを再帰走査する (RebuildDrawListの実体)
    /// @param node 走査中のノード
    /// @param parent_accum 親までの累積変換 (G_root·…·G_{n-1})
    /// @note 可視/抑制サブツリーはスキップ. node大域変換を掛けた累積をworld_transform_へ
    ///       流し (M_entityは含めない)、graphicsをシェーダー別にdraw_list_へ収集する.
    void WalkAssembly(const models::Assembly& node,
                      const igesio::Matrix4d& parent_accum) const;

    /// @brief キャッシュした描画リストをシェーダー単位で描画する
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    void ExecuteDrawList(const DrawContext& ctx) const;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_RENDERER_H_
