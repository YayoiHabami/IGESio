/**
 * @file graphics/core/i_entity_graphics.h
 * @brief 描画クラスのインターフェース定義 (EntityGraphicsの型消去クラス)
 * @author Yayoi Habami
 * @date 2025-08-05
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_I_ENTITY_GRAPHICS_H_
#define IGESIO_GRAPHICS_CORE_I_ENTITY_GRAPHICS_H_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/entities/entity_type.h"
#include "igesio/models/global_param.h"
#include "igesio/graphics/core/i_open_gl.h"
#include "igesio/graphics/core/material_property.h"
#include "igesio/graphics/core/ray.h"
#include "igesio/graphics/core/draw_context.h"
#include "igesio/graphics/core/shader_id.h"



namespace igesio::graphics {

/// @brief デフォルトの線の表示太さ [px]
constexpr double kDefaultLineWidth =
        models::kDefaultMaxLineWeight / models::kDefaultLineWeightGradations;

/// @brief 無限パラメータ範囲を持つ曲線/曲面を離散化する際のクランプ値
/// @note 描画 (i_curve_graphics) と範囲選択のサンプリングで離散化範囲を一致させる
///       ための共有定数. 半直線・直線・無限平面などの無限端をこの値で打ち切る
constexpr double kInfiniteParamClamp = 100.0;

/// @brief 特定のシェーダーコードを持つShaderIdか
/// @param shader_id ShaderIdの値
/// @return kCompositeやkNone、未登録IDなど、shader_idが特定の
///         シェーダーコードを持たない場合はfalseを返す
/// @note ShaderRegistryの登録内容 (codeの有無) を参照する
bool HasSpecificShaderCode(const ShaderId);

/// @brief 面塗り (三角形描画) のシェーダーか
/// @param shader_id シェーダーの識別子
/// @return 面塗りの場合はtrue
/// @note ShaderRegistryのメタ情報 (ShaderDrawCategory) を参照する
bool IsSurfaceFill(const ShaderId);

/// @brief ShaderIdの値を文字列に変換する
/// @param shader_id ShaderIdの値
/// @return ShaderRegistryに登録された名前. 未登録IDの場合は"Unknown"
std::string ToString(const ShaderId);

/// @brief シェーダーの計算が光源を使用するか
/// @param shader_id ShaderIdの値
/// @return 光源を使用する場合はtrue, そうでない場合はfalse
/// @note ShaderRegistryのメタ情報 (uses_lighting) を参照する
bool UsesLighting(const ShaderId);


/// @brief 範囲選択用のジオメトリサンプル（ワールド座標）
struct SelectionSamples {
    /// @brief 折れ線群. 各内側ベクタが連続頂点列.
    /// @note 1本の曲線でも、TryGetPointAtがnulloptを返す点や射影不能 (near面より
    ///       手前) の点で分割して複数の折れ線になり得る. 欠落点をまたいで頂点を
    ///       橋渡ししない (偽の線分を作らないため). 曲面境界は各ループが1本の折れ線
    std::vector<std::vector<Vector3d>> polylines;
    /// @brief 孤立点（Point(116)など）
    std::vector<Vector3d> points;
    /// @brief polylinesが閉じた境界ループか（点内外判定に利用可能か）
    /// @note 分割が起きた場合は閉ループ性が保証されないためfalseとする
    bool polylines_closed = false;

    /// @brief 線分群の共有頂点プール (ワールド座標)
    /// @note メッシュのエッジのように多数の線分が頂点を共有する場合用.
    ///       判定側は各頂点を1回だけ射影すればよく、線分ごとの点列
    ///       (polylines) より大幅に軽い. 内包判定ではpointsと同様に
    ///       「全点が矩形内」の対象になる (線分に属さない頂点も含む)
    std::vector<Vector3d> segment_vertices;
    /// @brief segment_verticesへのインデックスペア (各ペアが1線分)
    /// @note 交差判定で線分として評価される. 折れ線と異なり連続性を
    ///       仮定しないため、エッジ集合をそのまま表現できる
    std::vector<std::array<std::uint32_t, 2>> segments;

    /// @brief サンプルが空か
    /// @note 範囲選択の早期リジェクトに利用可能
    bool IsEmpty() const {
        return points.empty() && polylines.empty() &&
               segment_vertices.empty() && segments.empty();
    }
};

/// @brief EntityGraphicsの型消去クラス
/// @note すべての描画用クラスの基底クラス
/// @note 処理は段階に分かれ、各段階に置ける処理が決まっている。レンダラは
///       生成 (ctor) → `PrewarmCpu` (CPU準備) → `Synchronize`/`DoSynchronize`
///       (GPU転送) → `Draw` (毎フレーム) の順に駆動する。シェーダー型列挙・ピック・
///       バウンディング・バケット構築が要る状態は `PrewarmCpu` までに確定させること。
///       各関数に置ける処理は個々のコメントを参照。
/// @note GLを呼べるのはGPU転送 (`DoSynchronize`)・毎フレーム描画 (`Draw`)・
///       `SyncTexture`・`Cleanup` に限る。生成・`PrewarmCpu`・ピック系では呼ばない。
/// @note 子の保持は2方式。`child_graphics_` + `AddChildGraphics` を使うと、型列挙・
///       変換/色伝播・`PrewarmCpu` カスケード・選択集約を基底が肩代わりする。独自メンバで
///       子を持つ場合はこれらを各々オーバーライドして伝播し、子は `PrewarmCpu` で生成する。
class IEntityGraphics {
 protected:
    /// @brief エンティティの色 (RGBA)
    /// @note フラグメントシェーダーの`mainColor`変数に対応する.
    /// @note オーバーライドした、メインで使用する色. 例えばサーフェスの場合、
    ///       面の色に相当し、境界線の色は別途設定する必要がある
    float color_[4] = {.8f, .8f, .8f, 1.0f};  // デフォルトは薄いグレー
    /// @brief 色をオーバーライドしたか. falseの場合は、エンティティ側が指定する色
    ///        を使用する. 取得に失敗した場合や、trueの場合は`color_`を使用する
    bool is_color_overridden_ = false;

    /// @brief (IGESで管理されない) 描画プロパティ
    MaterialProperty material_property_;

    /// @brief グローバル座標系への変換行列
    /// @note 頂点シェーダーの`model`変数に対応する.
    /// @note エンティティが定義される空間から、グローバル座標系への変換を行う
    ///       同次変換行列. `CircularArcGraphics`等の、パラメータをGPUに渡す
    ///       グラフィックスクラスでは、エンティティの定義空間からグローバル座標系への
    ///       変換 (すなわちエンティティがDEレコード7で指定する変換行列と、
    ///       モデル空間までの各親の変換行列をすべて掛け合わせたもの) を指定する.
    ///       `ICurveGraphics`等の、離散化をCPU上で行うグラフィックスクラスでは、
    ///       エンティティがDEレコード7で指定する変換行列を含まない.
    /// @note 頂点シェーダーの`model`変数に対応する.
    /// @note CPU側の正準値として倍精度で保持する. GPUへ渡す際にのみ単精度へ
    ///       変換することで、BB計算・ピッキング等の幾何計算での精度損失を防ぐ.
    igesio::Matrix4d world_transform_ = igesio::Matrix4d::Identity();
    /// @brief model変数にentity_が参照する変換行列を使用するか
    /// @note 通常、model変数には、エンティティよりも上位の変換行列
    ///       (親 -> ... -> モデル空間)　を使用する. 各個別エンティティの
    ///       `DrawImpl`実装において、シェーダーに定義空間でのパラメータを
    ///       渡す場合は、world_transform_に加えて、entity_が参照する
    ///       変換行列を掛け合わせたものを使用する.
    bool use_entity_transform_ = false;

    /// @brief OpenGL関数のラッパー
    std::shared_ptr<IOpenGL> gl_;

    /// @brief 描画に関するグローバルパラメータ
    std::shared_ptr<const models::GraphicsGlobalParam> global_param_;

    /// @brief Synchronize()実行時に記録した同期キー
    /// @note CurrentGeometryKey()との不一致が再テッセレーションの要否を表す
    uint64_t synced_geometry_key_ = 0;

    /// @brief 再同期の実体 (エンティティのジオメトリからGPUリソースを構築・転送する)
    /// @note 呼び出しはSynchronize() (NVI) 経由でのみ行うこと.
    ///       キー記録を基底で一元化するための分離
    /// @note GLリソースの生成・転送のみを行う。重いCPU準備は`PrewarmCpu`に置き、
    ///       未準備時に備え冒頭で`PrewarmCpu`を呼ぶ。同期キーはSynchronizeが記録するため
    ///       ここでは設定しない。
    virtual void DoSynchronize() = 0;

    /// @brief コンストラクタ
    /// @param gl OpenGL関数のラッパー
    /// @param use_entity_transform シェーダーのmodel変数に
    ///        entity_が参照する変換行列を掛け合わせるか
    /// @note 派生クラスのコンストラクタは同一性の確立 (entity_/gl_/shader_id_・
    ///       自前バッファ) のみ行うこと。`Synchronize`を呼ばず、GLリソース生成や重い
    ///       CPU処理も行わない (同期はレンダラ/ファクトリが駆動する)。
    explicit IEntityGraphics(const std::shared_ptr<IOpenGL>&, bool);



 public:
    /// @brief デストラクタ
    virtual ~IEntityGraphics() = default;

    // コピーコンストラクタとコピー代入演算子を禁止
    IEntityGraphics(const IEntityGraphics&) = delete;
    IEntityGraphics& operator=(const IEntityGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のEntityGraphics
    /// @note ムーブ元のリソース所有権を放棄
    IEntityGraphics(IEntityGraphics&&) noexcept;

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のEntityGraphics
    /// @return *this
    /// @note ムーブ元のリソース所有権を放棄
    IEntityGraphics& operator=(IEntityGraphics&&) noexcept;

    /// @brief エンティティのIDを取得する
    /// @return エンティティのID
    virtual const ObjectID& GetEntityID() const = 0;

    /// @brief 描画オブジェクトのIDを取得する
    /// @return 描画オブジェクトのID
    virtual const ObjectID& GetGraphicsID() const = 0;

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param shader_id 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    /// @note shader_idに合致する要素がない場合は何もしない
    /// @note 描画コマンドの発行とctxからの状態適用のみを行う。バケットに登録されていても
    ///       バッファが空・未構築なら自衛する (IsDrawable等)。論理状態は変更しない。
    virtual void Draw(gl::Uint, const ShaderId, const std::pair<float, float>&,
                      const DrawContext&) const = 0;

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    virtual void Draw(gl::Uint, const std::pair<float, float>&,
                      const DrawContext&) const = 0;

    /// @brief エンティティをセットアップする (NVI; 非virtual)
    /// @note 内部で参照するエンティティの状態に基づいて、描画用のリソースを
    ///       再セットアップ (DoSynchronize) し、末尾で同期キーを記録する.
    ///       キー記録を基底で一元化し、各実装の記録漏れを構造的に排除する
    /// @note 非virtual。派生クラスは本関数でなく`DoSynchronize`を実装する。
    void Synchronize() {
        DoSynchronize();
        synced_geometry_key_ = CurrentGeometryKey();
    }

    /// @brief 現在の同期キーを計算する
    /// @return テッセレーションが読む全エンティティの (ObjectID, GeometryRevision)
    ///         を順序通りにハッシュ結合した値
    /// @note 既定実装はEntityGraphics<T>にあり、自エンティティ+DE変換チェーン+
    ///       物理従属子 (GetChildIDs) を再帰的に結合する. 同一性(ID)を含めることで、
    ///       参照集合の変化とカウンタの変化の両方が必ずキーに現れる
    ///       (リビジョン総和では参照先交換時に相殺しうるため不可)
    /// @note 同期状態に依らずいつでも呼べること (NeedsResyncの判定に使う)。
    virtual uint64_t CurrentGeometryKey() const = 0;

    /// @brief 再Synchronizeが必要か
    /// @return 最後のSynchronize以降に同期キーが変化した場合はtrue
    bool NeedsResync() const {
        return synced_geometry_key_ != CurrentGeometryKey();
    }

    /// @brief テクスチャ用の描画リソースを同期する
    /// @note GLテクスチャの生成・転送を行う (GLスレッドから呼ぶこと)。
    virtual void SyncTexture() = 0;

    /// @brief 描画用CPUデータ (テッセレーション・遅延する子の生成等) を事前構築する
    /// @note GLを一切呼ばない。重いCPU処理を持つクラス (RestrictedSurfaceGraphics等) や
    ///       子を遅延生成するクラス (CurveOnAParametricSurfaceGraphics) がオーバーライド
    ///       し、結果を自身のCPUステージングへ格納する。レンダラのreconcile経路から
    ///       ワーカースレッドで並列に呼べるよう、自身のメンバのみ書き込む
    ///       (entity_は読み取り専用)。
    /// @note 冪等であること (同期キー/存在チェックでガードし、再呼び出しで作り直さない)。
    ///       ステージングは`Cleanup`で破棄されない専用メンバへ置く (DoSynchronizeが
    ///       先頭でCleanupするため)。GPU転送はSynchronize/DoSynchronizeが別途行う。
    virtual void PrewarmCpu() {}



    /**
     * 描画用パラメータの取得・設定
     */

    /// @brief 描画用のグローバルパラメータを設定する
    /// @param global_param 描画用のグローバルパラメータ
    /// @note 内部で調整済みのコピーを保持するため、引数の寿命には依存しない
    void SetGlobalParam(const models::GraphicsGlobalParam&);

    /// @brief グローバル座標系への変換行列を設定する
    /// @param matrix グローバル座標系への変換行列
    /// @note エンティティが定義される空間から、グローバル座標系への変換を行う
    ///       同次変換行列. `CircularArcGraphics`等の、パラメータをGPUに渡す
    ///       グラフィックスクラスでは、エンティティの定義空間からグローバル座標系への
    ///       変換 (すなわちエンティティがDEレコード7で指定する変換行列と、
    ///       モデル空間までの各親の変換行列をすべて掛け合わせたもの) を指定する.
    ///       `ICurveGraphics`等の、離散化をCPU上で行うグラフィックスクラスでは、
    ///       エンティティがDEレコード7で指定する変換行列を含まない.
    virtual void SetWorldTransform(const igesio::Matrix4d&);
    /// @brief グローバル座標系への変換行列をGPU用の単精度で取得する
    /// @return グローバル座標系への変換行列 (float).
    ///         use_entity_transform_がtrueの場合は、
    ///         `SetWorldTransform`で設定された変換行列に
    ///         entity_が参照する変換行列を掛け合わせたものを返す.
    /// @note CPU側の幾何計算では精度を失わないようGetWorldTransformD()を使うこと.
    virtual igesio::Matrix4f GetWorldTransform() const = 0;
    /// @brief グローバル座標系への変換行列を倍精度で取得する (CPU幾何の正準値)
    /// @return グローバル座標系への変換行列 (double).
    ///         use_entity_transform_がtrueの場合の扱いはGetWorldTransform()と同じ.
    virtual igesio::Matrix4d GetWorldTransformD() const = 0;

    /// @brief 現在のメインの色を取得する
    /// @return メインの色 (RGBA; [0, 1]の範囲)
    /// @note SetColorで色をオーバーライドした場合はその色を返す.
    ///       そうでない場合は、エンティティが保持する色を返す.
    virtual std::array<float, 4> GetColor() const = 0;

    /// @brief メインの色を設定する
    /// @param color メインの色 (RGBA; [0, 1]の範囲)
    /// @note この関数で設定可能な色は描画上の色であり、エンティティが保持する
    ///       (IGES側で定義する) 色とは異なる.したがって、この関数で色を設定したとしても、
    ///       エンティティのインスタンスの色情報は変更されない.
    virtual void SetColor(const std::array<float, 4>&);

    /// @brief 色をデフォルトのエンティティの色に戻す
    virtual void ResetColor() = 0;

    /// @brief 線の太さを取得する
    /// @return 線の太さ
    virtual double GetLineWidth() const = 0;

    /// @brief 描画属性を取得する (const版)
    const MaterialProperty& MaterialProperty() const {
        return material_property_;
    }
    /// @brief 描画属性を取得する (非const版)
    graphics::MaterialProperty& MaterialProperty() {
        return material_property_;
    }



    /**
     * リソースの確認・変更
     */

    /// @brief 描画用のシェーダーのタイプを取得する
    /// @return 描画用のシェーダーのタイプ
    virtual ShaderId GetShaderId() const = 0;

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return 全ての可能なシェーダータイプのリスト
    /// @note 例えば子要素がある場合、`GetShaderId`のShaderIdに加えて、
    ///       各子要素のShaderIdも含まれる.
    /// @note 描きうるシェーダー型 (能力) を返し、GPU生成物 (vbo_/edge_buffer_) に
    ///       依存させないこと。バケット構築 (RebuildDrawBuckets) はGPU転送前・
    ///       `PrewarmCpu`後に本関数を呼ぶ。GPUバッファで決まる能力 (面エッジ) は
    ///       無条件に報告し (空ならDrawが早期return)、CPUで確定する型 (遅延する子の型)
    ///       は`PrewarmCpu`で子を生成して反映する。
    virtual std::unordered_set<ShaderId> GetShaderIds() const {
        return {GetShaderId()};
    }

    /// @brief OpenGLリソースを解放する
    /// @note GLリソースの解放のみ。`PrewarmCpu`のCPUステージングは破棄しない
    ///       (DoSynchronizeが先頭で本関数を呼ぶため)。
    virtual void Cleanup() = 0;

    /// @brief 描画可能な状態かどうかを確認する
    /// @note GPUへパラメータを渡し、GPU上で離散点を計算する場合は
    ///       オーバーライド不要.
    virtual bool IsDrawable() const = 0;



    /**
     * ピッキング（レイ交差判定）
     */

    /// @brief レイとの交差判定が可能か
    /// @return 交差判定可能な場合はtrue, そうでない場合はfalse
    /// @note デフォルト実装は常にfalseを返す
    virtual bool CanIntersect() const { return false; }

    /// @brief レイとエンティティの交差点を求める
    /// @param ray ワールド空間のレイ (kRayとして扱う)
    /// @param params 探索制御パラメータ
    /// @return 交差点のリスト (distance昇順). CanIntersect()がfalseの場合は空リスト
    /// @note デフォルト実装は空リストを返す
    /// @note ピック・選択系 (CanIntersect/本関数/GetSelectionSamples) はエンティティを
    ///       解析的に読み、GPUメッシュに依存しない (GPU転送なしで成立)。委譲型は
    ///       `PrewarmCpu`後に子へ委譲してよい。
    virtual std::vector<RayHit> Intersect(
            const Ray&,
            const RayIntersectionParams& = {}) const { return {}; }

    /// @brief 範囲選択用にエンティティをサンプリングした点列を返す
    /// @param params サンプリング制御パラメータ
    /// @return ワールド座標のサンプル. 判定不可なら空
    /// @note デフォルト実装は空を返す
    virtual SelectionSamples GetSelectionSamples(
            const SelectionSampleParams& = {}) const { return {}; }

    /// @brief ワールド座標系におけるバウンディングボックスを取得する
    /// @return エンティティのバウンディングボックスにworld_transform_を
    ///         適用したもの. BBを定義できない (IGeometryでない) 場合はstd::nullopt
    /// @note ピッキング時の代表深度の算出に用いる
    /// @note エンティティ由来でGPU生成物に依存しない (GPU転送なしで成立)。
    virtual std::optional<numerics::BoundingBox> GetWorldBoundingBox() const = 0;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_I_ENTITY_GRAPHICS_H_
