/**
 * @file common/test_parallel.cpp
 * @brief common/parallel.hのテスト
 * @author Yayoi Habami
 * @date 2026-06-02
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   - igesio::ParallelFor(count, func, min_parallel_size)
 *   - igesio::ParallelForEach(items, func, min_parallel_size)
 *
 * 各環境 (Windows/macOS/Linux × GCC/Clang/MSVC) でスレッドバックエンドが正しく
 * 動作することの確認を主眼とする。スレッド数や実際に並列実行されたか否かは環境依存
 * かつ非決定的なため検証しない (フレーキー回避)。代わりに以下を検証する:
 *   - 各インデックス/要素がちょうど一度ずつ処理されること (網羅性)
 *   - 競合状態がないこと (atomic合計・独立スロット書き込みの整合)
 *   - funcの例外がデッドロックせず呼び出し側へ伝播すること
 *
 * TODO: 実際に複数スレッドで同時実行されたことの直接検証は行わない
 *       (タイミング依存でフレーキーになり、単一コア環境では成立しないため)。
 */
#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "igesio/common/parallel.h"

namespace {

/// @brief 並列パスを強制する閾値 (count>0なら必ず並列分岐を試みる)
constexpr std::size_t kForceParallel = 0;

/// @brief 直列パスを強制する閾値 (実用上ありえない大きさ)
constexpr std::size_t kForceSerial = std::numeric_limits<std::size_t>::max();

/// @brief ParallelForEach検証用の作業単位
/// @note 入力inputから結果resultを計算する。各インスタンスは自身のresultのみ
///       書き込むため、ロックなしで並列処理できる (Assemblyのキャッシュ構築と同型)。
struct Worker {
    std::size_t input;
    std::size_t result = 0;
    /// @brief result = input^2 を計算する
    void Compute() { result = input * input; }
};

}  // namespace



/**
 * ParallelFor: 正常系 (網羅性・競合なし)
 */

// 代表値: 各インデックスがちょうど一度ずつ呼ばれる (二重呼び出しをatomicで検出)
TEST(ParallelForTest, EachIndexVisitedExactlyOnce) {
    constexpr std::size_t kCount = 1000;
    std::vector<std::atomic<int>> visits(kCount);
    for (auto& v : visits) v.store(0, std::memory_order_relaxed);

    igesio::ParallelFor(kCount, [&visits](std::size_t i) {
        visits[i].fetch_add(1, std::memory_order_relaxed);
    }, kForceParallel);

    for (std::size_t i = 0; i < kCount; ++i) {
        EXPECT_EQ(visits[i].load(std::memory_order_relaxed), 1)
                << "index " << i;
    }
}

// 共有atomicへの加算が競合なく集計される (取りこぼし・重複がないこと)
TEST(ParallelForTest, AtomicSumUnderContention) {
    constexpr std::size_t kCount = 10000;
    std::atomic<std::uint64_t> sum{0};

    igesio::ParallelFor(kCount, [&sum](std::size_t i) {
        sum.fetch_add(i, std::memory_order_relaxed);
    }, kForceParallel);

    // 0 + 1 + ... + (kCount-1)
    const std::uint64_t expected = kCount * (kCount - 1) / 2;
    EXPECT_EQ(sum.load(), expected);
}

// 直列パスと並列パスで結果が一致する
TEST(ParallelForTest, SerialAndParallelProduceSameResult) {
    constexpr std::size_t kCount = 500;
    std::vector<int> serial(kCount, 0);
    std::vector<int> parallel(kCount, 0);

    igesio::ParallelFor(kCount, [&serial](std::size_t i) {
        serial[i] = static_cast<int>(i * 3);
    }, kForceSerial);
    igesio::ParallelFor(kCount, [&parallel](std::size_t i) {
        parallel[i] = static_cast<int>(i * 3);
    }, kForceParallel);

    EXPECT_EQ(serial, parallel);
}



/**
 * ParallelFor: 境界値
 */

// count=0: funcは一度も呼ばれず、ハングしない
TEST(ParallelForTest, Count0NeverCallsFunc) {
    std::atomic<int> calls{0};
    igesio::ParallelFor(0, [&calls](std::size_t) {
        calls.fetch_add(1, std::memory_order_relaxed);
    }, kForceParallel);
    EXPECT_EQ(calls.load(), 0);
}

// count=1: funcはi=0で一度だけ呼ばれる
TEST(ParallelForTest, Count1CallsFuncOnce) {
    std::vector<std::size_t> seen;
    igesio::ParallelFor(1, [&seen](std::size_t i) {
        seen.push_back(i);
    }, kForceParallel);
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen.front(), 0u);
}

// min_parallel_size境界: 直列分岐 (count == min) でも全件処理される
TEST(ParallelForTest, AtMinParallelSizeUsesSerialPath) {
    constexpr std::size_t kCount = 4;
    std::vector<int> out(kCount, 0);
    igesio::ParallelFor(kCount, [&out](std::size_t i) {
        out[i] = static_cast<int>(i) + 1;
    }, /*min_parallel_size=*/kCount);  // count <= min → 直列
    EXPECT_EQ(out, (std::vector<int>{1, 2, 3, 4}));
}

// min_parallel_size境界: 並列分岐 (count == min+1) でも全件処理される
TEST(ParallelForTest, JustAboveMinParallelSizeUsesParallelPath) {
    constexpr std::size_t kMin = 4;
    constexpr std::size_t kCount = kMin + 1;
    std::vector<int> out(kCount, 0);
    igesio::ParallelFor(kCount, [&out](std::size_t i) {
        out[i] = static_cast<int>(i) + 1;
    }, /*min_parallel_size=*/kMin);  // count > min → 並列
    EXPECT_EQ(out, (std::vector<int>{1, 2, 3, 4, 5}));
}

// ストレス: 大量要素でも全スロットが正しく書き込まれる
TEST(ParallelForTest, LargeCountWritesAllSlots) {
    constexpr std::size_t kCount = 100000;
    std::vector<std::size_t> out(kCount, 0);
    igesio::ParallelFor(kCount, [&out](std::size_t i) {
        out[i] = i * 2;
    }, kForceParallel);
    for (std::size_t i = 0; i < kCount; ++i) {
        ASSERT_EQ(out[i], i * 2) << "index " << i;
    }
}



/**
 * ParallelFor: 例外伝播
 */

// 並列パス: funcが投げた例外がデッドロックせず伝播する
TEST(ParallelForTest, PropagatesExceptionFromParallelPath) {
    EXPECT_THROW(
        igesio::ParallelFor(1000, [](std::size_t) {
            throw std::runtime_error("boom");
        }, kForceParallel),
        std::runtime_error);
}

// 直列パス: funcが投げた例外が伝播する
TEST(ParallelForTest, PropagatesExceptionFromSerialPath) {
    EXPECT_THROW(
        igesio::ParallelFor(1, [](std::size_t) {
            throw std::runtime_error("boom");
        }, kForceSerial),
        std::runtime_error);
}



/**
 * ParallelForEach
 */

// 代表値: 各要素 (shared_ptr経由) が処理され、結果が正しい
// (Assembly::PrepareGeometryCachesがe->PrepareGeometryCache()を呼ぶ構造と同型)
TEST(ParallelForEachTest, ProcessesAllElements) {
    constexpr std::size_t kCount = 2000;
    std::vector<std::shared_ptr<Worker>> workers;
    workers.reserve(kCount);
    for (std::size_t i = 0; i < kCount; ++i) {
        workers.push_back(std::make_shared<Worker>(Worker{i}));
    }

    igesio::ParallelForEach(workers,
        [](const std::shared_ptr<Worker>& w) { w->Compute(); },
        kForceParallel);

    for (std::size_t i = 0; i < kCount; ++i) {
        EXPECT_EQ(workers[i]->result, i * i) << "index " << i;
    }
}

// 退化: 空vectorではfuncが呼ばれず、ハングしない
TEST(ParallelForEachTest, EmptyVectorNeverCallsFunc) {
    std::vector<int> empty;
    std::atomic<int> calls{0};
    igesio::ParallelForEach(empty, [&calls](const int&) {
        calls.fetch_add(1, std::memory_order_relaxed);
    }, kForceParallel);
    EXPECT_EQ(calls.load(), 0);
}
