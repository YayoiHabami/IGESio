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
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/matrix.h"
#include "igesio/entities/entity_type.h"
#include "igesio/models/global_param.h"
#include "igesio/graphics/core/i_open_gl.h"



namespace igesio::graphics {

/// @brief デフォルトの線の表示太さ [px]
constexpr double kDefaultLineWidth = 1.0;

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
    /// @brief NURBS曲線シェーダー (Type: 126; Rational B-Spline Curve)
    kRationalBSplineCurve,

    /// @brief 複数のシェーダーを使用する
    /// @note CompositeEntityGraphicsを継承したクラスで使用される.
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




/// @brief EntityGraphicsの型消去クラス
/// @note すべての描画用クラスの基底クラス
class IEntityGraphics {
 protected:
    /// @brief エンティティの色 (RGBA)
    /// @note フラグメントシェーダーの`mainColor`変数に対応する.
    /// @note オーバーライドした、メインで使用する色. 例えばサーフェスの場合、
    ///       面の色に相当し、境界線の色は別途設定する必要がある
    GLfloat color_[4] = {.8f, .8f, .8f, 1.0f};  // デフォルトは薄いグレー
    /// @brief 色をオーバーライドしたか. falseの場合は、エンティティ側が指定する色
    ///        を使用する. 取得に失敗した場合や、trueの場合は`color_`を使用する
    bool is_color_overridden_ = false;

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
    virtual uint64_t GetEntityID() const = 0;

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param shader_type 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note shader_typeに合致する要素がない場合は何もしない
    virtual void Draw(GLuint, const ShaderType, const std::pair<float, float>&) const = 0;

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    virtual void Draw(GLuint, const std::pair<float, float>&) const = 0;

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    virtual void Synchronize() = 0;



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
    virtual std::array<GLfloat, 4> GetColor() const = 0;

    /// @brief メインの色を設定する
    /// @param color メインの色 (RGBA; [0, 1]の範囲)
    /// @note この関数で設定可能な色は描画上の色であり、エンティティが保持する
    ///       (IGES側で定義する) 色とは異なる.したがって、この関数で色を設定したとしても、
    ///       エンティティのインスタンスの色情報は変更されない.
    virtual void SetColor(const std::array<GLfloat, 4>&);

    /// @brief 色をデフォルトのエンティティの色に戻す
    virtual void ResetColor() = 0;

    /// @brief 線の太さを取得する
    /// @return 線の太さ
    virtual double GetLineWidth() const = 0;



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
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_I_ENTITY_GRAPHICS_H_
