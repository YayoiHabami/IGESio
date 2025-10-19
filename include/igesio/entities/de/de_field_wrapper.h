/**
 * @file entities/de/de_field_wrapper.h
 * @brief Directory Entryのフィールドのうち、ポインタを持つ可能性のあるフィールドの
 *        値を格納するクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 * @details DEセクションの20個のフィールドを、本ライブラリでは①列挙体やプリミティブ型を返すもの、
 *          ②ポインタも持つ可能性のあるもの、③IEntityクラスのインスタンス上で意味を持たないもの
 *          の3つに分類している。このうち、②の値を格納するクラスを定義する。
 */
#ifndef IGESIO_ENTITIES_DE_DE_FIELD_WRAPPER_H_
#define IGESIO_ENTITIES_DE_DE_FIELD_WRAPPER_H_

#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "igesio/common/id_generator.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"



namespace igesio::entities {

/// @brief DEフィールドの値の種類
/// @note DEフィールドが保持する値がデフォルト値、ポインタ、正の値の
///       いずれであるかを示す
enum class DEFieldValueType {
    /// @brief デフォルト値（無効値）
    /// @note 値が0で、特別な意味を持たない状態
    kDefault,
    /// @brief エンティティへのポインタ
    /// @note 負の値で、他のエンティティを参照する状態
    kPointer,
    /// @brief 正の値
    /// @note 標準的な規定値を示す状態
    kPositive
};

/// @brief DEフィールドを包含する基底クラス
/// @tparam Args IEntityIdentifierを継承した型の可変長引数
/// @note Directory Entryのフィールド値を構造化データとして提供する
///       抽象化レイヤー
/// @note Argsは少なくとも1つの型を持つ必要があり、すべての型は
///       IEntityIdentifierを継承している必要がある
template<typename... Args>
class DEFieldWrapper {
    static_assert(sizeof...(Args) > 0, "At least one argument type is required");
    static_assert((std::is_base_of_v<IEntityIdentifier, Args> && ...),
                  "All types must inherit from IEntityIdentifier");

 private:
    /// @brief このDEフィールドが参照するエンティティのID
    /// @note プログラム上でエンティティが一意に持つ数値.
    ///       `!id_.IsSet()`の場合はこのフィールドがポインタを持たないことを示す.
    /// @note ポインタ設定前の予約値としての役割がメイン
    ObjectID id_;

    /// @brief このDEフィールドの値の種類
    /// @note デフォルト値、ポインタ、正の値のいずれかを示す
    DEFieldValueType value_type_ = DEFieldValueType::kDefault;

    /// @brief 各型のweak_ptrを保持するタプル
    /// @note 複数のポインタが有効になることはない.
    ///       いずれか一つの型のポインタのみが有効であるか、
    ///       すべてのポインタが無効であることを保証する
    std::tuple<std::weak_ptr<const Args>...> weak_ptrs_;

    /// @brief 指定された型のshared_ptrを設定する実装
    /// @param ptr 設定するshared_ptr
    /// @note id_がUnsetIDの場合はポインタを設定しない
    /// @throw std::invalid_argument ptrのIDがid_と一致しない場合
    template<typename T>
    void SetPointerImpl(const std::shared_ptr<const T>& ptr) {
        if (ptr) {
            if (id_ != IDGenerator::UnsetID() && ptr->GetID() != id_) {
                throw std::invalid_argument("ID mismatch: expected " + ToString(id_) +
                                          ", got " + ToString(ptr->GetID()));
            }
        }

        // 他のすべてのweak_ptrを無効化
        InvalidateAllPointers();

        // 指定された型のweak_ptrを設定
        std::get<std::weak_ptr<const T>>(weak_ptrs_) = ptr;
    }

    /// @brief すべてのweak_ptrを無効化する
    void InvalidateAllPointers() {
        std::apply([](auto&... ptrs) {
            ((ptrs = std::weak_ptr<const typename std::remove_reference_t<
                    decltype(ptrs)>::element_type>{}), ...);
        }, weak_ptrs_);
    }

 protected:
    /// @brief value_type_をkPointerに設定する
    void SetAsPointer() { value_type_ = DEFieldValueType::kPointer; }
    /// @brief value_type_をkPositiveに設定する
    /// @note id_をUnsetIDに置き換え、ポインタを無効化する
    void SetAsPositive() {
        value_type_ = DEFieldValueType::kPositive;
        if (id_ != IDGenerator::UnsetID()) {
            // ポインタを無効化
            id_ = IDGenerator::UnsetID();
            InvalidateAllPointers();
        }
    }
    /// @brief value_type_をkDefaultに設定する
    /// @note id_をUnsetIDに置き換え、ポインタを無効化する
    void SetAsDefault() {
        value_type_ = DEFieldValueType::kDefault;
        if (id_ != IDGenerator::UnsetID()) {
            // ポインタを無効化
            id_ = IDGenerator::UnsetID();
            InvalidateAllPointers();
        }
    }



 public:
    /**
     * コンストラクタ・代入演算子・デストラクタ
     */

    /// @brief デフォルトコンストラクタ
    /// @note デフォルト値（0）のケースで使用
    DEFieldWrapper() : id_(IDGenerator::UnsetID()) {}

    /// @brief IDを指定するコンストラクタ
    /// @param id このDEフィールドが参照するエンティティのID
    /// @note 与えられる値は正の値
    /// @note 本来DEフィールドにおいてポインタは負の値で表現されるが、
    ///       その負値から正値への変換は別途行うことを想定している
    explicit DEFieldWrapper(const ObjectID& id) : id_(id) {
        if (id.IsSet()) SetAsPointer();
    }

    /// @brief コピーコンストラクタ
    /// @param other コピー元のDEFieldWrapperインスタンス
    DEFieldWrapper(const DEFieldWrapper& other)
        : id_(other.id_), weak_ptrs_(other.weak_ptrs_),
          value_type_(other.value_type_) {}

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のDEFieldWrapperインスタンス
    /// @note 他のインスタンスのIDを無効化する (UnsetIDに設定)
    DEFieldWrapper(DEFieldWrapper&& other) noexcept
        : id_(other.id_), weak_ptrs_(std::move(other.weak_ptrs_)),
          value_type_(other.value_type_) {
        // 他のインスタンスのIDを無効化
        other.SetAsDefault();
    }

    /// @brief コピー代入演算子
    /// @param other コピー元のDEFieldWrapperインスタンス
    /// @return このインスタンスへの参照
    DEFieldWrapper& operator=(const DEFieldWrapper& other) {
        if (this != &other) {
            id_ = other.id_;
            value_type_ = other.value_type_;
            weak_ptrs_ = other.weak_ptrs_;
        }
        return *this;
    }

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のDEFieldWrapperインスタンス
    /// @return このインスタンスへの参照
    /// @note 他のインスタンスのIDを無効化する (UnsetIDに設定)
    DEFieldWrapper& operator=(DEFieldWrapper&& other) noexcept {
        if (this != &other) {
            id_ = other.id_;
            value_type_ = other.value_type_;
            weak_ptrs_ = std::move(other.weak_ptrs_);
            // 他のインスタンスのIDを無効化
            other.SetAsDefault();
        }
        return *this;
    }

    /// @brief デストラクタ
    ~DEFieldWrapper() = default;



    /// @brief 格納されている値の種類を取得する
    /// @return 値の種類
    /// @note 値の種類は、デフォルト値 (kDefault; 0)、ポインタ (kPointer; 負の値)、
    ///       または正の値 (kPositive; 正の値) のいずれか
    DEFieldValueType GetValueType() const { return value_type_; }

    /// @brief 値を取得する
    /// @param id2de IDとDEポインターのマッピング
    /// @return デフォルト値の場合は0、ポインタの場合は正の値を返す
    /// @throw std::out_of_range id2deが空でなく、かつポインタが設定されている場合に
    ///        id2deに存在しないIDが参照されている場合
    /// @note id2deを指定した場合、ポインタの値はid2deに基づいて変換される.
    int GetValue(const id2pointer& id2de = {}) const {
        switch (value_type_) {
            case DEFieldValueType::kDefault:
                return 0;  // デフォルト値
            case DEFieldValueType::kPointer:
                if (id2de.size() > 0) {
                    // ポインタの場合、id2deに基づいて変換
                    if (auto it = id2de.find(id_); it != id2de.end()) {
                        return static_cast<int>(it->second);
                    } else {
                        throw std::out_of_range(
                            "Entity ID " + ToString(id_) +
                            " in DEFieldWrapper not found in ID mapping.");
                    }
                }
                return id_.ToInt();
            default:
                throw std::logic_error("Unknown DEFieldValueType");
        }
    }



    /**
     * メンバ関数 (ID操作)
     */

    /// @brief 現在のIDを取得
    /// @return このフィールドが参照するエンティティのID
    /// @note エンティティを参照しない場合はUnsetIDを返す
    const ObjectID& GetID() const {
        return id_;
    }

    /// @brief 未設定のポインタがあればそのIDを取得する
    /// @return 未設定のポインタのID、またはstd::nullopt
    /// @note ポインタが設定済みの場合や、デフォルト値が設定されている場合、
    ///       規定値 (正の値) が設定されている場合はstd::nulloptを返す。
    virtual std::optional<ObjectID> GetUnsetID() const {
        // デフォルト値の場合 (id_がUnsetIDの場合) はstd::nulloptを返す
        if (!id_.IsSet()) return std::nullopt;

        // それ以外の場合、有効なポインタを持たなければid_を返す
        if (!HasValidPointer()) {
            return id_;
        }
        // 有効なポインタがある場合はstd::nulloptを返す
        return std::nullopt;
    }

    /// @brief IDを上書きする
    /// @param new_id 新しいID
    /// @note 新しいIDが現在のIDと異なる場合、すべてのポインタを無効化する.
    ///       このため、ポインタの再設定が必要となる. ポインタが既に作成済みの場合は
    ///       OverwritePointerを使用すること.
    void OverwriteID(const ObjectID& new_id) {
        if (id_ != new_id) {
            id_ = new_id;
            InvalidateAllPointers();

            // ValueTypeを更新
            if (new_id.IsSet()) {
                SetAsPointer();
            } else {
                SetAsDefault();
            }
        }
    }



    /**
     * メンバ関数 (ポインタ操作)
     */

    /// @brief 指定された型のポインタを取得
    /// @return 指定された型のポインタ (std::shared_ptr<const T>)
    template<typename T>
    typename std::enable_if_t<std::disjunction_v<std::is_same<T, Args>...>,
                              std::shared_ptr<const T>>
    GetPointer() const {
        return std::get<std::weak_ptr<const T>>(weak_ptrs_).lock();
    }

    /// @brief 有効なポインタがあるか
    /// @return いずれかの型のポインタが設定済みの場合はtrue,
    ///         すべてのポインタが無効な場合はfalse
    bool HasValidPointer() const {
        return std::apply([](const auto&... ptrs) {
            return ((!ptrs.expired()) || ...);
        }, weak_ptrs_);
    }

    /// @brief ポインタを設定する
    /// @tparam T 設定するポインタの型 (Argsのいずれかであること)
    /// @param ptr 設定するポインタ
    /// @throw std::invalid_argument ptrのIDが本インスタンスのIDと一致しない場合
    template<typename T>
    typename std::enable_if_t<std::disjunction_v<std::is_same<T, Args>...>, void>
    SetPointer(const std::shared_ptr<const T>& ptr) {
        if (id_ == IDGenerator::UnsetID()) {
            // id_がUnsetIDの場合、ポインタは有効化されない
            InvalidateAllPointers();
            return;
        }

        SetPointerImpl<T>(ptr);
    }

    /// @brief ポインタを設定する (非const版)
    /// @tparam T 設定するポインタの型 (Argsのいずれかであること)
    /// @param ptr 設定するポインタ
    /// @throw std::invalid_argument ptrのIDが本インスタンスのIDと一致しない場合
    template<typename T>
    typename std::enable_if_t<std::disjunction_v<std::is_same<T, Args>...>, void>
    SetPointer(const std::shared_ptr<T>& ptr) {
        auto ptr2 = std::static_pointer_cast<const T>(ptr);
        SetPointer<T>(ptr2);
    }

    /// @brief ポインタを上書きする
    /// @tparam T 上書きするポインタの型 (Argsのいずれかであること)
    /// @param ptr 上書きするポインタ
    /// @throw std::invalid_argument ptrがnullptrの場合
    template<typename T>
    typename std::enable_if_t<std::disjunction_v<std::is_same<T, Args>...>, void>
    OverwritePointer(const std::shared_ptr<const T>& ptr) {
        if (!ptr) {
            throw std::invalid_argument("Pointer cannot be nullptr.");
        } else if (id_ != ptr->GetID()) {
            // IDが異なる場合は上書き
            OverwriteID(ptr->GetID());
        }

        SetPointerImpl<T>(ptr);
    }

    /// @brief ポインタを上書きする (非const版)
    /// @tparam T 上書きするポインタの型 (Argsのいずれかであること)
    /// @param ptr 上書きするポインタ
    /// @throw std::invalid_argument ptrがnullptrの場合
    template<typename T>
    typename std::enable_if_t<std::disjunction_v<std::is_same<T, Args>...>, void>
    OverwritePointer(const std::shared_ptr<T>& ptr) {
        auto ptr2 = std::static_pointer_cast<const T>(ptr);
        OverwritePointer<T>(ptr2);
    }

    /// @brief デフォルト状態に戻す
    /// @note ポインタを持たない状態に戻し、IDをUnsetIDに設定する
    void Reset() { SetAsDefault(); }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_DE_DE_FIELD_WRAPPER_H_
