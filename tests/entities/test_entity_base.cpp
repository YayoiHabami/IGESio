/**
 * @file entities/test_entity_base.cpp
 * @brief entities/entity_base.h & entities/structure/unsupported_entity.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-15
 * @copyright 2025 Yayoi Habami
 * @note このファイルでは、EntityBaseの基本的な機能を確認する.
 *       EntityBase自体は抽象クラスであるため、検証にはUnsupportedEntityを使用する.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/pd.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/structures/unsupported_entity.h"
#include "igesio/reader.h"

namespace {

namespace i_ent = igesio::entities;

/// @brief IGESのDEとPDのレコード
std::pair<std::vector<i_ent::RawEntityDE>, std::vector<i_ent::RawEntityPD>>
GetTestRecords() {
    auto source_dir = std::filesystem::absolute(__FILE__).parent_path()
                      .parent_path() / "test_data";
    auto source_path = (source_dir / "single_rounded_cube.iges").string();

    auto data = igesio::ReadIgesIntermediate(source_path);
    auto& pd = data.parameter_data_section;
    auto& de = data.directory_entry_section;

    EXPECT_EQ(pd.size(), de.size());

    return {de, pd};
}

/// @brief エンティティのリスト
std::vector<std::shared_ptr<i_ent::EntityBase>> GetEntities() {
    auto [de_records, pd_records] = GetTestRecords();
    uint64_t iges_id = igesio::IDGenerator::Generate();

    // IDを予約する
    i_ent::pointer2ID de2id;
    for (const auto& de : de_records) {
        auto id = igesio::IDGenerator::Reserve(iges_id, de.sequence_number);
        de2id.emplace(de.sequence_number, id);
    }

    std::vector<std::shared_ptr<i_ent::EntityBase>> entities;
    for (size_t i = 0; i < de_records.size(); ++i) {
        auto& de = de_records[i];
        auto pd = i_ent::ToIGESParameterVector(pd_records[i]);

        entities.push_back(std::make_shared<i_ent::UnsupportedEntity>(
                de, pd, de2id, iges_id));
        EXPECT_EQ(entities.back()->GetID(), de2id[de.sequence_number])
            << "Entity ID does not match DE sequence number: "
            << de.sequence_number << " vs " << entities.back()->GetID();
    }

    return entities;
}

/* 上のテスト用IGESデータは以下のようようもの
102 entities:
  Surface of Revolution Entity (type#120): 1
  Trimmed Surface Entity (type#144): 7
  Curve on a Parametric Surface Entity (type#142): 7
  Transformation Matrix Entity (type#124): 4
  Line Entity (type#110): 28
  Circular Arc Entity (type#100): 4
  Rational B-Spline Curve Entity (type#126): 30
  Composite Curve Entity (type#102): 14
  Rational B-Spline Surface Entity (type#128): 6
  Color Definition Entity (type#314): 1
*/

}  // namespace



// 複数のタイプのエンティティをエラーなく作成できることを確認する
TEST(EntityBaseTest, Constructor) {
    auto [de_records, pd_records] = GetTestRecords();

    for (size_t i = 0; i < de_records.size(); ++i) {
        auto& de = de_records[i];
        auto pd = i_ent::ToIGESParameterVector(pd_records[i]);

        // UnsupportedEntityのインスタンスがエラーなく作成できることを確認
        EXPECT_NO_THROW({
            i_ent::UnsupportedEntity entity(de, pd);
        });
    }
}



/**
 * IEntityIdentifier implementation
 */

// エンティティのタイプが正しく取得できることを確認する
TEST(EntityBaseTest, GetType) {
    auto [de_records, pd_records] = GetTestRecords();

    std::unordered_map<i_ent::EntityType, size_t> type_count;
    for (size_t i = 0; i < de_records.size(); ++i) {
        auto& de = de_records[i];
        auto pd = i_ent::ToIGESParameterVector(pd_records[i]);

        i_ent::UnsupportedEntity entity(de, pd);
        auto type = entity.GetType();
        type_count[type]++;
    }

    // 各エンティティタイプの出現回数を確認
    EXPECT_EQ(type_count[i_ent::EntityType::kSurfaceOfRevolution], 1);
    EXPECT_EQ(type_count[i_ent::EntityType::kTrimmedSurface], 7);
    EXPECT_EQ(type_count[i_ent::EntityType::kCurveOnAParametricSurface], 7);
    EXPECT_EQ(type_count[i_ent::EntityType::kTransformationMatrix], 4);
    EXPECT_EQ(type_count[i_ent::EntityType::kLine], 28);
    EXPECT_EQ(type_count[i_ent::EntityType::kCircularArc], 4);
    EXPECT_EQ(type_count[i_ent::EntityType::kRationalBSplineCurve], 30);
    EXPECT_EQ(type_count[i_ent::EntityType::kCompositeCurve], 14);
    EXPECT_EQ(type_count[i_ent::EntityType::kRationalBSplineSurface], 6);
    EXPECT_EQ(type_count[i_ent::EntityType::kColorDefinition], 1);
}



/**
 * パラメータの検証
 */



/**
 * Directory Entry (DE) フィールドの操作 (EntityType, FormNumberを除く)
 */



/**
 * Parameter Data (PD) フィールドの操作
 */

// AreAllReferencesSetのテスト
// GetReferencedEntityIDs()が空ではなく、GetUnresolvedReferences()が空ではない
// エンティティについては、AreAllReferencesSet()がfalseを返すことを確認する
TEST(EntityBaseTest, AreAllReferencesSet) {
    auto entities = GetEntities();

    for (const auto& entity : entities) {
        // GetReferencedEntityIDs()が空ではないことを確認
        if (entity->GetReferencedEntityIDs().empty()) continue;
        // GetUnresolvedReferences()が空ではないことを確認
        if (entity->GetUnresolvedReferences().empty()) continue;

        // AreAllReferencesSet()がfalseを返すことを確認
        EXPECT_FALSE(entity->AreAllReferencesSet());
    }
}

// GetUnresolvedReferencesのテスト
// すべてのCircularArcエンティティが未解決の参照を持つこと、そのIDが
// TransformationMatrixエンティティのIDと一致することを確認する
TEST(EntityBaseTest, GetUnresolvedReferences) {
    auto entities = GetEntities();

    // TransformationMatrixエンティティのIDを取得
    std::vector<uint64_t> trans_ids;
    std::string trans_ids_str;
    for (const auto& entity : entities) {
        if (entity->GetType() == i_ent::EntityType::kTransformationMatrix) {
            trans_ids.push_back(entity->GetID());
            trans_ids_str += std::to_string(entity->GetID()) + ",";
        }
    }

    // CircularArcエンティティがTransformationMatrixエンティティを参照していることを確認
    for (const auto& entity : entities) {
        if (entity->GetType() != i_ent::EntityType::kCircularArc) {
            continue;
        }

        auto unresolved_refs = entity->GetUnresolvedReferences();
        EXPECT_TRUE(unresolved_refs.size() == 1)
            << "Entity ID: " << entity->GetID() << " has no unresolved references"
            << " (expected 1, actual " << unresolved_refs.size() << ")";

        // 参照するIDを検索
        auto id_it = unresolved_refs.begin();
        EXPECT_TRUE(id_it != unresolved_refs.end())
            << "Entity ID: " << entity->GetID() << " has no unresolved references";
        if (id_it != unresolved_refs.end()) {
            auto it = std::find(trans_ids.begin(), trans_ids.end(), *id_it);
            EXPECT_TRUE(it != trans_ids.end())
                << "Entity ID: " << entity->GetID() << " references an invalid "
                "TransformationMatrix ID" << " (" << trans_ids_str << ")";
        }
    }
}

// SetUnresolvedReferenceのテスト
// CircularArcエンティティにTransformation Matrixのインスタンスを渡し、
// GetUnresolvedReferencesが空にならないことを確認する
//   -> `ITransformation`を継承したクラスである必要があるため
TEST(EntityBaseTest, SetUnresolvedReferenceWithInvalidBase) {
    auto entities = GetEntities();

    // TransformationMatrixエンティティのポインタを取得
    std::unordered_map<uint64_t, std::shared_ptr<const i_ent::EntityBase>>
            trans_map;
    for (const auto& entity : entities) {
        if (entity->GetType() == i_ent::EntityType::kTransformationMatrix) {
            trans_map[entity->GetID()] = entity;
        }
    }

    // CircularArcエンティティにTransformationMatrixエンティティのポインタを設定
    // ただし、TransformationMatrixのインスタンスではなくUnsupportedEntityを
    // 渡すため、ポインタは設定されない
    for (const auto& entity : entities) {
        if (entity->GetType() != i_ent::EntityType::kCircularArc) {
            continue;
        }

        for (const auto& [trans_id, trans_entity] : trans_map) {
            EXPECT_FALSE(entity->SetUnresolvedReference(trans_entity));
        }
    }

    // その後GetUnresolvedReferencesが空にならない
    for (const auto& entity : entities) {
        if (entity->GetType() != i_ent::EntityType::kCircularArc) {
            continue;
        }

        EXPECT_FALSE(entity->GetUnresolvedReferences().empty());
    }
}
