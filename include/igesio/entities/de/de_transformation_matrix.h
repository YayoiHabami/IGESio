/**
 * @file entities/de/de_transformation_matrix.h
 * @brief 7th Directory Entryフィールド (Transformation Matrix) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_DE_TRANSFORMATION_MATRIX_H_
#define IGESIO_ENTITIES_DE_DE_TRANSFORMATION_MATRIX_H_

#include <memory>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/de/de_field_wrapper.h"



namespace igesio::entities {

/// @brief 変換行列フィールドを表すクラス
/// @note DEフィールド7: Transformation Matrix
///       0は単位行列とゼロ平行移動ベクトル、負の値はTransformation Matrix Entity (Type 124)への参照
class DETransformationMatrix : public DEFieldWrapper<ITransformation> {
 public:
    using DEFieldWrapper<ITransformation>::DEFieldWrapper;

    using DEFieldWrapper<ITransformation>::GetPointer;
    /// @brief 指定された型のポインタを取得
    /// @return 指定された型のポインタ
    std::shared_ptr<const ITransformation> GetPointer() const;

    /// @brief 参照するTransformation Matrixの回転行列を取得する
    /// @return 3x3の回転行列、参照先がない場合 (デフォルト) は単位行列を返す
    Matrix3d GetRotation() const;

    /// @brief 参照するTransformation Matrixの平行移動ベクトルを取得する
    /// @return 3次元ベクトル (x, y, z) の平行移動量、
    ///         参照先がない場合はゼロベクトルを返す
    Vector3d GetTranslation() const;

    /// @brief 参照するTransformation Matrixの同次変換行列を取得する
    /// @return 4x4の同次変換行列、参照先がない場合は単位行列を返す
    Matrix4d GetTransformation() const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_DE_DE_TRANSFORMATION_MATRIX_H_
