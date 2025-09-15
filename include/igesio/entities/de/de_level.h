/**
 * @file entities/de/de_level.h
 * @brief 4th Directory Entryフィールド (Level) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_DE_LEVEL_H_
#define IGESIO_ENTITIES_DE_DE_LEVEL_H_

#include <memory>

#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/de/de_field_wrapper.h"



namespace igesio::entities {

/// @brief レベルフィールドを表すクラス
/// @note DEフィールド5: Level
///       正の値は単一レベル番号、負の値はDefinition Levels Property Entity
///       (Type 406, Form 1)への参照
class DELevel : public DEFieldWrapper<IDefinitionLevelsProperty> {
 private:
    /// @brief レベル番号
    int level_number_ = 0;

 public:
    using DEFieldWrapper<IDefinitionLevelsProperty>::DEFieldWrapper;

    using DEFieldWrapper<IDefinitionLevelsProperty>::GetPointer;

    /// @brief 指定された型のポインタを取得
    /// @return 指定された型のポインタ
    std::shared_ptr<const IDefinitionLevelsProperty> GetPointer() const;

    /// @brief レベル番号を指定するコンストラクタ
    /// @param value レベル番号
    explicit DELevel(const int);

    /// @brief レベル番号を取得する
    /// @return レベル番号 (0以上の整数値)
    int GetLevelNumber() const { return level_number_; }

    /// @brief レベル番号を設定する
    /// @param value レベル番号
    /// @throw std::invalid_argument valueが0未満の場合
    void SetLevelNumber(const int);

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

#endif  // IGESIO_ENTITIES_DE_DE_LEVEL_H_
