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
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/models/scene.h"
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

/// @brief 表示モード (面と面エッジの描画組み合わせ)
/// @note 従属しない曲線エンティティはいずれのモードでも常に描画される.
enum class DisplayMode {
    /// @brief 面と面エッジの両方を描画する
    kShaded,
    /// @brief 面エッジのみを描画する (面の塗りつぶしを描画しない)
    kWireFrame,
    /// @brief 面のみを描画する (面エッジを描画しない)
    kNoEdge
};

/// @brief 描画全般に関する (細かい) 設定
struct GraphicsSettings {
    /// @brief アンチエイリアシングを有効にするか
    bool enable_antialiasing = false;
    /// @brief 半透明のオブジェクトの描画を有効にするか
    /// @note 有効にした場合、ブレンドを行う
    bool enable_transparency = false;
    /// @brief 表示モード (面/面エッジの描画組み合わせ)
    DisplayMode display_mode = DisplayMode::kShaded;
};

/// @brief 表示フィルタ (レンダラ単位のビュー状態)
/// @note 除外された型は描画リスト/可視リストに収集されず、描画もピックもされない.
///       キャッシュ済み描画オブジェクトは温存される (再表示が安価).
///       Scene (セッション状態) ではなくレンダラ毎に保持し、複数ビューで
///       異なる種別表示を可能にする
struct DisplayFilter {
    /// @brief 非表示にするエンティティ型の集合
    std::unordered_set<entities::EntityType> hidden_types;

    /// @brief 指定型を描画対象とするか
    /// @param type エンティティ型
    /// @return 描画対象とする場合はtrue
    bool ShouldRender(const entities::EntityType type) const {
        return hidden_types.count(type) == 0;
    }
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
    std::unordered_map<ShaderType, gl::Uint> shader_programs_;

    /// @brief 描画オブジェクトのキャッシュ (Sceneツリーの派生キャッシュ)
    /// @note EnsureSyncedの遅延生成で作られ、ツリーから削除されたエントリは
    ///       Sweepで破棄される. 値がnullptrのエントリは「生成失敗 (型起因)」の
    ///       負キャッシュで、再試行を抑止する (factory.hの不変条件を参照).
    ///       描画オブジェクトは外側には公開しない
    mutable std::unordered_map<ObjectID, std::unique_ptr<IEntityGraphics>>
            graphics_cache_;

    /// @brief 描画対象のサイズ [px]
    /// @note 次回の描画時に反映される (すなわち、glGetIntegrevで取得できるサイズ
    ///       とは異なる場合は、次回のDraw呼び出し時に以下のサイズに更新される)
    int display_width_ = kDefaultDisplayWidth,
        display_height_ = kDefaultDisplayHeight;

    /// @brief 背景色
    std::array<float, 4> background_color_ = {1.0f, 1.0f, 1.0f, 1.0f};  // 白色

    /// @brief 環境光 (アンビエント) の色 (RGB) [0.0 - 1.0]
    /// @note IBLの代替として、面シェーダーが一定の環境光として反射する.
    ///       金属は誘電体と異なり拡散を持たないため、この値が暗部の明るさを決める
    std::array<float, 3> ambient_color_ = {0.1f, 0.1f, 0.1f};

    /// @brief カメラクラス
    /// @note mutable: 自動クリップ球はシーンの派生キャッシュであり、
    ///       constな同期経路 (EnsureSynced/ResyncGeometries) から更新される
    mutable graphics::Camera camera_;

    /// @brief 光源リスト
    /// @note 既定では方向光を1個保持する. 描画時はkMaxLightsまで送信される.
    std::vector<graphics::Light> lights_ = { graphics::Light{} };

    /// @brief 描画に関するグローバルパラメータ (デフォルト)
    std::shared_ptr<const models::GraphicsGlobalParam> default_global_param_;

    /// @brief 描画全般に関する設定
    GraphicsSettings settings_;

    /// @brief 描画対象のシーン (非所有).
    ///        セッション状態(root + 選択 + フィルタ)を一元管理するシーン
    /// @note nullptrの間は描画しない. 設定時は描画でツリーを走査し、各エンティティの
    ///       ワールド変換と色/不透明度オーバーライドをリフレッシュしつつ、可視/抑制
    ///       サブツリーをスキップする. sceneは本クラスより長く生存する必要がある.
    const models::Scene* scene_ = nullptr;
    /// @brief 最後に同期したSceneのモデルリビジョン
    /// @note リビジョン値は同一rootに対してのみ比較可能 (synced_root_とペアで
    ///       初めて意味を持つ)
    mutable uint64_t synced_revision_ = 0;
    /// @brief 最後に同期したルートAssemblyの同一性 (ObjectID)
    /// @note root差し替え時の等値衝突 (別rootが偶然同じリビジョン値を持つ) を
    ///       構造的に防ぐ. EnsureSyncedはリビジョンと同一性の両方を比較する
    mutable ObjectID synced_root_;
    /// @brief レンダラ内部要因による再同期要求
    /// @note SetScene/SetDisplayFilter等、ツリー側のリビジョンに現れない
    ///       レンダラ自身の状態変更で立てる
    mutable bool local_dirty_ = true;
    /// @brief キャッシュした描画リスト (シェーダー別の描画オブジェクト. 非所有ポインタ)
    /// @note EnsureSyncedのツリー走査で構築し、フレームを越えて再利用する.
    ///       graphics_cache_の要素を指す
    mutable std::unordered_map<ShaderType, std::vector<IEntityGraphics*>> draw_list_;
    /// @brief 可視エンティティの平坦リスト (ピック用. エンティティ毎に一意)
    /// @note draw_list_は複合エンティティを複数シェーダーへ重複登録するため、
    ///       ピックはこちらを走査して二重判定を避ける. 削除済み・非表示・抑制中・
    ///       フィルタ除外のエンティティは構造的に含まれない
    mutable std::vector<std::pair<ObjectID, IEntityGraphics*>> visible_list_;
    /// @brief 表示フィルタ (ビュー状態)
    DisplayFilter display_filter_;
    /// @brief エンティティ毎の描画プロパティのオーバーライド
    /// @note 生成時に適用するほか、設定変更はEnsureSyncedの走査で遅延適用する
    ///       (setterはGLを触らない). Sweepで所有者を失ったエントリは破棄する
    mutable std::unordered_map<ObjectID, graphics::MaterialProperty>
            material_overrides_;
    /// @brief マテリアル適用待ちのID集合
    /// @note SetMaterialProperty/ClearMaterialPropertyで追加され、走査で適用
    ///       (SyncTexture含む) して除去される遅延キュー. SyncTexture (GL操作) を
    ///       setter内で行わないための機構
    mutable std::unordered_set<ObjectID> pending_material_ids_;

 public:
    /// @brief コンストラクタ
    /// @param gl OpenGLラッパー (nullptr可; 後で`SetGLBackend`で設定できる)
    /// @param width ウィンドウの幅の初期値 [px]
    /// @param height ウィンドウの高さの初期値 [px]
    /// @note オブジェクトを作成した時点では描画可能な状態ではない.
    ///       描画を行う前にGLバックエンドを設定し、Initialize()を呼び出す必要がある.
    /// @note glにnullptrを渡すと、GLコンテキストが無い段階でも値メンバとして構築できる.
    ///       この場合、コンテキストをカレント化した後に`SetGLBackend`を呼ぶこと.
    explicit EntityRenderer(std::shared_ptr<IOpenGL> = nullptr,
                            const int = kDefaultDisplayWidth,
                            const int = kDefaultDisplayHeight);

    /// @brief デストラクタ
    ~EntityRenderer() { Cleanup(); }

    // コピーコンストラクタとコピー代入演算子を禁止
    EntityRenderer(const EntityRenderer&) = delete;
    EntityRenderer& operator=(const EntityRenderer&) = delete;

    /// @brief GLバックエンド (OpenGLラッパー) を設定する
    /// @param gl OpenGLラッパー
    /// @note コンストラクタでglを渡さなかった場合に、GLコンテキストをカレント化した後で
    ///       呼び出す. `Initialize()`より前に設定する必要がある.
    void SetGLBackend(std::shared_ptr<IOpenGL> gl);

    /// @brief 初期化されているか
    /// @return GLバックエンドが設定済みかつシェーダーがコンパイル済みの場合はtrue
    bool IsInitialized() const;

    /// @brief 初期化する
    /// @note シェーダープログラムのコンパイルとリンクなどを行う
    /// @throw igesio::ImplementationError シェーダーの初期化に失敗した場合、
    ///        またはGLバックエンドが未設定の場合
    void Initialize();

    /// @brief OpenGLリソースを解放する
    /// @note 子要素も含めた、全てのOpenGLリソースを解放する
    void Cleanup();



    /**
     * シーン・表示状態の設定
     */

    /// @brief 描画対象のシーンを設定する
    /// @param scene シーン (非所有). nullptrで描画を停止する
    /// @note 描画オブジェクトはscene->Root()のツリーとの突き合わせ (Reconcile) で
    ///       遅延生成・破棄される. ツリーの編集はAssembly側のモデルリビジョンで
    ///       自動検知されるため、編集後の手動通知は不要. 選択ハイライトは
    ///       scene->ActiveSelection()からPULLする. sceneは本クラスより長く生存すること.
    void SetScene(const models::Scene* scene);

    /// @brief 表示フィルタを設定する
    /// @param filter 表示フィルタ (非表示にするエンティティ型の集合)
    /// @note 除外された型の描画オブジェクトは温存され、解除時に再利用される
    void SetDisplayFilter(const DisplayFilter& filter) {
        display_filter_ = filter;
        local_dirty_ = true;
    }
    /// @brief 表示フィルタを取得する
    /// @return 現在の表示フィルタ
    const DisplayFilter& GetDisplayFilter() const { return display_filter_; }

    /// @brief エンティティの描画プロパティのオーバーライドを設定する
    /// @param id エンティティのID
    /// @param material 描画プロパティ (IGESが保持しないデータ)
    /// @note 適用は次回の描画/ピック時に遅延して行われる (本メソッドはGL
    ///       コンテキスト前提を持たない). ツリーから削除されたIDの設定は
    ///       Sweepで自動破棄される
    void SetMaterialProperty(const ObjectID& id,
                             const graphics::MaterialProperty& material) {
        material_overrides_[id] = material;
        pending_material_ids_.insert(id);
        local_dirty_ = true;
    }
    /// @brief エンティティの描画プロパティのオーバーライドを解除する
    /// @param id エンティティのID
    /// @note 次回の描画/ピック時にデフォルトの描画プロパティへ戻す
    void ClearMaterialProperty(const ObjectID& id) {
        material_overrides_.erase(id);
        pending_material_ids_.insert(id);
        local_dirty_ = true;
    }



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

    /// @brief 環境光 (アンビエント) の色を取得する
    /// @return 環境光の色 (RGB) [0.0 - 1.0]
    std::array<float, 3> GetAmbientColor() const;

    /// @brief 環境光 (アンビエント) の色を設定する
    /// @param red 赤成分 [0.0 - 1.0]
    /// @param green 緑成分 [0.0 - 1.0]
    /// @param blue 青成分 [0.0 - 1.0]
    /// @note IBLの代替として面シェーダーが反射する一定の環境光. 値を上げると
    ///       暗部 (特に金属) が明るくなる. 反映には`Draw()`の呼び出しが必要
    void SetAmbientColor(const float, const float, const float);

    /// @brief カメラの参照を取得する (const)
    /// @return カメラの参照
    const Camera& Camera() const { return camera_; }
    /// @brief カメラの参照を取得する (非const)
    /// @return カメラの参照
    /// @note カメラの各変数などはこの参照を通じて設定する
    graphics::Camera& Camera() { return camera_; }

    /// @brief シーン全体が画面に収まるようにカメラを調整する (FitView)
    /// @note scene->Root()のワールドバウンディングボックスと現在の表示サイズから
    ///       アスペクト比を求め、Camera::FitToBoundingBoxを呼ぶ. シーン未設定・
    ///       バウンディングボックスが空・表示サイズが無効な場合は何もしない.
    /// @note 反映には別途Draw()の呼び出しが必要.
    void FitView();

    /// @brief 光源リストの参照を取得する (const)
    /// @return 光源リストの参照
    const std::vector<Light>& Lights() const { return lights_; }
    /// @brief 光源リストの参照を取得する (非const)
    /// @return 光源リストの参照
    /// @note 光源の追加・削除や各光源の設定はこの参照を通じて行う.
    ///       要素数がkMaxLightsを超えた分は描画時に無視される.
    std::vector<Light>& Lights() { return lights_; }

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

    /// @brief 表示モードを設定する
    /// @param mode 表示モード (shaded / wireframe / no-edge)
    /// @note 設定を変更するだけであり、反映にはDraw()の呼び出しが必要
    void SetDisplayMode(const DisplayMode);
    /// @brief 表示モードを取得する
    /// @return 現在の表示モード
    DisplayMode GetDisplayMode() const;



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
    gl::Uint CompileVertexShader(const std::string&);

    /// @brief ジオメトリシェーダーをコンパイルする
    /// @param geometry_source ジオメトリシェーダーのソースコード
    /// @return コンパイルされたシェーダーのID、
    ///         geometry_sourceが空の場合は0を返す
    /// @throw igesio::ImplementationError コンパイルに失敗した場合
    gl::Uint CompileGeometryShader(const std::string&);

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
    gl::Uint CompileFragmentShader(const std::string&);

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
    gl::Uint CreateShaderProgram(gl::Uint, gl::Uint,
                                 gl::Uint = 0, gl::Uint = 0, gl::Uint = 0);

    /// @brief 描画キャッシュ・描画/可視リストをSceneツリーと突き合わせる (Reconcile)
    /// @note Draw/PickEntities/PickEntitiesInRectの冒頭で呼ぶ. モデルリビジョン
    ///       (synced_root_とのペア比較) とlocal_dirty_が一致する間は何もしない.
    ///       不一致時はSweep→ツリー走査 (遅延生成+リスト再構築)→自動クリップ球更新を
    ///       行う. 描画オブジェクトの生成/Cleanup()を行うため、GLコンテキストが
    ///       カレントなスレッドから呼ぶこと. scene_またはgl_が未設定なら何もしない
    void EnsureSynced() const;

    /// @brief ツリーから削除されたエンティティの描画キャッシュを破棄する (Sweep)
    /// @param root 所属判定に用いるルートAssembly
    /// @note FindOwner(id)==nullptrのエントリをCleanup()して除去する (O(1)/件).
    ///       material_overrides_/pending_material_ids_も同条件で掃除する
    void SweepStaleGraphics(const models::Assembly& root) const;

    /// @brief 可視エンティティのジオメトリ再同期を遅延実行する
    /// @note Drawの冒頭 (EnsureSynced後) で呼ぶ. visible_list_の各要素について
    ///       同期キーの不一致 (NeedsResync) を検査し、Synchronize()を再実行する.
    ///       形状変更はBBoxも変えるため、再同期があれば自動クリップ球も更新する.
    ///       ピッキングはエンティティを解析的に読むため再同期不要
    void ResyncGeometries() const;

    /// @brief 指定IDの描画オブジェクトを取得し、未在席なら遅延生成する
    /// @param id エンティティのID
    /// @param entity 生成に用いるエンティティ
    /// @return 描画オブジェクトのポインタ. 生成失敗 (型起因) はnullptr
    /// @note 生成失敗はnullptrを負キャッシュし、再試行しない. 生成時は
    ///       default_global_param_とmaterial_overrides_を適用する
    IEntityGraphics* FindOrCreateGraphics(
            const ObjectID& id,
            const std::shared_ptr<entities::EntityBase>& entity) const;

    /// @brief 指定IDの描画オブジェクトを取得する
    /// @param id エンティティのID
    /// @return 描画オブジェクトのポインタ. 存在しない場合はnullptr
    /// @note 所有権は移譲しない (graphics_cache_が保持し続ける)
    IEntityGraphics* FindGraphics(const ObjectID&) const;

    /// @brief シーンのワールドBBoxから外接球を計算し、カメラの自動クリップ球を更新する
    /// @note カメラ操作でシーンがクリップされないよう、同期経路から呼ぶ.
    ///       シーン未設定またはBBoxが空の場合は自動クリッピングを解除する
    void UpdateAutoClipSphere() const;

    /// @brief scene_を走査して描画/可視リストを再構築し、ワールド変換と色を
    ///        リフレッシュする (EnsureSyncedのWalkステップ)
    void RebuildDrawList() const;

    /// @brief Assemblyツリーを再帰走査する (RebuildDrawListの実体)
    /// @param node 走査中のノード
    /// @param parent_accum 親までの累積変換 (G_root·…·G_{n-1})
    /// @param inherited_color 親までの最近接の色オーバーライド (無ければnullopt)
    /// @param inherited_opacity 親までの最近接の不透明度オーバーライド (無ければnullopt)
    /// @note 可視/抑制サブツリー (node.Display()で判定) と表示フィルタ除外型は
    ///       スキップする (キャッシュは温存). 未在席の描画オブジェクトは遅延生成し、
    ///       適用待ちマテリアルを適用する. node大域変換を掛けた累積をworld_transform_へ
    ///       流し (M_entityは含めない)、graphicsをシェーダー別にdraw_list_へ、
    ///       エンティティ毎に一意にvisible_list_へ収集する. 色/不透明度の最近接
    ///       オーバーライドを解決し、各描画オブジェクトへフレーム毎にPUSHする.
    void WalkAssembly(const models::Assembly& node,
                      const igesio::Matrix4d& parent_accum,
                      const std::optional<std::array<float, 3>>& inherited_color,
                      const std::optional<float>& inherited_opacity) const;

    /// @brief キャッシュした描画リストをシェーダー単位で描画する
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    void ExecuteDrawList(const DrawContext& ctx) const;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_RENDERER_H_
