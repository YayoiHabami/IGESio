/**
 * @file entities/de/de_color.h
 * @brief 13th Directory Entryフィールド (Color) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_DE_COLOR_H_
#define IGESIO_ENTITIES_DE_DE_COLOR_H_

#include <memory>

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
    /// @throw std::invalid_argument valueが無効な場合 (0未満または8より大きい)
    explicit DEColor(const int);

    /// @brief 色を指定するコンストラクタ
    /// @param value 色の値
    explicit DEColor(const ColorNumber);

    /// @brief 色 (RGB) を取得する
    /// @return RGB値の配列 (それぞれ0.0〜100.0)
    std::array<double, 3> GetRGB() const;

    /// @brief 色 (CMY) を取得する
    /// @return CMY値の配列 (それぞれ0.0〜100.0)
    std::array<double, 3> GetCMY() const {
        auto [r, g, b] = GetRGB();
        return {100 - r, 100 - g, 100 - b};
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
