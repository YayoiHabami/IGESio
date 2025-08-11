/**
 * @file common/id_generator.h
 * @brief IDを生成するためのクラスを定義する
 * @author Yayoi Habami
 * @date 2025-06-02
 * @copyright 2025 Yayoi Habami
 *
 * @todo プログラム中でintへのキャストを行う箇所があるため、
 *       IDの型をintに変更するか、独自の型を定義する
 */
#ifndef IGESIO_COMMON_ID_GENERATOR_H_
#define IGESIO_COMMON_ID_GENERATOR_H_

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <utility>


namespace igesio {

/// @brief 未設定のID
/// @note 参照先のIDが未設定の場合に使用される
constexpr uint64_t kUnsetID = 0;

/// @brief std::pair<uint64_t, unsigned int>用のハッシュ関数
struct PairHash {
    std::size_t operator()(const std::pair<uint64_t, unsigned int>& p) const;
};

/// @brief エンティティおよびIGESDataのIDを生成するためのクラス
/// @note プログラム全体で一意のIDを生成するために使用される.
///       スレッドセーフであり、kUnsetIDおよび以前に生成したIDより大きい値を生成する.
///       あくまでプログラムの起動から終了までの間で一意のIDを生成することを
///       目的としているため、プログラムの再起動後はIDがリセットされることに注意.
///       したがって、同じIGESの同じエンティティであっても、プログラムの再起動後は
///       異なるIDが生成される可能性がある.
class IDGenerator {
    /// @brief IDを生成するためのカウンター
    /// @note カウンターは1から始まる
    inline static std::atomic<uint64_t> counter{kUnsetID};

    /// @brief reserved_ids_にアクセスするためのmutex
    /// @note スレッドセーフなアクセスを保証するために使用
    inline static std::mutex reserved_ids_mutex_;
    /// @brief 予約されたIDを保持するためのマップ
    /// @note keyは親のIGESDataのIDとエンティティのPDレコードのシーケンス番号,
    ///       valueは予約されたID
    inline static std::unordered_map<std::pair<uint64_t, unsigned int>,
                                     uint64_t, PairHash> reserved_ids_;

 public:
    /// @brief 新しいIDを生成する
    /// @return 新しいID
    /// @note 生成されるIDは、kUnsetIDより大きい値であり、以前に生成したIDよりも
    ///       大きい値であることが保証される (連続性は保証されない).
    static uint64_t Generate() {
        // 頻繁に呼び出される上、実装が単純なためインライン関数として定義する
        return ++counter;
    }

    /// @brief IDを予約する
    /// @param iges_id 親のIGESDataのID
    /// @param pd_pointer エンティティのPDレコードのシーケンス番号
    /// @return 予約されたID
    static uint64_t Reserve(const uint64_t, const unsigned int);

    /// @brief 予約されたIDを取得する
    /// @param iges_id 親のIGESDataのID
    /// @param pd_pointer エンティティのPDレコードのシーケンス番号
    /// @return 予約されたID
    /// @throw std::invalid_argument 予約されていない場合
    static uint64_t GetReservedID(const uint64_t, const unsigned int);
};

}  // namespace igesio

#endif  // IGESIO_COMMON_ID_GENERATOR_H_
