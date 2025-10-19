/**
 * @file common/test_id_generator.h
 * @brief IDGeneratorのテスト
 * @author Yayoi Habami
 * @date 2025-06-02
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <unordered_set>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "igesio/common/id_generator.h"

namespace {

using igesio::IDGenerator;
using igesio::ObjectType;
using igesio::ObjectID;

/// @brief IgedData用のObjectIDを生成する
ObjectID GetIgesDataID() {
    return IDGenerator::Generate(ObjectType::kIgesData);
}
/// @brief Assembly用のObjectIDを生成する
ObjectID GetAssemblyID() {
    return IDGenerator::Generate(ObjectType::kAssembly);
}
/// @brief Entity (in program)用のObjectIDを生成する
/// @param entity_type エンティティのタイプ (念のため実際に存在する値を指定すること)
ObjectID GetEntityNewID(const uint16_t entity_type) {
    return IDGenerator::Generate(ObjectType::kEntityNew, entity_type);
}
/// @brief EntityGraphics用のObjectIDを生成する
/// @param entity_type エンティティのタイプ (念のため実際に存在する値を指定すること)
ObjectID GetEntityGraphicsID(const uint16_t entity_type) {
    return IDGenerator::Generate(ObjectType::kEntityGraphics, entity_type);
}
/// @brief Entity (from IGES)用のObjectIDを予約する
/// @param entity_type エンティティのタイプ (念のため実際に存在する値を指定すること)
/// @param de_pointer DEレコードのシーケンス番号
/// @param iges_id 読み込み元のIGESファイルに対応するIgesDataのObjectID
ObjectID GetEntityFID(const uint16_t entity_type,
                      const unsigned int de_pointer,
                      const ObjectID& iges_id) {
    return IDGenerator::Reserve(iges_id, entity_type, de_pointer);
}

/// @brief nullptrではないIdentifierへの共有ポインタを取得する
std::shared_ptr<const igesio::Identifier> ToId(const ObjectID& obj_id) {
    auto id_ptr = obj_id.GetIdentifier();
    EXPECT_NE(id_ptr, nullptr);
    return id_ptr;
}

/// @brief uint64_tのペアのハッシュ関数
struct PairHash {
    std::size_t operator()(const std::pair<uint64_t, uint64_t>& p) const {
        auto h1 = std::hash<uint64_t>{}(p.first);
        auto h2 = std::hash<uint64_t>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

}  // namespace



/**
 * ObjectTypeのテスト
 */

// ObjectTypeのToStringテスト
TEST(IDGeneratorTest, ObjectTypeToStringTest) {
    std::unordered_set<std::string> unique_strings;
    for (int i = 0; i <= 5; ++i) {
        auto object_type = static_cast<ObjectType>(i);
        auto str = igesio::ToString(object_type);
        EXPECT_TRUE(unique_strings.insert(str).second)
            << "Duplicate string found for ObjectType: " << i;
    }
}



/**
 * Identifierのテスト
 * (内部的に実装されたIdentifierImplのテストも兼ねる)
 */

// GetUniqueID, GetIDPrefix, GetIDSuffix, GetIntIDのテスト
TEST(IDGeneratorTest, IdentifierGettersTest) {
    auto id_ptr = ToId(GetEntityNewID(126));

    auto unique_id = id_ptr->GetUniqueID();
    EXPECT_EQ(unique_id.first, id_ptr->GetIDPrefix());
    EXPECT_EQ(unique_id.second, id_ptr->GetIDSuffix());
    EXPECT_NE(id_ptr->GetIntID(), igesio::kInvalidIntID);

    // 異なるIDを生成して、それぞれ異なる値が返されることを確認
    auto iges_id = GetIgesDataID();
    std::vector<std::shared_ptr<const igesio::Identifier>> ids = {
        ToId(GetEntityNewID(126)), ToId(GetEntityNewID(128)),
        ToId(GetEntityGraphicsID(126)), ToId(GetEntityGraphicsID(128)),
        ToId(GetIgesDataID()), ToId(GetIgesDataID()),
        ToId(GetAssemblyID()), ToId(GetAssemblyID()),
        ToId(GetEntityFID(126, 1, iges_id)), ToId(GetEntityFID(126, 3, iges_id))
    };
    std::unordered_set<std::pair<uint64_t, uint64_t>, PairHash> unique_ids;
    std::unordered_set<int> unique_int_ids;
    for (const auto& id : ids) {
        // idが一意であることを確認
        auto uid = id->GetUniqueID();
        EXPECT_TRUE(unique_ids.insert(uid).second)
            << "Duplicate UniqueID found: (" << uid.first << ", " << uid.second << ")";
        // int型IDが一意であることを確認
        auto int_id = id->GetIntID();
        EXPECT_TRUE(unique_int_ids.insert(int_id).second)
            << "Duplicate IntID found: " << int_id;

        // prefixとsuffixが正しいことを確認
        EXPECT_EQ(uid.first, id->GetIDPrefix());
        EXPECT_EQ(uid.second, id->GetIDSuffix());
    }
}

// GetObjectType, GetIGESIntID, GetDEPointer, GetEntityType, GetTimestampのテスト
TEST(IDGeneratorTest, IdentifierMetadataTest) {
    auto entity_new_id = ToId(GetEntityNewID(126));
    EXPECT_EQ(entity_new_id->GetObjectType(), ObjectType::kEntityNew);
    EXPECT_EQ(entity_new_id->GetIGESIntID(), std::nullopt);
    EXPECT_EQ(entity_new_id->GetDEPointer(), std::nullopt);
    EXPECT_EQ(entity_new_id->GetEntityType(), 126);
    auto timestamp1 = entity_new_id->GetTimestamp();

    auto entity_graphics_id = ToId(GetEntityGraphicsID(128));
    EXPECT_EQ(entity_graphics_id->GetObjectType(), ObjectType::kEntityGraphics);
    EXPECT_EQ(entity_graphics_id->GetIGESIntID(), std::nullopt);
    EXPECT_EQ(entity_graphics_id->GetDEPointer(), std::nullopt);
    EXPECT_EQ(entity_graphics_id->GetEntityType(), 128);
    auto timestamp2 = entity_graphics_id->GetTimestamp();

    auto iges_data_id = ToId(GetIgesDataID());
    EXPECT_EQ(iges_data_id->GetObjectType(), ObjectType::kIgesData);
    EXPECT_EQ(iges_data_id->GetIGESIntID(), std::nullopt);
    EXPECT_EQ(iges_data_id->GetDEPointer(), std::nullopt);
    EXPECT_EQ(iges_data_id->GetEntityType(), std::nullopt);
    auto timestamp3 = iges_data_id->GetTimestamp();

    auto assembly_id = ToId(GetAssemblyID());
    EXPECT_EQ(assembly_id->GetObjectType(), ObjectType::kAssembly);
    EXPECT_EQ(assembly_id->GetIGESIntID(), std::nullopt);
    EXPECT_EQ(assembly_id->GetDEPointer(), std::nullopt);
    EXPECT_EQ(assembly_id->GetEntityType(), std::nullopt);
    auto timestamp4 = assembly_id->GetTimestamp();

    auto general_iges_id = GetIgesDataID();
    auto entity_fid = ToId(GetEntityFID(126, 42, general_iges_id));
    EXPECT_EQ(entity_fid->GetObjectType(), ObjectType::kEntityFromIGES);
    EXPECT_EQ(entity_fid->GetIGESIntID(), general_iges_id.ToInt());
    EXPECT_EQ(entity_fid->GetDEPointer(), 42);
    EXPECT_EQ(entity_fid->GetEntityType(), 126);
    auto timestamp5 = entity_fid->GetTimestamp();

    // 各タイムスタンプが現在時刻以前かつ
    // timestamp1 <= timestamp2 <= ... となっていることを確認
    auto now = std::chrono::system_clock::now();
    EXPECT_LE(timestamp1, now);
    EXPECT_LE(timestamp2, now);
    EXPECT_LE(timestamp3, now);
    EXPECT_LE(timestamp4, now);
    EXPECT_LE(timestamp5, now);
    EXPECT_LE(timestamp1, timestamp2);
    EXPECT_LE(timestamp2, timestamp3);
    EXPECT_LE(timestamp3, timestamp4);
    EXPECT_LE(timestamp4, timestamp5);
}

// Identifierの等価比較演算子・不等価比較演算子のテスト
TEST(IDGeneratorTest, IdentifierComparisonOperatorsTest) {
    auto id1 = ToId(GetEntityNewID(126));
    auto id2 = ToId(GetEntityNewID(126));
    auto id3 = ToId(GetEntityGraphicsID(126));

    // 同じIdentifier同士の比較
    EXPECT_TRUE(*id1 == *id1);
    EXPECT_FALSE(*id1 != *id1);

    // 異なるIdentifier同士の比較
    EXPECT_FALSE(*id1 == *id2);
    EXPECT_TRUE(*id1 != *id2);
    EXPECT_FALSE(*id1 == *id3);
    EXPECT_TRUE(*id1 != *id3);
}



/**
 * ObjectIDのテスト
 */

// コンストラクタのテスト
TEST(IDGeneratorTest, ObjectIDConstructorTest) {
    auto id_ptr = ToId(GetEntityNewID(126));
    igesio::ObjectID obj_id(id_ptr);

    // GetIdentifierで正しいIdentifierの共有ポインタが取得できることを確認
    auto retrieved_id_ptr = obj_id.GetIdentifier();
    EXPECT_EQ(retrieved_id_ptr, id_ptr);

    // nullptrで初期化した場合のテスト
    igesio::ObjectID null_obj_id(nullptr);
    EXPECT_EQ(null_obj_id.GetIdentifier(), nullptr);
}

// ToIntのテスト
TEST(IDGeneratorTest, ObjectIDToIntTest) {
    auto entity_new_id = GetEntityNewID(126);
    EXPECT_NE(entity_new_id.ToInt(), igesio::kInvalidIntID);
    EXPECT_EQ(ToId(entity_new_id)->GetIntID(), entity_new_id.ToInt());

    igesio::ObjectID unset_id = IDGenerator::UnsetID();
    EXPECT_EQ(unset_id.ToInt(), igesio::kInvalidIntID);

    auto null_id = igesio::ObjectID(nullptr);
    EXPECT_EQ(null_id.ToInt(), igesio::kInvalidIntID);
}

// IsSetのテスト
TEST(IDGeneratorTest, ObjectIDIsSetTest) {
    auto entity_new_id = GetEntityNewID(126);
    EXPECT_TRUE(entity_new_id.IsSet());

    igesio::ObjectID unset_id = IDGenerator::UnsetID();
    EXPECT_FALSE(unset_id.IsSet());

    auto null_id = igesio::ObjectID(nullptr);
    EXPECT_FALSE(null_id.IsSet());
}

// ObjectID同士の等価比較演算子・不等価比較演算子のテスト
TEST(IDGeneratorTest, ObjectIDComparisonOperatorsTest) {
    auto id1 = GetEntityNewID(126);
    auto id2 = GetEntityNewID(126);
    auto id3 = GetEntityGraphicsID(126);
    igesio::ObjectID null_id(nullptr);
    igesio::ObjectID unset_id = IDGenerator::UnsetID();

    // 同じObjectID同士の比較
    EXPECT_TRUE(id1 == id1);
    EXPECT_FALSE(id1 != id1);

    // 異なるObjectID同士の比較
    EXPECT_FALSE(id1 == id2);
    EXPECT_TRUE(id1 != id2);
    EXPECT_FALSE(id1 == id3);
    EXPECT_TRUE(id1 != id3);

    // nullptr ObjectIDとの比較
    EXPECT_FALSE(id1 == null_id);
    EXPECT_TRUE(id1 != null_id);
    EXPECT_TRUE(null_id == null_id);
    EXPECT_FALSE(null_id != null_id);

    // UnsetIDとの比較
    EXPECT_FALSE(id1 == unset_id);
    EXPECT_TRUE(id1 != unset_id);
    EXPECT_TRUE(unset_id == unset_id);
    EXPECT_FALSE(unset_id != unset_id);

    // nullptr ObjectIDとUnsetIDの比較
    EXPECT_TRUE(null_id == unset_id);
    EXPECT_FALSE(null_id != unset_id);
}

// ObjectIDとIdentifier共有ポインタの等価比較演算子・不等価比較演算子のテスト
TEST(IDGeneratorTest, ObjectIDAndIdentifierComparisonOperatorsTest) {
    auto id1 = GetEntityNewID(126);
    auto id2 = GetEntityNewID(126);
    auto id3 = GetEntityGraphicsID(126);
    auto id1_ptr = ToId(id1);
    auto id2_ptr = ToId(id2);
    auto id3_ptr = ToId(id3);
    igesio::ObjectID null_id(nullptr);
    igesio::ObjectID unset_id = IDGenerator::UnsetID();

    // ObjectIDと同じIdentifier共有ポインタの比較
    EXPECT_TRUE(id1 == id1_ptr);
    EXPECT_FALSE(id1 != id1_ptr);

    // ObjectIDと異なるIdentifier共有ポインタの比較
    EXPECT_FALSE(id1 == id2_ptr);
    EXPECT_TRUE(id1 != id2_ptr);
    EXPECT_FALSE(id1 == id3_ptr);
    EXPECT_TRUE(id1 != id3_ptr);

    // nullptr Identifier共有ポインタとの比較
    EXPECT_FALSE(id1 == nullptr);
    EXPECT_TRUE(id1 != nullptr);
    EXPECT_TRUE(null_id == nullptr);
    EXPECT_FALSE(null_id != nullptr);

    // UnsetIDとの比較
    EXPECT_FALSE(unset_id == id1_ptr);
    EXPECT_TRUE(unset_id != id1_ptr);
    EXPECT_TRUE(unset_id == nullptr);
    EXPECT_FALSE(unset_id != nullptr);

    // nullptr Identifier共有ポインタとUnsetIDの比較
    EXPECT_TRUE(null_id == nullptr);
    EXPECT_FALSE(null_id != nullptr);
}



/**
 * IDGeneratorのテスト
 */

// 基本的なID生成のテスト
TEST(IDGeneratorTest, BasicIDGeneration) {
    auto id1 = IDGenerator::Generate(ObjectType::kEntityNew, 126);
    auto id2 = IDGenerator::Generate(ObjectType::kEntityNew, 126);

    // 生成された各IDが、常にUnsetIDとは異なり、かつ一意であることを確認
    EXPECT_NE(id1, IDGenerator::UnsetID());
    EXPECT_NE(id2, IDGenerator::UnsetID());
    EXPECT_NE(id2, id1);
}

// ID予約機能のテスト
TEST(IDGeneratorTest, ReserveID) {
    auto iges_id = IDGenerator::Generate(ObjectType::kIgesData);
    unsigned int pd_pointer = 200;

    auto reserved_id = IDGenerator::Reserve(iges_id, 128, pd_pointer);
    EXPECT_NE(reserved_id, IDGenerator::UnsetID());

    // 同じキーで再度予約すると、同じIDが返される
    auto same_id = IDGenerator::Reserve(iges_id, 128, pd_pointer);
    EXPECT_EQ(reserved_id, same_id);

    // 非IgesDataのObjectIDからkEntityFromIGESを生成しようとすると例外が発生することをテスト
    EXPECT_THROW(IDGenerator::Reserve(GetAssemblyID(), 128, pd_pointer),
                 std::invalid_argument);
}

// 予約されたIDの取得テスト
TEST(IDGeneratorTest, GetReservedID) {
    auto iges_id = IDGenerator::Generate(ObjectType::kIgesData);
    unsigned int pd_pointer = 400;

    auto reserved_id = IDGenerator::Reserve(iges_id, 128, pd_pointer);
    auto retrieved_id = IDGenerator::GetReservedID(iges_id, pd_pointer);

    EXPECT_EQ(reserved_id, retrieved_id);
}

// GetByIntのテスト
TEST(IDGeneratorTest, GetByIntIDTest) {
    int int_id;
    std::set<int> ids;
    {
        auto entity_new_id = GetEntityNewID(126);
        int_id = entity_new_id.ToInt();

        ids = IDGenerator::GetAllIntIDs();
        EXPECT_TRUE(ids.find(int_id) != ids.end());
    }

    // int型の最大値は登録されていないことを確認
    auto max_int_id = std::numeric_limits<int>::max();
    EXPECT_TRUE(ids.find(max_int_id) == ids.end());

    // 存在しないint型IDで例外が発生することをテスト
    EXPECT_THROW(IDGenerator::GetByIntID(max_int_id),
                 std::invalid_argument);

    // entity_new_idのスコープを抜けたので、expiredしているはず
    EXPECT_THROW(IDGenerator::GetByIntID(int_id),
                 std::invalid_argument);
}

// 予約されていないIDの取得で例外が発生することをテスト
TEST(IDGeneratorTest, GetUnreservedIDThrowsException) {
    auto iges_id = IDGenerator::Generate(ObjectType::kIgesData);
    unsigned int pd_pointer = 600;

    EXPECT_THROW(IDGenerator::GetReservedID(iges_id, pd_pointer),
                 std::invalid_argument);
}

// 異なるキーで異なるIDが予約されることをテスト
TEST(IDGeneratorTest, DifferentKeysGetDifferentIDs) {
    auto iges_id1 = IDGenerator::Generate(ObjectType::kIgesData);
    unsigned int pd_pointer1 = 800;
    auto iges_id2 = IDGenerator::Generate(ObjectType::kIgesData);
    unsigned int pd_pointer2 = 801;

    auto id1 = IDGenerator::Reserve(iges_id1, 126, pd_pointer1);
    auto id2 = IDGenerator::Reserve(iges_id2, 126, pd_pointer2);

    EXPECT_NE(id1, id2);
}

// スレッドセーフティのテスト
TEST(IDGeneratorTest, ThreadSafety) {
    const int num_threads = 10;
    const int ids_per_thread = 100;
    std::vector<std::thread> threads;
    std::vector<std::vector<ObjectID>> results(num_threads);

    // 複数スレッドでID生成
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&results, i, ids_per_thread]() {
            for (int j = 0; j < ids_per_thread; ++j) {
                results[i].push_back(GetIgesDataID());
                results[i].push_back(GetAssemblyID());
                results[i].push_back(GetEntityNewID(126));
                results[i].push_back(GetEntityGraphicsID(126));
            }
        });
    }

    // 全スレッドの完了を待機
    for (auto& thread : threads) {
        thread.join();
    }

    // 生成されたIDがすべて一意であることを確認
    // 同時に、各IDがUnsetIDではなく、IDGenerator::GetByIntID()で取得可能であることも確認
    std::unordered_set<ObjectID> unique_ids;
    int unmatched_count = 0;
    for (const auto& thread_results : results) {
        for (ObjectID id : thread_results) {
            EXPECT_TRUE(unique_ids.insert(id).second)
                << "Duplicate ID found: " << id;
            ASSERT_NE(id, IDGenerator::UnsetID());

            // IDGenerator::GetByIntID()で取得可能であることを確認
            int int_id = id.ToInt();
            auto retrieved_id = IDGenerator::GetByIntID(int_id);
            EXPECT_EQ(retrieved_id, id) << "For IntID: " << int_id;
            if (retrieved_id != id) ++unmatched_count;
        }
    }
    EXPECT_EQ(unmatched_count, 0) << "Some IDs could not be retrieved correctly by GetByIntID."
                                  << " Total ids: " << IDGenerator::GetAllIntIDs().size()
                                  << ", Unmatched count: " << unmatched_count;

    // 期待される一意なIDの数を確認
    EXPECT_EQ(unique_ids.size(), num_threads * ids_per_thread * 4);
}

// 予約機能のスレッドセーフティテスト
TEST(IDGeneratorTest, ReservationThreadSafety) {
    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::vector<ObjectID> results(num_threads);

    auto iges_id = IDGenerator::Generate(ObjectType::kIgesData);
    unsigned int pd_pointer = 2000;

    // 複数スレッドで同じキーのID予約
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&results, i, iges_id, pd_pointer]() {
            results[i] = IDGenerator::Reserve(iges_id, 126, pd_pointer);
        });
    }

    // 全スレッドの完了を待機
    for (auto& thread : threads) {
        thread.join();
    }

    // すべてのスレッドが同じIDを取得したことを確認
    ObjectID expected_id = results[0];
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(results[i], expected_id);
    }
}
