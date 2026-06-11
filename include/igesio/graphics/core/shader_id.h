/**
 * @file graphics/core/shader_id.h
 * @brief シェーダープログラムの開いた識別子を定義する
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_SHADER_ID_H_
#define IGESIO_GRAPHICS_CORE_SHADER_ID_H_

#include <cstddef>
#include <cstdint>
#include <functional>

namespace igesio::graphics {

/// @brief シェーダープログラムの識別子
/// @note 閉じた列挙体ではなく開いたキーであり、ユーザーが`ShaderRegistry::Register`で
///       独自シェーダーを登録すると新しい値が採番される。組み込みシェーダーは
///       静的メンバ定数 (固定値0〜10) として提供する。
/// @note 値の大小・順序に意味は無い。照明使用・表示モードでの取捨等の性質は
///       `ShaderRegistry`が保持するメタ情報 (`ShaderInfo`) で判定する
///       (`UsesLighting`/`IsSurfaceFill`等のヘルパーを参照)。
class ShaderId {
 public:
    /// @brief 汎用曲線シェーダー
    /// @note `ICurve`を継承したクラスについて、曲線をCPU上で離散化したうえで、
    ///       離散化した点をGPUに渡して描画する
    static const ShaderId kGeneralCurve;
    /// @brief 円弧シェーダー (Type: 100; Circular Arc)
    static const ShaderId kCircularArc;
    /// @brief 楕円シェーダー (Type: 104, Form: 1; Conic Arc)
    /// @note 双曲線/放物線は特殊化なし (汎用曲線シェーダーを使用)
    static const ShaderId kEllipse;
    /// @brief Copious Dataシェーダー (Type: 106, Forms: 1-3, 11-13)
    static const ShaderId kCopiousData;
    /// @brief 線分シェーダー (Type: 110, Form 0; Line)
    static const ShaderId kSegment;
    /// @brief 半直線/直線シェーダー (Type: 110, Forms 1-2; Line)
    static const ShaderId kLine;
    /// @brief 点シェーダー (Type: 116; Point)
    static const ShaderId kPoint;
    /// @brief NURBS曲線シェーダー (Type: 126; Rational B-Spline Curve)
    static const ShaderId kRationalBSplineCurve;
    /// @brief サーフェス境界エッジシェーダー
    /// @note 面の境界を線として描画する (汎用曲線シェーダーを再利用する)
    static const ShaderId kSurfaceEdge;
    /// @brief 汎用曲面シェーダー
    static const ShaderId kGeneralSurface;
    /// @brief NURBS曲面シェーダー (Type: 128; Rational B-Spline Surface)
    static const ShaderId kRationalBSplineSurface;

    /// @brief 複数のシェーダーを使用する (センチネル; シェーダーコード無し)
    /// @note 子要素(child_graphics_)を持つEntityGraphics(複合ノード)で使用される.
    ///       Composite Curveなど、複数の子要素を持ち、それぞれの子要素で
    ///       異なるシェーダーを使用する場合に使用される.
    static const ShaderId kComposite;
    /// @brief シェーダーなし (センチネル; シェーダーコード無し)
    static const ShaderId kNone;

    /// @brief デフォルトコンストラクタ (kNoneと等価)
    constexpr ShaderId() = default;

    /// @brief 値を指定して構築する
    /// @param value 識別子の内部値
    /// @note 通常はこのコンストラクタを直接使わず、組み込み定数または
    ///       `ShaderRegistry::Register`の戻り値を使用すること
    constexpr explicit ShaderId(const std::uint32_t value) : value_(value) {}

    /// @brief 内部値を取得する
    /// @return 識別子の内部値
    constexpr std::uint32_t Value() const { return value_; }

    /// @brief 等値比較
    friend constexpr bool operator==(const ShaderId lhs, const ShaderId rhs) {
        return lhs.value_ == rhs.value_;
    }
    /// @brief 非等値比較
    friend constexpr bool operator!=(const ShaderId lhs, const ShaderId rhs) {
        return lhs.value_ != rhs.value_;
    }

 private:
    /// @brief kNoneの内部値
    static constexpr std::uint32_t kNoneValue = 0xFFFFFFFFu;

    /// @brief 識別子の内部値
    std::uint32_t value_ = kNoneValue;
};

// 組み込み定数の定義 (値はShaderRegistryの組み込み登録と1対1に対応する)

/// @brief 汎用曲線シェーダー
/// @note `ICurve`を継承したクラスについて、曲線をCPU上で離散化したうえで、
    ///       離散化した点をGPUに渡して描画する
inline const ShaderId ShaderId::kGeneralCurve{0};
/// @brief 円弧シェーダー (Type: 100; Circular Arc)
inline const ShaderId ShaderId::kCircularArc{1};
/// @brief 楕円シェーダー (Type: 104, Form: 1; Conic Arc)
/// @note 双曲線/放物線は特殊化なし (汎用曲線シェーダーを使用)
inline const ShaderId ShaderId::kEllipse{2};
/// @brief Copious Dataシェーダー (Type: 106, Forms: 1-3, 11-13)
inline const ShaderId ShaderId::kCopiousData{3};
/// @brief 線分シェーダー (Type: 110, Form 0; Line)
inline const ShaderId ShaderId::kSegment{4};
/// @brief 半直線/直線シェーダー (Type: 110, Forms 1-2; Line)
inline const ShaderId ShaderId::kLine{5};
/// @brief 点シェーダー (Type: 116; Point)
inline const ShaderId ShaderId::kPoint{6};
/// @brief NURBS曲線シェーダー (Type: 126; Rational B-Spline Curve)
inline const ShaderId ShaderId::kRationalBSplineCurve{7};
/// @brief サーフェス境界エッジシェーダー
inline const ShaderId ShaderId::kSurfaceEdge{8};
/// @brief 汎用曲面シェーダー
inline const ShaderId ShaderId::kGeneralSurface{9};
/// @brief NURBS曲面シェーダー (Type: 128; Rational B-Spline Surface)
inline const ShaderId ShaderId::kRationalBSplineSurface{10};
/// @brief 複数のシェーダーを使用する (センチネル; シェーダーコード無し)
/// @note 子要素(child_graphics_)を持つEntityGraphics(複合ノード)で使用される.
///       Composite Curveなど、複数の子要素を持ち、それぞれの子要素で
///       異なるシェーダーを使用する場合に使用される.
inline const ShaderId ShaderId::kComposite{0xFFFFFFFEu};
/// @brief シェーダーなし (センチネル; シェーダーコード無し)
inline const ShaderId ShaderId::kNone{0xFFFFFFFFu};

}  // namespace igesio::graphics

namespace std {

/// @brief ShaderIdをunordered_map/setのキーにするためのハッシュ特殊化
template <>
struct hash<igesio::graphics::ShaderId> {
    /// @brief ハッシュ値を計算する
    /// @param id ハッシュするShaderId
    /// @return 内部値のハッシュ
    std::size_t operator()(const igesio::graphics::ShaderId id) const noexcept {
        return std::hash<std::uint32_t>{}(id.Value());
    }
};

}  // namespace std

#endif  // IGESIO_GRAPHICS_CORE_SHADER_ID_H_
