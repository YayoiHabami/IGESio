/**
 * @file entities/pointer_container.h
 * @brief Parameter Data (PD) セクションで定義されるポインタフィールドを、
 *        プログラム上で取り扱うためのコンテナクラス
 * @author Yayoi Habami
 * @date 2025-06-06
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_POINTER_CONTAINER_H_
#define IGESIO_ENTITIES_POINTER_CONTAINER_H_

#include <memory>

#include "igesio/entities/interfaces/i_entity_identifier.h"



namespace igesio::entities {

/// @brief 複数の型のポインタを保持するためのコンテナクラス
/// @tparam UseWeakPtr ポインタに`weak_ptr`を使用するか.
///         循環参照が生じないことが保証されている場合は`false`を指定することで、
///         `shared_ptr`を使用できる. 例えばCompositeCurveの子要素の格納に使用することで、
///         子要素のライフタイムのことを気にせずに親要素のみ管理することができる.
///         (weak_ptrを使用する場合、参照先を別途保持していなければ、親要素が残っていても
///         参照先が解放されてしまう可能性があることに注意)
/// @tparam DerivedTypes IEntityIdentifierを継承したクラス群
/// @note 例えば曲線全般を参照するポインタ（Composite CurveのPDレコードなど）を
///       指定する場合は、`PointerContainer<ICurve>`のように指定することができる.
///       また、複数の型を同時に指定することも可能.
/// @note DerivedTypesにおいて、IEntityIdentifierは指定不可能.
///       すべてのエンティティのポインタを指定したい場合は、EntityBaseを指定すること.
template<bool UseWeakPtr, typename... DerivedTypes>
class PointerContainer {
    // 全ての型がIEntityIdentifierを継承していることを確認
    static_assert((std::is_base_of_v<IEntityIdentifier, DerivedTypes> && ...),
                  "All DerivedTypes must inherit from IEntityIdentifier");

    /// @brief 参照に使用するポインタの型. UseWeakPtrがtrueの場合はweak_ptr,
    ///        falseの場合はshared_ptrを使用する
    template<typename T>
    using PtrType = std::conditional_t<
            UseWeakPtr, std::weak_ptr<T>, std::shared_ptr<T>>;

    /// @brief entity_のID
    /// @note 指し示す先が未設定の場合はid_.IsSet() == falseとなる
    ObjectID id_;
    /// @brief ポインタを保持するためのvariant
    /// @note 循環参照が生じる可能性を否定できない場合、`weak_ptr`を使用可能にする
    std::variant<PtrType<const DerivedTypes>...> entity_;
    /// @brief ポインタが設定されているかどうか
    bool is_pointer_set_ = false;

 public:
    /**
     * コンストラクタ
     */

    /// @brief エンティティのIDのみを指定したコンストラクタ
    /// @param id 参照先のエンティティのID
    /// @note 参照先のIDが未定の場合は、UnsetIDを使用する
    explicit PointerContainer(const ObjectID& id)
        : id_(id), entity_(), is_pointer_set_(false) {}

    /// @brief DerivedTypesのいずれかの型のオブジェクトを受け取るコンストラクタ
    /// @tparam T DerivedTypesのいずれかの型
    /// @param entity 参照先のオブジェクト
    /// @note エンティティのIDを自動的に取得し、ポインタを設定する
    template<typename T,
             std::enable_if_t<(std::is_same_v<T, DerivedTypes> || ...), int> = 0>
    explicit PointerContainer(const std::shared_ptr<const T>& entity)
        : id_(entity->GetID()), entity_(PtrType<const T>(entity)),
          is_pointer_set_(true) {}

    /// @brief DerivedTypesのいずれかの型のオブジェクトを受け取るコンストラクタ（非const版）
    /// @tparam T DerivedTypesのいずれかの型
    /// @param entity 参照先のオブジェクト
    /// @note 非const版からconst版への暗黙変換を許可する
    template<typename T,
             std::enable_if_t<(std::is_same_v<T, DerivedTypes> || ...), int> = 0>
    explicit PointerContainer(const std::shared_ptr<T>& entity)
        : id_(entity->GetID()), entity_(PtrType<T>(entity)),
          is_pointer_set_(true) {}

    /// @brief IEntityIdentifierからDerivedTypesへの変換コンストラクタ
    /// @param entity 変換元のIEntityIdentifierオブジェクト
    /// @throws std::runtime_error dynamic_castに失敗した場合
    /// @note dynamic_castを使用してIEntityIdentifierからDerivedTypesのいずれかに変換する
    explicit PointerContainer(const std::shared_ptr<const IEntityIdentifier>& entity) {
        if (!entity) {
            throw std::runtime_error(
                    "Cannot convert null IEntityIdentifier to DerivedTypes");
        }
        bool success = false;
        // 成功するまで各型へのキャストを試みる
        ([&] {
            if (success) return;
            if (auto derived = std::dynamic_pointer_cast<const DerivedTypes>(entity)) {
                entity_ = PtrType<const DerivedTypes>(derived);
                success = true;
            }
        }(), ...);

        if (!success) {
            throw std::runtime_error(
                    "Cannot convert IEntityIdentifier to any of the specified DerivedTypes");
        }
        id_ = entity->GetID();
        is_pointer_set_ = true;
    }

    /// @brief IEntityIdentifierからDerivedTypesへの変換コンストラクタ（非const版）
    /// @param entity 変換元のIEntityIdentifierオブジェクト
    /// @throws std::runtime_error dynamic_castに失敗した場合
    explicit PointerContainer(const std::shared_ptr<IEntityIdentifier>& entity)
        : PointerContainer(std::const_pointer_cast<const IEntityIdentifier>(entity)) {}


    /// @brief コピーコンストラクタ
    PointerContainer(const PointerContainer& other) = default;

    /// @brief ムーブコンストラクタ
    PointerContainer(PointerContainer&& other) noexcept = default;

    /// @brief コピー代入演算子
    PointerContainer& operator=(const PointerContainer& other) = default;

    /// @brief ムーブ代入演算子
    PointerContainer& operator=(PointerContainer&& other) noexcept = default;


    /**
     * 情報の取得
     */

    /// @brief ポインタが設定されているかどうかを確認する
    /// @return ポインタが設定されており、参照先が有効な場合はtrue、そうでなければfalse
    bool IsPointerSet() const {
        if (!is_pointer_set_) return false;
        return std::visit([](const auto& ptr) {
            if constexpr (UseWeakPtr) {
                return !ptr.expired();
            } else {
                return static_cast<bool>(ptr);
            }
        }, entity_);
    }

    /// @brief 参照先エンティティのIDを取得する
    /// @return 参照先エンティティのID
    const ObjectID& GetID() const { return id_; }

    /// @brief 格納されているポインタが指定された型であるかを確認する
    /// @tparam T 確認したい型。DerivedTypesのいずれかである必要がある
    /// @return 指定された型のポインタを保持している場合はtrue、そうでなければfalse
    template<typename T>
    bool IsHoldingType() const {
        static_assert((std::is_same_v<T, DerivedTypes> || ...),
                      "T must be one of the DerivedTypes");
        return std::holds_alternative<PtrType<const T>>(entity_);
    }

    /// @brief 参照先エンティティのポインタをvariantとして取得する
    /// @return 参照先エンティティのポインタを保持するvariant
    /// @throws std::runtime_error ポインタが設定されていない、または参照先が破棄されている場合
    std::variant<std::shared_ptr<const DerivedTypes>...> GetEntityVariant() const {
        if (!IsPointerSet()) {
            throw std::runtime_error("Pointer is not set or has expired.");
        }
        return std::visit(
            [](const auto& ptr) -> std::variant<std::shared_ptr<const DerivedTypes>...> {
                if constexpr (UseWeakPtr) {
                    return ptr.lock();
                } else {
                    return ptr;
                }
            },
            entity_);
    }

    /// @brief 参照先エンティティを特定の型として取得する
    /// @tparam T 取得したい型
    /// @return 参照先エンティティのポインタ
    /// @throws std::runtime_error ポインタが設定されていない、
    ///         参照先が破棄されている、または型が一致しない場合
    template<typename T>
    std::shared_ptr<const T> GetEntity() const {
        if (!IsPointerSet()) {
            throw std::runtime_error("Pointer is not set or has expired.");
        }
        auto* ptr_ptr = std::get_if<PtrType<const T>>(&entity_);
        if (!ptr_ptr) {
            throw std::runtime_error("Stored pointer is not of the requested type.");
        }
        if constexpr (UseWeakPtr) {
            auto locked = ptr_ptr->lock();
            if (!locked) {
                throw std::runtime_error("Referenced entity has been destroyed.");
            }
            return locked;
        } else {
            return *ptr_ptr;
        }
    }

    /// @brief 参照先エンティティをIEntityIdentifierとして取得する
    /// @return 参照先エンティティのIEntityIdentifierポインタ
    /// @throws std::runtime_error ポインタが設定されていない、または参照先が破棄されている場合
    std::shared_ptr<const IEntityIdentifier> GetEntityAsIEntityIdentifier() const {
        return std::visit(
            [](const auto& ptr) -> std::shared_ptr<const IEntityIdentifier> {
                return std::dynamic_pointer_cast<const IEntityIdentifier>(ptr);
            }, GetEntityVariant());
    }

    /// @brief 安全に参照先エンティティを特定の型として取得する
    /// @tparam T 取得したい型
    /// @return 参照先エンティティのポインタ、取得できない場合はnullopt
    template<typename T>
    std::optional<std::shared_ptr<const T>> TryGetEntity() const {
        if (!is_pointer_set_) return std::nullopt;

        // DerivedTypesのいずれかの型でvariantを検索する
        return std::visit([&](auto&& arg) -> std::optional<std::shared_ptr<const T>> {
            using ActualType = std::decay_t<decltype(arg)>;

            // ActualTypeがweak_ptrであるか確認
            if constexpr (std::is_same_v<ActualType, std::monostate>) {
                // variantが空の場合
                return std::nullopt;
            } else {
                std::shared_ptr<const T> ptr;
                if constexpr (UseWeakPtr) {
                    // weak_ptrからshared_ptrへの変換を試みる
                    auto locked = arg.lock();
                    if (!locked) return std::nullopt;
                    ptr = std::dynamic_pointer_cast<const T>(locked);
                } else {
                    ptr = std::dynamic_pointer_cast<const T>(arg);
                }
                if (ptr) return ptr;
                // dynamicなキャストに失敗した場合、またはweak_ptrがexpiredの場合
                return std::nullopt;
            }
        }, entity_);
    }

    /**
     * ポインターの設定
     */

    /// @brief ポインターを設定する
    /// @tparam T DerivedTypesのいずれかの型
    /// @param entity 設定する参照先のオブジェクト
    /// @return 設定に成功した場合はtrue、失敗した場合はfalse
    template<typename T,
             std::enable_if_t<(std::is_same_v<T, DerivedTypes> || ...), int> = 0>
    bool SetPointer(const std::shared_ptr<const T>& entity) {
        if (entity->GetID() != id_) return false;
        entity_ = PtrType<const T>(entity);
        is_pointer_set_ = true;
        return true;
    }

    /// @brief ポインターを設定する（非const版）
    template<typename T,
             std::enable_if_t<(std::is_same_v<T, DerivedTypes> || ...), int> = 0>
    bool SetPointer(const std::shared_ptr<T>& entity) {
        return SetPointer(std::const_pointer_cast<const T>(entity));
    }

    /// @brief ポインターを設定する（IEntityIdentifier版）
    /// @param entity 設定する参照先のIEntityIdentifierオブジェクト
    /// @return 設定に成功した場合はtrue、失敗した場合はfalse
    bool SetPointer(const std::shared_ptr<const IEntityIdentifier>& entity) {
        if (entity->GetID() != id_) return false;

        bool success = false;
        ([&] {
            if (success) return;
            if (auto derived = std::dynamic_pointer_cast<const DerivedTypes>(entity)) {
                entity_ = PtrType<const DerivedTypes>(derived);
                is_pointer_set_ = true;
                success = true;
            }
        }(), ...);
        return success;
    }

    /// @brief ポインターを設定する（非const IEntity版）
    /// @param entity 設定する参照先のIEntityIdentifierオブジェクト
    /// @return 設定に成功した場合はtrue、失敗した場合はfalse
    bool SetPointer(const std::shared_ptr<IEntityIdentifier>& entity) {
        return SetPointer(std::const_pointer_cast<const IEntityIdentifier>(entity));
    }

    /// @brief IDが異なる場合は上書きしてポインタを設定する
    /// @tparam T DerivedTypesのいずれかの型
    /// @param entity 設定する参照先のオブジェクト
    template<typename T,
             std::enable_if_t<(std::is_same_v<T, DerivedTypes> || ...), int> = 0>
    bool OverwritePointer(const std::shared_ptr<const T>& entity) {
        id_ = entity->GetID();
        entity_ = PtrType<const T>(entity);
        is_pointer_set_ = true;
        return true;
    }

    /// @brief IDが異なる場合は上書きしてポインタを設定する（非const版）
    template<typename T,
             std::enable_if_t<(std::is_same_v<T, DerivedTypes> || ...), int> = 0>
    bool OverwritePointer(const std::shared_ptr<T>& entity) {
        return OverwritePointer(std::const_pointer_cast<const T>(entity));
    }

    /// @brief IDが異なる場合は上書きしてポインタを設定する（IEntityIdentifier版）
    /// @param entity 設定する参照先のIEntityIdentifierオブジェクト
    /// @return 設定に成功した場合はtrue、失敗した場合はfalse
    bool OverwritePointer(const std::shared_ptr<const IEntityIdentifier>& entity) {
        bool success = false;
        ([&] {
            if (success) return;
            if (auto derived = std::dynamic_pointer_cast<const DerivedTypes>(entity)) {
                id_ = derived->GetID();
                entity_ = PtrType<const DerivedTypes>(derived);
                is_pointer_set_ = true;
                success = true;
            }
        }(), ...);
        return success;
    }

    /// @brief IDが異なる場合は上書きしてポインタを設定する（非const IEntity版）
    bool OverwritePointer(const std::shared_ptr<IEntityIdentifier>& entity) {
        return OverwritePointer(
                std::const_pointer_cast<const IEntityIdentifier>(entity));
    }


    /**
     * 演算子オーバーロード
     */

    /// @brief 等価演算子
    /// @note IDが一致する場合にtrueを返す
    bool operator==(const PointerContainer& other) const {
        return id_ == other.id_;
    }

    /// @brief 非等価演算子
    bool operator!=(const PointerContainer& other) const {
        return !(*this == other);
    }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_POINTER_CONTAINER_H_
