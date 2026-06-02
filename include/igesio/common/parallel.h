/**
 * @file common/parallel.h
 * @brief 標準ライブラリのみで実装した並列実行ユーティリティ
 * @author Yayoi Habami
 * @date 2026-06-02
 * @copyright 2026 Yayoi Habami
 * @note <execution> (Parallel STL) やOpenMPに依存せず、std::async/std::futureのみで
 *       実装する。これによりWindows/macOS/Linux × GCC/Clang + Windows×MSVCの全構成で、
 *       追加の外部ライブラリなしに動作する (LinuxではThreads::Threadsのリンクのみ必要)。
 */
#ifndef IGESIO_COMMON_PARALLEL_H_
#define IGESIO_COMMON_PARALLEL_H_

#include <algorithm>
#include <cstddef>
#include <future>
#include <thread>
#include <vector>



namespace igesio {

/// @brief 並列実行に切り替える最小要素数の既定値
/// @note これ以下の要素数では直列実行する (スレッド生成のオーバーヘッドを避けるため)
constexpr std::size_t kDefaultMinParallelSize = 2;

/// @brief [0, count) の各インデックスに対しfuncを並列に適用する
/// @tparam Func std::size_tを引数に取る呼び出し可能型
/// @param count 反復回数
/// @param func 各インデックスに適用する処理。func(i)の形で呼ばれる
/// @param min_parallel_size 並列化する最小要素数 (これ以下は直列実行)
/// @throws funcが送出した例外 (複数スレッドで生じた場合はいずれか一つを送出する)
/// @note funcは異なるインデックスにつき高々一度ずつ、別スレッドから呼ばれうる。
///       同一インデックスへの同時呼び出しは行わないため、各反復が互いに独立した
///       書き込み先のみを扱う限りロックは不要。本関数は全ワーカーを待ち合わせてから返る
///       (戻り時点でワーカーの書き込みは呼び出しスレッドから可視となる)。
template <typename Func>
void ParallelFor(const std::size_t count, const Func& func,
                 const std::size_t min_parallel_size = kDefaultMinParallelSize) {
    // ハードウェア並列度 (取得不能時は1とみなす)
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 1;

    // 直列フォールバック: 要素数が少ない、または並列度が1
    if (count <= min_parallel_size || hw <= 1) {
        for (std::size_t i = 0; i < count; ++i) func(i);
        return;
    }

    // スレッド数を決定し、[0, count) を連続するチャンクへ分割する
    const std::size_t n_threads = std::min(static_cast<std::size_t>(hw), count);
    const std::size_t chunk = (count + n_threads - 1) / n_threads;

    // 1チャンク [begin, end) を処理する
    const auto run_chunk = [&func](std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) func(i);
    };

    // 先頭のn_threads-1チャンクを非同期起動し、末尾チャンクは呼び出しスレッドで実行する
    std::vector<std::future<void>> futures;
    futures.reserve(n_threads - 1);
    for (std::size_t t = 0; t + 1 < n_threads; ++t) {
        const std::size_t begin = t * chunk;
        const std::size_t end = std::min(begin + chunk, count);
        if (begin >= end) break;
        futures.push_back(std::async(std::launch::async, run_chunk, begin, end));
    }
    const std::size_t last_begin = (n_threads - 1) * chunk;
    if (last_begin < count) run_chunk(last_begin, count);

    // 全ワーカーを待ち合わせる (例外は最初の一つを再送出する)
    for (auto& f : futures) f.get();
}

/// @brief vectorの各要素に対しfuncを並列に適用する
/// @tparam T 要素の型
/// @tparam Func const T&を引数に取る呼び出し可能型
/// @param items 対象の要素列
/// @param func 各要素に適用する処理。func(items[i])の形で呼ばれる
/// @param min_parallel_size 並列化する最小要素数 (これ以下は直列実行)
/// @throws funcが送出した例外
/// @note ParallelForのvector版。要素の読み取りは並列に行われるため、funcは各要素
///       (および各要素が指す先) を書き換えない、または互いに独立した書き込みのみを行うこと。
template <typename T, typename Func>
void ParallelForEach(const std::vector<T>& items, const Func& func,
                     const std::size_t min_parallel_size
                             = kDefaultMinParallelSize) {
    ParallelFor(items.size(),
                [&items, &func](std::size_t i) { func(items[i]); },
                min_parallel_size);
}

}  // namespace igesio

#endif  // IGESIO_COMMON_PARALLEL_H_
