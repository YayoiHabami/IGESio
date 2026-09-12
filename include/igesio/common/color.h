/**
 * @file common/color.h
 * @brief RGBA色の値型
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note double値RGBA (0.0〜1.0)、 IGES (Type 314/DE13; 0〜100)、8bit (0〜255),
 *       描画用float (0.0〜1.0)、 16進カラーコードなど複数のスケールで扱われる色を
 *       統一して扱うためのデータ型. 変換はすべて本構造体のメンバ関数にまとめ,
 *       利用側では変換処理を書かないようにする.
 */
#ifndef IGESIO_COMMON_COLOR_H_
#define IGESIO_COMMON_COLOR_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>



namespace igesio {

/// @brief 色構造体
/// @note メンバはR, G, B, Aの4成分で、各成分は0.0～1.0のdouble値.
///       IGESの色 (RGB, 0〜100)・8bit (0〜255)・float・16進文字列
///       との変換はメンバ関数を通して行う.
/// @note 構築時に範囲検証は行わない (範囲外かどうかはIsInUnitRangeで判定する).
///       `Color{r, g, b}`のように3成分で構築した場合は不透明 (a = 1.0).
struct Color {
    /// @brief 赤成分 [0.0, 1.0]
    double r = 0.0;
    /// @brief 緑成分 [0.0, 1.0]
    double g = 0.0;
    /// @brief 青成分 [0.0, 1.0]
    double b = 0.0;
    /// @brief 不透明度 [0.0, 1.0] (1.0が完全不透明)
    double a = 1.0;

    /**
     * 生成関連
     */

    /// @brief 8bit値 (0〜255) から生成する
    /// @param r 赤成分 (0〜255)
    /// @param g 緑成分 (0〜255)
    /// @param b 青成分 (0〜255)
    /// @param a 不透明度 (0〜255; 既定は255 = 不透明)
    /// @note 範囲検証は行わない (範囲外の場合は不正なColorになる)
    static constexpr Color FromRGB255(const int r, const int g, const int b,
                                      const int a = 255) {
        return Color{r / 255.0, g / 255.0, b / 255.0, a / 255.0};
    }

    /// @brief IGES形式の色表現 (RGBのみ、0.0〜100.0) から生成する (a = 1.0)
    /// @param r 赤成分 (0.0〜100.0)
    /// @param g 緑成分 (0.0〜100.0)
    /// @param b 青成分 (0.0〜100.0)
    /// @note 範囲検証は行わない (範囲外の場合は不正なColorになる)
    static constexpr Color FromIgesRGB(const double r, const double g,
                                       const double b) {
        return Color{r / 100.0, g / 100.0, b / 100.0, 1.0};
    }

    /// @brief IGES形式の色表現 (RGBのみ、0.0〜100.0) の配列から生成する (a = 1.0)
    /// @param rgb RGB各成分 (0.0〜100.0)
    /// @note 範囲検証は行わない (範囲外の場合は不正なColorになる)
    static constexpr Color FromIgesRGB(const std::array<double, 3>& rgb) {
        return FromIgesRGB(rgb[0], rgb[1], rgb[2]);
    }

    /// @brief float RGBA (0.0〜1.0) から生成する
    /// @param rgba RGBA各成分 (0.0〜1.0)
    /// @note 範囲検証は行わない (範囲外の場合は不正なColorになる)
    static constexpr Color FromFloatRGBA(const std::array<float, 4>& rgba) {
        return Color{static_cast<double>(rgba[0]), static_cast<double>(rgba[1]),
                     static_cast<double>(rgba[2]), static_cast<double>(rgba[3])};
    }

    /// @brief float RGB (0.0～1.0) から生成する
    /// @param rgb RGB各成分 (0.0～1.0)
    /// @param a 不透明度 (0.0～1.0; 既定は1.0 = 不透明)
    /// @note 範囲検証は行わない (範囲外の場合は不正なColorになる)
    static constexpr Color FromFloatRGB(const std::array<float, 3>& rgb,
                                        const double a = 1.0) {
        return Color{static_cast<double>(rgb[0]), static_cast<double>(rgb[1]),
                     static_cast<double>(rgb[2]), a};
    }

    /// @brief 16進カラーコードから生成する
    /// @param text "#RRGGBB" / "RRGGBB" / "#RRGGBBAA" / "RRGGBBAA"
    ///        (大文字/小文字は区別しない)
    /// @return 6桁の場合はa = 1.0. 形式が不正な場合はstd::nullopt
    static std::optional<Color> TryParseHex(std::string_view text);

    /// @brief 16進カラーコードから生成する
    /// @param text "#RRGGBB" / "RRGGBB" / "#RRGGBBAA" / "RRGGBBAA"
    ///        (大文字/小文字は区別しない)
    /// @return 6桁の場合はa = 1.0
    /// @throw std::invalid_argument textの形式が不正な場合
    static Color FromHex(std::string_view text);

    /**
     * 変換関連
     */

    /// @brief floatのRGBA値 (0.0〜1.0) に変換する (描画関連用)
    /// @return RGBA各成分 (float)
    constexpr std::array<float, 4> ToFloatRGBA() const {
        return {static_cast<float>(r), static_cast<float>(g),
                static_cast<float>(b), static_cast<float>(a)};
    }

    /// @brief floatのRGB値 (0.0〜1.0) に変換する (αは捨てる)
    /// @return RGB各成分 (float)
    constexpr std::array<float, 3> ToFloatRGB() const {
        return {static_cast<float>(r), static_cast<float>(g),
                static_cast<float>(b)};
    }

    /// @brief IGESの色形式 (RGBのみ、0.0〜100.0) に変換する (αは捨てる)
    /// @return RGB各成分 (0.0〜100.0)
    constexpr std::array<double, 3> ToIgesRGB() const {
        return {r * 100.0, g * 100.0, b * 100.0};
    }

    /// @brief 8bitのRGB値 (0〜255) に変換する (αは捨てる)
    /// @return RGB各成分 (0〜255)
    std::array<int, 3> ToRGB255() const;

    /// @brief 16進カラーコードに変換する
    /// @param with_alpha trueの場合は"#RRGGBBAA"、falseの場合は"#RRGGBB"
    /// @return 大文字の16進カラーコード
    std::string ToHex(bool with_alpha = false) const;

    /// @brief 不透明度を変更した色を返す
    /// @param alpha 新しい不透明度 [0.0, 1.0]
    constexpr Color WithAlpha(const double alpha) const {
        return Color{r, g, b, alpha};
    }

    /// @brief 全成分の値を0.0〜1.0に正規化した色を返す
    constexpr Color Clamped() const {
        return Color{std::clamp(r, 0.0, 1.0), std::clamp(g, 0.0, 1.0),
                     std::clamp(b, 0.0, 1.0), std::clamp(a, 0.0, 1.0)};
    }

    /**
     * その他判定等
     */

    /// @brief 全成分の値が[0.0, 1.0]の範囲に収まっているか
    /// @note NaNを含む場合はfalse
    constexpr bool IsInUnitRange() const {
        return r >= 0.0 && r <= 1.0 && g >= 0.0 && g <= 1.0
            && b >= 0.0 && b <= 1.0 && a >= 0.0 && a <= 1.0;
    }

    /// @brief RGB空間におけるユークリッド距離の二乗 (αは無視)
    /// @param other 比較する色
    /// @note 最近接色の探索用.
    constexpr double SquaredDistanceRGB(const Color& other) const {
        const double dr = r - other.r;
        const double dg = g - other.g;
        const double db = b - other.b;
        return dr * dr + dg * dg + db * db;
    }

    /// @brief RGBAの各成分を添字で取得する
    /// @param index 成分の添字 (0: r, 1: g, 2: b, 3: a)
    /// @throw std::out_of_range indexが3より大きい場合
    constexpr double operator[](const std::size_t index) const {
        switch (index) {
            case 0: return r;
            case 1: return g;
            case 2: return b;
            case 3: return a;
            default:
                throw std::out_of_range(
                        "Color index must be in [0, 3], but got "
                        + std::to_string(index) + ".");
        }
    }
};

/// @brief 厳密比較 (全成分が等しいか)
/// @note 近似比較が必要な場合は成分ごとに許容誤差付きで比較すること
constexpr bool operator==(const Color& lhs, const Color& rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

/// @brief 厳密比較の否定
constexpr bool operator!=(const Color& lhs, const Color& rhs) {
    return !(lhs == rhs);
}

}  // namespace igesio

#endif  // IGESIO_COMMON_COLOR_H_
