/**
 * @file entities/interfaces/de_related.h
 * @brief Directory Entry (DE) セクションのパラメータに関連する
 *        インターフェース定義
 * @author Yayoi Habami
 * @date 2025-07-12
 * @copyright 2025 Yayoi Habami
 * @details このファイルは、Directory Entry (DE) セクションにおける
 *          他のエンティティへのポインタを持ちうるフィールドのための、
 *          インターフェース定義を提供する. 依存性逆転の原則に従い、
 *          `EntityBase`クラスが個別のエンティティクラスの実装を
 *          参照するのではなく、インターフェースを通じて
 *          参照することを目的としている.
 */
#ifndef IGESIO_ENTITIES_INTERFACES_DE_RELATED_H_
#define IGESIO_ENTITIES_INTERFACES_DE_RELATED_H_

#include <memory>
#include <string>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"

namespace igesio::entities {

/// @brief 3rd DE fieldで参照するエンティティ (Type 402, Form 5001-9999
///        or Type 422, Form 0-1) のインターフェース
/// @todo 実装を追加する＆構造は要検討
class IStructure : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~IStructure() = default;
};



/// @brief 4th DE fieldで参照するLine Font Definition Entity
///        (Type 304)のインターフェース
/// @todo 実装を追加する＆構造は要検討
class ILineFontDefinition : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~ILineFontDefinition() = default;
};



/// @brief 5th DE fieldで参照するDefinition Levels Property Entity
///        (Type 406, Form 1)のインターフェース
/// @todo 実装を追加する＆構造は要検討
class IDefinitionLevelsProperty : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~IDefinitionLevelsProperty() = default;
};



/// @brief 6th DE fieldで参照するView Entity (Type 410)のインターフェース
/// @todo 実装を追加する＆構造は要検討
class IView : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~IView() = default;
};



/// @brief 6th DE fieldで参照するViews Visible Associativity Entity
///        (Type 402, Form 3, 4, 19)のインターフェース
/// @todo 実装を追加する＆構造は要検討
class IViewsVisibleAssociativity : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~IViewsVisibleAssociativity() = default;
};



/// @brief Transformation Matrixエンティティのインターフェース
/// @note 他のエンティティに対する変換 (回転・平行移動; PDセクション) と、
///       さらに他の変換行列への参照 (DEセクション 7th param) を持つことができる.
class ITransformation : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~ITransformation() = default;

    /// @brief 回転行列を取得する
    /// @return 3x3の回転行列
    virtual Matrix3d GetRotation() const = 0;
    /// @brief 平行移動ベクトルを取得する
    /// @return 3次元ベクトル (x, y, z) の平行移動量
    virtual Vector3d GetTranslation() const = 0;
    /// @brief 同次変換行列を取得する
    /// @return 4x4の同次変換行列
    virtual Matrix4d GetTransformation() const = 0;

    /// @brief 他の変換行列への参照を設定する
    /// @param transformation 参照先の変換行列
    /// @return 参照の設定に成功した場合は`true`、失敗した場合は`false`.
    ///         循環参照の生じる参照の場合は常に`false`を返す.
    virtual bool SetReference(const std::shared_ptr<ITransformation>&) = 0;
    /// @brief 他の変換行列への参照を設定する
    /// @param transformation 参照先の変換行列
    /// @return 参照の設定に成功した場合は`true`、失敗した場合は`false`.
    ///         循環参照の生じる参照の場合は常に`false`を返す.
    virtual bool SetReference(const std::shared_ptr<const ITransformation>&) = 0;
    /// @brief 他の変換行列への参照を取得する
    /// @return 参照先の変換行列. 参照が設定されていない場合は`nullptr`を返す.
    virtual std::shared_ptr<const ITransformation> GetRefTransformation() const = 0;
};



/// @brief 8th DE fieldで参照するLabel Display Associativity Entity
///        (Type 402, Form 5)のインターフェース
/// @todo 実装を追加する＆構造は要検討
class ILabelDisplayAssociativity : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~ILabelDisplayAssociativity() = default;
};



/// @brief 13th DE fieldで参照するColor Definition Entity (Type 314)のインターフェース
class IColorDefinition : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~IColorDefinition() = default;

    /// @brief 色名を取得する
    /// @return 色名
    virtual std::string GetColorName() const = 0;

    /// @brief RGB値を取得する (0.0〜100.0)
    /// @return RGB値
    virtual std::array<double, 3> GetRGB() const = 0;

    /// @brief CMY値を取得する (0.0〜100.0)
    /// @return CMY値
    std::array<double, 3> GetCMY() const {
        auto rgb = GetRGB();
        return {100.0 - rgb[0], 100.0 - rgb[1], 100.0 - rgb[2]};
    }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_DE_RELATED_H_
