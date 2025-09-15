/**
 * @file entities/de/de_line_font_pattern.h
 * @brief 4th Directory Entryフィールド (Line Font Pattern) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_DE_LINE_FONT_PATTERN_H_
#define IGESIO_ENTITIES_DE_DE_LINE_FONT_PATTERN_H_

#include <memory>

#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/de/de_field_wrapper.h"



namespace igesio::entities {

/// @brief 線種パターンの種類 (4th field of DE; Line Font Pattern)
/// @note IGESファイルにおけるエンティティの表示パターンを定義
enum class LineFontPattern {
    /// @brief パターン未指定（デフォルト）
    kNoPattern = 0,
    /// @brief 実線
    kSolid = 1,
    /// @brief 破線
    kDashed = 2,
    /// @brief 一点鎖線
    kPhantom = 3,
    /// @brief 中心線
    kCenterline = 4,
    /// @brief 点線
    kDotted = 5
};

/// @brief 線種パターンフィールドを表すクラス
/// @note DEフィールド4: Line Font Pattern
///       正の値は標準パターン、負の値はLine Font Definition Entity (Type 304)への参照
class DELineFontPattern : public DEFieldWrapper<ILineFontDefinition> {
 private:
    /// @brief 線種パターン
    LineFontPattern pattern_ = LineFontPattern::kNoPattern;

 public:
    using DEFieldWrapper<ILineFontDefinition>::DEFieldWrapper;

    using DEFieldWrapper<ILineFontDefinition>::GetPointer;

    /// @brief 指定された型のポインタを取得
    /// @return 指定された型のポインタ
    std::shared_ptr<const ILineFontDefinition> GetPointer() const;

    /// @brief パターン値を指定するコンストラクタ
    /// @param value 線種パターンの値
    /// @throw std::invalid_argument valueが無効な場合 (0以下または6以上)
    explicit DELineFontPattern(const int);

    /// @brief パターン値を指定するコンストラクタ
    /// @param value 線種パターンの値
    explicit DELineFontPattern(const LineFontPattern);

    /// @brief 線種パターンを取得する
    /// @return 線種パターン
    LineFontPattern GetPattern() const { return pattern_; }

    /// @brief 線種パターンを設定する
    /// @param value 線種パターンの値
    void SetPattern(const LineFontPattern);

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

#endif  // IGESIO_ENTITIES_DE_DE_LINE_FONT_PATTERN_H_
