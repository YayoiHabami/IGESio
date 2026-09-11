/**
 * @file entities/de/de_transformation_matrix.cpp
 * @brief 7th Directory Entryフィールド (Transformation Matrix) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/de_transformation_matrix.h"

#include <memory>
#include <unordered_set>

namespace {

namespace i_ent = igesio::entities;
using DETransMatrix = i_ent::DETransformationMatrix;

/// @brief 変換行列の参照チェーンを合成した実効変換を計算する
/// @param first エンティティが直接参照する変換行列 M1 (nullptrなら単位行列)
/// @return E = Mn ··· M2 M1. M1が先に適用され、M1のDE第7欄が指すM2が
///         その結果に適用される (IGES 5.3 §3.2.3 explicit case)
/// @note 不正なファイル由来の循環参照で無限ループしないよう、訪問済みの
///       IDに戻った時点で打ち切る
igesio::Matrix4d ComposeChain(
        const std::shared_ptr<const i_ent::ITransformation>& first) {
    igesio::Matrix4d effective = igesio::Matrix4d::Identity();
    std::unordered_set<igesio::ObjectID> visited;
    auto current = first;
    while (current) {
        if (!visited.insert(current->GetID()).second) break;
        effective = current->GetTransformation() * effective;
        current = current->GetRefTransformation();
    }
    return effective;
}

}  // namespace



std::shared_ptr<const i_ent::ITransformation> DETransMatrix::GetPointer() const {
    return GetPointer<ITransformation>();
}

igesio::Matrix3d DETransMatrix::GetRotation() const {
    return ComposeChain(GetPointer<ITransformation>()).block<3, 3>(0, 0);
}

igesio::Vector3d DETransMatrix::GetTranslation() const {
    return ComposeChain(GetPointer<ITransformation>()).block<3, 1>(0, 3);
}

igesio::Matrix4d DETransMatrix::GetTransformation() const {
    return ComposeChain(GetPointer<ITransformation>());
}
