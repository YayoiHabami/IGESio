/**
 * @file entities/de/de_transformation_matrix.cpp
 * @brief 7th Directory Entryフィールド (Transformation Matrix) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/de_transformation_matrix.h"

#include <memory>

namespace {

namespace i_ent = igesio::entities;
using DETransMatrix = i_ent::DETransformationMatrix;

}  // namespace



std::shared_ptr<const i_ent::ITransformation> DETransMatrix::GetPointer() const {
    return GetPointer<ITransformation>();
}

igesio::Matrix3d DETransMatrix::GetRotation() const {
    auto ptr = GetPointer<ITransformation>();
    if (ptr) {
        return ptr->GetRotation();
    }
    // デフォルトの単位行列を返す
    return Matrix3d::Identity();
}

igesio::Vector3d DETransMatrix::GetTranslation() const {
    auto ptr = GetPointer<ITransformation>();
    if (ptr) {
        return ptr->GetTranslation();
    }
    // デフォルトのゼロベクトルを返す
    return Vector3d::Zero();
}

igesio::Matrix4d DETransMatrix::GetTransformation() const {
    auto ptr = GetPointer<ITransformation>();
    if (ptr) {
        return ptr->GetTransformation();
    }
    // デフォルトの単位行列を返す
    return Matrix4d::Identity();
}
