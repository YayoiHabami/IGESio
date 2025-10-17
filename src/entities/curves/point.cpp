/**
 * @file entities/curves/point.cpp
 * @brief Point (Type 116): 点エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/point.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace {

namespace i_ent = igesio::entities;
using i_ent::Point;
using igesio::Vector3d;

}  // namespace



/**
 * コンストラクタ
 */

Point::Point(const RawEntityDE& de_record, const IGESParameterVector& parameters,
             const pointer2ID& de2id, const uint64_t iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id),
          subfigure_(kUnsetID) {
    InitializePD(de2id);
}

Point::Point(const IGESParameterVector& parameters)
        : Point(RawEntityDE::ByDefault(i_ent::EntityType::kPoint),
                parameters, {}, kUnsetID) {}

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
    uint64_t subfigure_id;
    if (pd.access_as<int>(3) == 0) {
        // 0の場合、Subfigure Definitionへの参照は指定されない
        subfigure_id = kUnsetID;
    } else if (!de2id.empty()) {
        // de2idが空でない場合は、取得した数値をde2idでIDに変換
        auto id_int = static_cast<unsigned int>(pd.access_as<int>(3));
        if (de2id.find(id_int) == de2id.end()) {
            throw std::out_of_range("Subfigure Definition (ID " + std::to_string(id_int)
                                    + ") not found in DE to ID mapping.");
        }
        subfigure_id = de2id.at(id_int);
    } else {
        // de2idが空の場合は、そのままIDとして使用
        subfigure_id = static_cast<uint64_t>(pd.access_as<int>(3));
    }
    subfigure_ = PointerContainer<false, ISubfigureDefinition>(subfigure_id);

    return 4;
}

std::unordered_set<uint64_t> Point::GetUnresolvedPDReferences() const {
    std::unordered_set<uint64_t> unresolved;

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

std::vector<uint64_t> Point::GetChildIDs() const {
    std::vector<uint64_t> ids;
    ids.push_back(subfigure_.GetID());
    return ids;
}

std::shared_ptr<const i_ent::EntityBase>
Point::GetChildEntity(const uint64_t id) const {
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
    if (subfigure_.GetID() != kUnsetID) {
        if (!subfigure_.IsPointerSet()) {
            // 参照が存在するはずだが、指定されていない場合
            errors.emplace_back("Subfigure Definition (ID " +
                                std::to_string(subfigure_.GetID()) +
                                ") is referenced but not set.");
        } else {
            // 参照が存在する場合
            auto result = (*subfigure_.TryGetEntity<EntityBase>())->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Subfigure Definition (ID " +
                                    std::to_string(subfigure_.GetID()) +
                                    ") is invalid: " + result.Message());
            }
        }
    }

    return MakeValidationResult(errors);
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
