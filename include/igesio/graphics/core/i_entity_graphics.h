/**
 * @file graphics/core/i_entity_graphics.h
 * @brief 描画クラスのインターフェース定義 (EntityGraphicsの型消去クラス)
 * @author Yayoi Habami
 * @date 2025-08-05
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_I_ENTITY_GRAPHICS_H_
#define IGESIO_GRAPHICS_CORE_I_ENTITY_GRAPHICS_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/numerics/bounding_box.h"
#include "igesio/entities/entity_type.h"
#include "igesio/models/global_param.h"
#include "igesio/graphics/core/i_open_gl.h"
#include "igesio/graphics/core/material_property.h"
#include "igesio/graphics/core/ray.h"
#include "igesio/graphics/core/draw_context.h"



namespace igesio::graphics {

/// @brief デフォルトの線の表示太さ [px]
constexpr double kDefaultLineWidth =
        models::kDefaultMaxLineWeight / models::kDefaultLineWeightGradations;

/// @brief 無限パラメータ範囲を持つ曲線/曲面を離散化する際のクランプ値
/// @note 描画 (i_curve_graphics) と範囲選択のサンプリングで離散化範囲を一致させる
///       ための共有定数. 半直線・直線・無限平面などの無限端をこの値で打ち切る
constexpr double kInfiniteParamClamp = 100.0;

/// @brief シェーダーのタイプ
/// @note この列挙体の値に基づき、どのシェーダープログラムを使用するかを決定する
/// @note `kNone`は最大値として使用される.
///       forループなどでは、`kNone`未満の値を使用すること
enum class ShaderType {
    /// @brief 汎用曲線シェーダー
    /// @note `ICurve`を継承したクラスについて、
    ///       曲線をCPU上で離散化したうえで、離散化した点をGPUに渡して描画する
    kGeneralCurve,
    /// @brief 円弧シェーダー (Type: 100; Circular Arc)
    kCircularArc,
    /// @brief 楕円シェーダー (Type: 104, Form: 1; Conic Arc)
    kEllipse,
    // 双曲線シェーダー/放物線シェーダーは特殊化なし

    /// @brief Copious Dataシェーダー
    ///        (Type: 106, Forms: 1-3, 11-13; Copious Data)
    kCopiousData,

    /// @brief 線分シェーダー (Type: 110, Form 0; Line)
    kSegment,
    /// @brief 半直線/直線シェーダー (Type: 110, Forms 1-2; Line)
    kLine,
    /// @brief 点シェーダー (Type: 116; Point)
    kPoint,
    /// @brief NURBS曲線シェーダー (Type: 126; Rational B-Spline Curve)
    kRationalBSplineCurve,

    /// @brief 汎用曲面シェーダー
    /// @note これ以降のすべてのシェーダーは、Lightを使用する
    kGeneralSurface,

    /// @brief NURBS曲面シェーダー (Type: 128; Rational B-Spline Surface)
    kRationalBSplineSurface,

    /// @brief 複数のシェーダーを使用する
    /// @note 子要素(child_graphics_)を持つEntityGraphics(複合ノード)で使用される.
    ///       Composite Curveなど、複数の子要素を持ち、それぞれの子要素で
    ///       異なるシェーダーを使用する場合に使用される.
    kComposite,
    /// @brief シェーダーなし
    /// @note 最大値として使用される
    kNone,
};

/// @brief 特定のシェーダーコードを持つShaderTypeか
/// @param shader_type ShaderTypeの値
/// @return kCompositeやkNoneなど、shader_typeが特定のシェーダーコード
///         を持たない場合はfalseを返す
inline bool HasSpecificShaderCode(const ShaderType shader_type) {
    return shader_type != ShaderType::kNone &&
           shader_type != ShaderType::kComposite;
}

/// @brief ShaderTypeの値を文字列に変換する
/// @param shader_type ShaderTypeの値
/// @return ShaderTypeの値に対応する文字列
std::string ToString(const ShaderType);

/// @brief シェーダーの計算が光源を使用するか
/// @param shader_type ShaderTypeの値
/// @return 光源を使用する場合はtrue, そうでない場合はfalse
/// @note kGeneralSurface以降のシェーダーは光源を使用する
///       (kCompositeとkNoneは除く)
bool UsesLighting(const ShaderType);


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
};

/// @brief EntityGraphicsの型消去クラス
/// @note すべての描画用クラスの基底クラス
/// @note 責務は2フェーズに分かれる:
///       - Build: `Synchronize`/`SyncTexture` がエンティティのジオメトリから
///         GPUリソース (VAO/テクスチャ等) を構築する. ジオメトリ変更時に再実行可.
///       - Draw: `Draw(..., ctx)` が `DrawContext` からPULLした表示状態 (変換・色・
///         材質・選択ハイライト) を適用して描画する. 論理状態は保持しない.
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
    igesio::Matrix4f world_transform_ = igesio::Matrix4f::Identity();
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

    /// @brief コンストラクタ
    /// @param gl OpenGL関数のラッパー
    /// @param use_entity_transform シェーダーのmodel変数に
    ///        entity_が参照する変換行列を掛け合わせるか
    explicit IEntityGraphics(const std::shared_ptr<IOpenGL>, bool);



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
    /// @param shader_type 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    /// @note shader_typeに合致する要素がない場合は何もしない
    virtual void Draw(gl::Uint, const ShaderType, const std::pair<float, float>&,
                      const DrawContext&) const = 0;

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    virtual void Draw(gl::Uint, const std::pair<float, float>&,
                      const DrawContext&) const = 0;

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    virtual void Synchronize() = 0;

    /// @brief テクスチャ用の描画リソースを同期する
    virtual void SyncTexture() = 0;



    /**
     * 描画用パラメータの取得・設定
     */

    /// @brief 描画用のグローバルパラメータを設定する
    /// @param global_param 描画用のグローバルパラメータ
    void SetGlobalParam(const std::shared_ptr<const models::GraphicsGlobalParam>);

    /// @brief グローバル座標系への変換行列を設定する
    /// @param matrix グローバル座標系への変換行列
    /// @note エンティティが定義される空間から、グローバル座標系への変換を行う
    ///       同次変換行列. `CircularArcGraphics`等の、パラメータをGPUに渡す
    ///       グラフィックスクラスでは、エンティティの定義空間からグローバル座標系への
    ///       変換 (すなわちエンティティがDEレコード7で指定する変換行列と、
    ///       モデル空間までの各親の変換行列をすべて掛け合わせたもの) を指定する.
    ///       `ICurveGraphics`等の、離散化をCPU上で行うグラフィックスクラスでは、
    ///       エンティティがDEレコード7で指定する変換行列を含まない.
    virtual void SetWorldTransform(const igesio::Matrix4f&);
    /// @brief グローバル座標系への変換行列を取得する
    /// @return グローバル座標系への変換行列.
    ///         use_entity_transform_がtrueの場合は、
    ///         `SetWorldTransform`で設定された変換行列に
    ///         entity_が参照する変換行列を掛け合わせたものを返す.
    virtual igesio::Matrix4f GetWorldTransform() const = 0;

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
    virtual ShaderType GetShaderType() const = 0;

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return 全ての可能なシェーダータイプのリスト
    /// @note 例えば子要素がある場合、`GetShaderType`のShaderTypeに加えて、
    ///       各子要素のShaderTypeも含まれる.
    virtual std::unordered_set<ShaderType> GetShaderTypes() const {
        return {GetShaderType()};
    }

    /// @brief OpenGLリソースを解放する
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
    virtual std::optional<numerics::BoundingBox> GetWorldBoundingBox() const = 0;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_I_ENTITY_GRAPHICS_H_
