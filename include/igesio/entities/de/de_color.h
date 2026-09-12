/**
 * @file entities/de/de_color.h
 * @brief 13th Directory Entryフィールド (Color) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_DE_COLOR_H_
#define IGESIO_ENTITIES_DE_DE_COLOR_H_

#include <array>
#include <memory>

#include "igesio/common/color.h"
#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/de/de_field_wrapper.h"



namespace igesio::entities {

/// @brief 標準色 (13th field of DE; Color Number)
/// @note IGESファイルにおけるエンティティの表示色を定義
enum class ColorNumber {
    /// @brief 色未指定（デフォルト）
    kNoColor = 0,
    /// @brief 黒 (RGB: #000000)
    kBlack = 1,
    /// @brief 赤 (RGB: #FF0000)
    kRed = 2,
    /// @brief 緑 (RGB: #00FF00)
    kGreen = 3,
    /// @brief 青 (RGB: #0000FF)
    kBlue = 4,
    /// @brief 黄 (RGB: #FFFF00)
    kYellow = 5,
    /// @brief マゼンタ (RGB: #FF00FF)
    kMagenta = 6,
    /// @brief シアン (RGB: #00FFFF)
    kCyan = 7,
    /// @brief 白 (RGB: #FFFFFF)
    kWhite = 8
};

/// @brief 標準色をColorへ変換する
/// @param number 標準色番号
/// @return 対応する色 (a = 1.0). kNoColorは黒として扱う
constexpr Color ToColor(const ColorNumber number) {
    switch (number) {
        case ColorNumber::kRed: return Color{1.0, 0.0, 0.0};
        case ColorNumber::kGreen: return Color{0.0, 1.0, 0.0};
        case ColorNumber::kBlue: return Color{0.0, 0.0, 1.0};
        case ColorNumber::kYellow: return Color{1.0, 1.0, 0.0};
        case ColorNumber::kMagenta: return Color{1.0, 0.0, 1.0};
        case ColorNumber::kCyan: return Color{0.0, 1.0, 1.0};
        case ColorNumber::kWhite: return Color{1.0, 1.0, 1.0};
        case ColorNumber::kNoColor:
        case ColorNumber::kBlack:
            return Color{};
    }
    return Color{};
}

/// @brief 指定した色に最も近い標準色を求める
/// @param color 比較する色 (αは無視する)
/// @return 最も近い標準色 (kBlack〜kWhite). 同距離の場合は番号の小さい方
/// @note 距離はRGB空間のユークリッド距離で評価する
ColorNumber ClosestColorNumber(const Color& color);

/// @brief 色フィールドを表すクラス
/// @note DEフィールド13: Color Number
///       非負の値は標準色、負の値はColor Definition Entity (Type 314)への参照
class DEColor : public DEFieldWrapper<IColorDefinition> {
 private:
    /// @brief 色 (規定色)
    /// @note kNoColorは規定色が指定されていないか、
    ///       Color Definition Entityが参照されていることを示す
    ColorNumber color_ = ColorNumber::kNoColor;

 public:
    using DEFieldWrapper<IColorDefinition>::DEFieldWrapper;

    using DEFieldWrapper<IColorDefinition>::GetPointer;

    /// @brief 指定された型のポインタを取得
    /// @return 指定された型のポインタ
    std::shared_ptr<const IColorDefinition> GetPointer() const;

    /// @brief 色を指定するコンストラクタ
    /// @param value 色の値
    /// @throw igesio::DataFormatError valueが無効な場合 (0未満または8より大きい)
    explicit DEColor(const int);

    /// @brief 色を指定するコンストラクタ
    /// @param value 色の値
    explicit DEColor(const ColorNumber);

    /// @brief 色 (RGB) を取得する
    /// @return 解決した色 (a = 1.0). 標準色はその色、参照先のColor Definition
    ///         Entityがあればその定義色、未設定・参照未解決の場合は黒
    Color GetRGB() const;

    /// @brief 色 (CMY) を取得する
    /// @return CMY値の配列 (それぞれ[0, 1])
    std::array<double, 3> GetCMY() const {
        const Color rgb = GetRGB();
        return {1.0 - rgb.r, 1.0 - rgb.g, 1.0 - rgb.b};
    }

    /// @brief 色を設定する
    /// @param value 色の値
    void SetColor(const ColorNumber);

    /// @brief 値を取得する
    /// @param id2de IDとDEポインターのマッピング
    /// @return デフォルト値の場合は0、ポインタの場合は負の値、
    ///         正の値の場合はその値を返す
    /// @throw std::out_of_range id2deが空でなく、かつポインタが設定されている場合に
    ///        id2deに存在しないIDが参照されている場合
    /// @note id2deを指定した場合、ポインタの値はid2deに基づいて変換される.
    int GetValue(const id2pointer& = {}) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_DE_DE_COLOR_H_
