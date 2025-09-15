/**
 * @file entities/factory.cpp
 * @brief 各エンティティクラスのファクトリクラス
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/factory.h"

#include <memory>

#include "igesio/entities/structures/unsupported_entity.h"

#include "igesio/entities/structures/null_entity.h"     // type 000
#include "igesio/entities/curves/circular_arc.h"        // type 100
#include "igesio/entities/curves/composite_curve.h"     // type 102
#include "igesio/entities/curves/conic_arc.h"           // type 104
#include "igesio/entities/curves/copious_data.h"        // type 106, forms  1- 3
#include "igesio/entities/curves/linear_path.h"         // type 106, forms 11-13
#include "igesio/entities/curves/line.h"                // type 110

#include "igesio/entities/transformations/transformation_matrix.h"  // type 124
#include "igesio/entities/curves/rational_b_spline_curve.h"         // type 126
#include "igesio/entities/structures/color_definition.h"            // type 314



namespace {

using IVec = igesio::IGESParameterVector;
using ET = igesio::entities::EntityType;
using DE = igesio::entities::RawEntityDE;
using p2I = igesio::entities::pointer2ID;
namespace i_ent = igesio::entities;

}  // namespace



void i_ent::EntityFactory::Initialize() {
    if (initialized_) return;

    // エンティティタイプと作成関数のマッピングを設定
    // 0 - Null Entity
    creators_[ET::kNull] = [](const DE& de, const IVec& p,
                              const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::NullEntity>(de, p, d2i, iid);
    };
    // 100 - Circular Arc
    creators_[ET::kCircularArc] = [](const DE& de, const IVec& p,
                                     const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::CircularArc>(de, p, d2i, iid);
    };
    // 102 - Composite Curve
    creators_[ET::kCompositeCurve] = [](const DE& de, const IVec& p,
                                        const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::CompositeCurve>(de, p, d2i, iid);
    };
    // 104 - Conic Arc
    creators_[ET::kConicArc] = [](const DE& de, const IVec& p,
                                  const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::ConicArc>(de, p, d2i, iid);
    };
    // 106 - Copious Data (CopiousDataBaseとして返す)
    creators_[ET::kCopiousData] = [](const DE& de, const IVec& p,
                                     const p2I& d2i, const uint64_t iid) {
        if (de.form_number <= static_cast<int>(i_ent::CopiousDataType::kSextuples)) {
            return std::static_pointer_cast<i_ent::CopiousDataBase>(
                std::make_shared<i_ent::CopiousData>(de, p, d2i, iid));
        } else if (de.form_number <= static_cast<int>(
                    i_ent::CopiousDataType::kPolylineAndVectors)) {
            return std::static_pointer_cast<i_ent::CopiousDataBase>(
                std::make_shared<i_ent::LinearPath>(de, p, d2i, iid));
        }
        return std::make_shared<i_ent::CopiousDataBase>(de, p, d2i, iid);
    };

    // 110 - Line
    creators_[ET::kLine] = [](const DE& de, const IVec& p,
                              const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::Line>(de, p, d2i, iid);
    };

    // 124 - Transformation Matrix
    creators_[ET::kTransformationMatrix] = [](const DE& de, const IVec& p,
                                              const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::TransformationMatrix>(de, p, d2i, iid);
    };
    // 126 - Rational B-Spline Curve
    creators_[ET::kRationalBSplineCurve] = [](const DE& de, const IVec& p,
                                              const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::RationalBSplineCurve>(de, p, d2i, iid);
    };

    // 314 - Color Definition
    creators_[ET::kColorDefinition] = [](const DE& de, const IVec& p,
                                         const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::ColorDefinition>(de, p, d2i, iid);
    };

    initialized_ = true;
}



std::shared_ptr<i_ent::EntityBase> i_ent::EntityFactory::CreateEntity(
        const RawEntityDE& de, const IVec& parameters,
        const pointer2ID& de2id, const uint64_t iges_id) {
    Initialize();  // 初期化

    auto it = creators_.find(de.entity_type);
    if (it != creators_.end()) {
        return it->second(de, parameters, de2id, iges_id);
    }

    // 対応するエンティティが存在しない場合はUnsupportedEntityを返す
    return std::make_shared<i_ent::UnsupportedEntity>(de, parameters, de2id);
}

std::shared_ptr<i_ent::EntityBase> i_ent::EntityFactory::CreateEntity(
        const RawEntityDE& de, const RawEntityPD& pd,
        const pointer2ID& de2id, const uint64_t iges_id) {
    return CreateEntity(de, ToIGESParameterVector(pd), de2id, iges_id);
}
