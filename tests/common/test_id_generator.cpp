/**
 * @file common/test_id_generator.h
 * @brief IDGeneratorのテスト
 * @author Yayoi Habami
 * @date 2025-06-02
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <unordered_set>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "igesio/common/id_generator.h"

namespace {

using igesio::IDGenerator;
using igesio::ObjectType;
using igesio::ObjectID;

}  // namespace



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
}

// 予約されたIDの取得テスト
TEST(IDGeneratorTest, GetReservedID) {
    auto iges_id = IDGenerator::Generate(ObjectType::kIgesData);
    unsigned int pd_pointer = 400;

    auto reserved_id = IDGenerator::Reserve(iges_id, 128, pd_pointer);
    auto retrieved_id = IDGenerator::GetReservedID(iges_id, pd_pointer);

    EXPECT_EQ(reserved_id, retrieved_id);
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
                results[i].push_back(IDGenerator::Generate(ObjectType::kEntityNew, 126));
            }
        });
    }

    // 全スレッドの完了を待機
    for (auto& thread : threads) {
        thread.join();
    }

    // 生成されたIDがすべて一意であることを確認
    std::unordered_set<ObjectID> unique_ids;
    for (const auto& thread_results : results) {
        for (ObjectID id : thread_results) {
            EXPECT_TRUE(unique_ids.insert(id).second)
                << "Duplicate ID found: " << id;
            EXPECT_NE(id, IDGenerator::UnsetID());
        }
    }

    EXPECT_EQ(unique_ids.size(), num_threads * ids_per_thread);
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
