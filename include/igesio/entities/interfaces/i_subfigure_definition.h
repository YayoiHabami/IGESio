/**
 * @file entities/interfaces/i_subfigure_definition.h
 * @brief Subfigure Definition entity (Type 308) のインターフェース定義
 * @author Yayoi Habami
 * @date 2025-10-15
 * @copyright 2025 Yayoi Habami
 * @note Point (type 116) が子要素としてSubfigure Definitionを参照するが、
 *       Subfigure DefinitionもPointを子要素として持つ可能性があるため、
 *       インターフェースを追加し、そのポインタとして持つように設計した.
 */
#ifndef IGESIO_ENTITIES_INTERFACES_I_SUBFIGURE_DEFINITION_H_
#define IGESIO_ENTITIES_INTERFACES_I_SUBFIGURE_DEFINITION_H_

#include <string>

#include "igesio/entities/interfaces/i_entity_identifier.h"



namespace igesio::entities {

/// @brief Subfigure Definition entity (Type 308) のインターフェース
/// @todo 実装を追加する＆構造は要検討
class ISubfigureDefinition : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~ISubfigureDefinition() = default;

    /// @brief ネストの深さを取得する
    /// @return DEPTH
    virtual int GetDepth() const = 0;

    /// @brief Subfigureの名前を取得する
    /// @return Subfigureの名前
    virtual std::string GetName() const = 0;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_SUBFIGURE_DEFINITION_H_
