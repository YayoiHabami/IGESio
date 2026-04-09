/**
 * @file entities/curves/point.cpp
 * @brief Point (Type 116): 点エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/point.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::Point;
using igesio::Vector3d;

}  // namespace



/**
 * コンストラクタ
 */

Point::Point(const RawEntityDE& de_record, const IGESParameterVector& parameters,
             const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id),
          subfigure_(IDGenerator::UnsetID()) {
    InitializePD(de2id);
}

Point::Point(const IGESParameterVector& parameters)
        : Point(RawEntityDE::ByDefault(i_ent::EntityType::kPoint),
                parameters, {}, IDGenerator::UnsetID()) {}

Point::Point(const Vector3d& position)
       : Point(IGESParameterVector{position[0], position[1], position[2], 0}) {}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector
Point::GetMainPDParameters() const {
    return {
        position_[0], position_[1], position_[2],
        subfigure_.GetID()
    };
}

size_t Point::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;

    if (pd.size() < 3) {
        throw igesio::DataFormatError("Point must have at least 3 parameters.");
    } else if (pd.size() == 3) {
        // subfigureが未定義
        return 3;
    }

    // position
    position_[0] = pd.access_as<double>(0);
    position_[1] = pd.access_as<double>(1);
    position_[2] = pd.access_as<double>(2);

    // subfigure
    try {
        subfigure_ = PointerContainer<false, ISubfigureDefinition>(
                GetObjectIDFromParameters(pd, 3, de2id, true));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError(
            "Failed to set Subfigure Definition pointer: " + std::string(e.what()));
    }

    return 4;
}

std::unordered_set<igesio::ObjectID> Point::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> unresolved;

    if (!subfigure_.IsPointerSet()) {
        unresolved.insert(subfigure_.GetID());
    }
    return unresolved;
}

bool Point::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    // 指定されたエンティティがnullptrの場合は失敗
    if (!entity) {
        return false;
    }

    if (entity->GetID() == subfigure_.GetID()) {
        if (!subfigure_.IsPointerSet()) {
            // ポインタが未設定の場合のみ設定
            if (auto ptr = std::dynamic_pointer_cast<const ISubfigureDefinition>(entity)) {
                return subfigure_.SetPointer(ptr);
            }
        }
    }

    // 指定されたエンティティと同一のIDを持つ参照がない場合
    return false;
}

std::vector<igesio::ObjectID> Point::GetChildIDs() const {
    std::vector<ObjectID> ids;
    ids.push_back(subfigure_.GetID());
    return ids;
}

std::shared_ptr<const i_ent::EntityBase>
Point::GetChildEntity(const ObjectID& id) const {
    if (subfigure_.GetID() == id) {
        auto ptr = subfigure_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    }

    // 指定されたIDを持つ子エンティティが存在しない場合
    return nullptr;
}

igesio::ValidationResult Point::ValidatePD() const {
    std::vector<ValidationError> errors;

    // subfigure
    if (subfigure_.GetID().IsSet()) {
        if (!subfigure_.IsPointerSet()) {
            // 参照が存在するはずだが、指定されていない場合
            errors.emplace_back("Subfigure Definition (ID " +
                                ToString(subfigure_.GetID()) +
                                ") is referenced but not set.");
        } else {
            // 参照が存在する場合
            auto result = (*subfigure_.TryGetEntity<EntityBase>())->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Subfigure Definition (ID " +
                                    ToString(subfigure_.GetID()) +
                                    ") is invalid: " + result.Message());
            }
        }
    }

    return MakeValidationResult(errors);
}



/**
 * IGeometry implementation
 */

i_num::BoundingBox Point::GetDefinedBoundingBox() const {
    auto bbox = numerics::BoundingBox();
    bbox.SetControl(position_);
    return bbox;
}



/**
 * 要素の変更・取得
 */

void Point::SetSubfigure(const std::shared_ptr<ISubfigureDefinition>& subfigure) {
    if (subfigure == nullptr) {
        throw std::invalid_argument("subfigure of Point must not be nullptr");
    }
    if (subfigure_.GetID() == subfigure->GetID()) {
        subfigure_.SetPointer(subfigure);
    } else {
        subfigure_.OverwritePointer(subfigure);
    }

    // SubordinateEntitySwitchをkPhysicallyDependentに設定
    // subfigureのさらに子・孫要素については、すでに従属状態に設定されているものとする
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(subfigure)) {
        entity_base->SetSubordinateEntitySwitch(
            SubordinateEntitySwitch::kPhysicallyDependent);
    }
}

std::shared_ptr<const i_ent::ISubfigureDefinition> Point::GetSubfigure() const {
    auto ptr = subfigure_.TryGetEntity<i_ent::ISubfigureDefinition>();
    if (!ptr) {
        throw std::runtime_error("Pointer of Subfigure Definition entity "
                                 "is not set or invalid.");
    }
    return ptr.value();
}
