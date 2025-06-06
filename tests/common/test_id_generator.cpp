/**
 * @file common/test_id_generator.h
 * @brief IDGeneratorのテスト
 * @author Yayoi Habami
 * @date 2025-06-02
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "igesio/common/id_generator.h"

namespace {

using IDGenerator = igesio::IDGenerator;
constexpr auto kUnsetID = igesio::kUnsetID;

}  // namespace



// 基本的なID生成のテスト
TEST(IDGeneratorTest, BasicIDGeneration) {
    uint64_t id1 = IDGenerator::Generate();
    uint64_t id2 = IDGenerator::Generate();

    // 生成された各IDが、常にkUnsetIDより大きく、
    // 以前に生成されたIDよりも大きいことを確認
    EXPECT_GT(id1, kUnsetID);
    EXPECT_GT(id2, kUnsetID);
    EXPECT_GT(id2, id1);
}

// ID予約機能のテスト
TEST(IDGeneratorTest, ReserveID) {
    uint64_t iges_id = 100;
    unsigned int pd_pointer = 200;

    uint64_t reserved_id = IDGenerator::Reserve(iges_id, pd_pointer);
    EXPECT_GT(reserved_id, kUnsetID);

    // 同じキーで再度予約すると、同じIDが返される
    uint64_t same_id = IDGenerator::Reserve(iges_id, pd_pointer);
    EXPECT_EQ(reserved_id, same_id);
}

// 予約されたIDの取得テスト
TEST(IDGeneratorTest, GetReservedID) {
    uint64_t iges_id = 300;
    unsigned int pd_pointer = 400;

    uint64_t reserved_id = IDGenerator::Reserve(iges_id, pd_pointer);
    uint64_t retrieved_id = IDGenerator::GetReservedID(iges_id, pd_pointer);

    EXPECT_EQ(reserved_id, retrieved_id);
}

// 予約されていないIDの取得で例外が発生することをテスト
TEST(IDGeneratorTest, GetUnreservedIDThrowsException) {
    uint64_t iges_id = 500;
    unsigned int pd_pointer = 600;

    EXPECT_THROW(IDGenerator::GetReservedID(iges_id, pd_pointer),
                 std::out_of_range);
}

// 異なるキーで異なるIDが予約されることをテスト
TEST(IDGeneratorTest, DifferentKeysGetDifferentIDs) {
    uint64_t iges_id1 = 700;
    unsigned int pd_pointer1 = 800;
    uint64_t iges_id2 = 701;
    unsigned int pd_pointer2 = 801;

    uint64_t id1 = IDGenerator::Reserve(iges_id1, pd_pointer1);
    uint64_t id2 = IDGenerator::Reserve(iges_id2, pd_pointer2);

    EXPECT_NE(id1, id2);
}

// スレッドセーフティのテスト
TEST(IDGeneratorTest, ThreadSafety) {
    const int num_threads = 10;
    const int ids_per_thread = 100;
    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> results(num_threads);

    // 複数スレッドでID生成
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&results, i, ids_per_thread]() {
            for (int j = 0; j < ids_per_thread; ++j) {
                results[i].push_back(IDGenerator::Generate());
            }
        });
    }

    // 全スレッドの完了を待機
    for (auto& thread : threads) {
        thread.join();
    }

    // 生成されたIDがすべて一意であることを確認
    std::set<uint64_t> unique_ids;
    for (const auto& thread_results : results) {
        for (uint64_t id : thread_results) {
            EXPECT_TRUE(unique_ids.insert(id).second)
                << "Duplicate ID found: " << id;
            EXPECT_GT(id, kUnsetID);
        }
    }

    EXPECT_EQ(unique_ids.size(), num_threads * ids_per_thread);
}

// 予約機能のスレッドセーフティテスト
TEST(IDGeneratorTest, ReservationThreadSafety) {
    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::vector<uint64_t> results(num_threads);

    uint64_t iges_id = 1000;
    unsigned int pd_pointer = 2000;

    // 複数スレッドで同じキーのID予約
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&results, i, iges_id, pd_pointer]() {
            results[i] = IDGenerator::Reserve(iges_id, pd_pointer);
        });
    }

    // 全スレッドの完了を待機
    for (auto& thread : threads) {
        thread.join();
    }

    // すべてのスレッドが同じIDを取得したことを確認
    uint64_t expected_id = results[0];
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(results[i], expected_id);
    }
}

// PairHashのテスト
TEST(IDGeneratorTest, PairHashFunction) {
    igesio::PairHash hasher;

    auto pair1 = std::make_pair(1ULL, 1U);
    auto pair2 = std::make_pair(1ULL, 2U);
    auto pair3 = std::make_pair(2ULL, 1U);

    std::size_t hash1 = hasher(pair1);
    std::size_t hash2 = hasher(pair2);
    std::size_t hash3 = hasher(pair3);

    // 異なるペアは異なるハッシュ値を持つべき
    EXPECT_NE(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_NE(hash2, hash3);

    // 同じペアは同じハッシュ値を持つべき
    EXPECT_EQ(hash1, hasher(pair1));
}
