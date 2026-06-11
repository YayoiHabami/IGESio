/**
 * @file graphics/graphics_registry.h
 * @brief エンティティ型と描画オブジェクト作成関数のレジストリ
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note コア改変なしに新しいエンティティ型の描画を追加するための登録機構.
 *       キーはエンティティの動的型 (std::type_index) であり、非IGESエンティティ・
 *       ユーザー定義IGESエンティティの両方を同じ仕組みでカバーする.
 */
#ifndef IGESIO_GRAPHICS_GRAPHICS_REGISTRY_H_
#define IGESIO_GRAPHICS_GRAPHICS_REGISTRY_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <utility>

#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/graphics/core/i_entity_graphics.h"



namespace igesio::graphics {

/// @brief エンティティ型 (C++の動的型) と描画オブジェクト作成関数のレジストリ
/// @note `CreateEntityGraphics`は組み込みの能力ディスパッチ (ICurve/ISurface/Point)
///       より先に本レジストリを動的型の完全一致で引く. これにより、
///       (1) メッシュ等の新しい能力を持つ型の描画をコア改変なしに追加できる
///       (2) 組み込み対応済みの具象型を登録した場合は既定の描画を差し替えられる
///       (完全一致キーのため、他の型の描画には波及しない)
/// @note 登録・解除は`Revision()`へ反映され、レンダラが負キャッシュ (生成失敗の
///       記録) を自動破棄して未描画の型の生成を再試行する. ただし生成済みの
///       描画オブジェクトには波及しない (差し替えを既存の個体へ反映するには
///       シーンの再設定等でオブジェクトを再生成すること).
/// @note スレッド安全性は保証しない. 登録は描画開始前に完了させること.
///       内部ストレージは関数内static (Meyersシングルトン) のため、静的初期化子
///       からの登録 (GraphicsAutoRegistrar) でも初期化順に依存せず安全.
class GraphicsRegistry {
 public:
    /// @brief 型消去された描画オブジェクト作成関数
    /// @note 引数のエンティティは、登録した型のインスタンスであることが
    ///       保証されて渡される
    using FactoryFunction = std::function<std::unique_ptr<IEntityGraphics>(
            const std::shared_ptr<const entities::IEntityIdentifier>&,
            const std::shared_ptr<IOpenGL>&)>;

    /// @brief 登録型で型付けされた描画オブジェクト作成関数
    /// @tparam T 対象の具象エンティティ型
    template <typename T>
    using TypedFactoryFunction = std::function<std::unique_ptr<IEntityGraphics>(
            const std::shared_ptr<const T>&,
            const std::shared_ptr<IOpenGL>&)>;

    /// @brief 描画オブジェクト作成関数を登録する
    /// @tparam T 対象の具象エンティティ型 (IEntityIdentifier派生・非抽象)
    /// @param creator 作成関数. nullptrの返却は「型起因の恒久的失敗 (描画不可)」
    ///        を意味する (factory.hの`CreateEntityGraphics`の不変条件と同じ)
    /// @throw std::invalid_argument creatorが空の場合、またはTの作成関数が
    ///        登録済みの場合
    template <typename T>
    static void Register(TypedFactoryFunction<T> creator) {
        if (!RegisterImpl(std::type_index(typeid(T)),
                          EraseType<T>(std::move(creator)))) {
            throw std::invalid_argument(
                    "Graphics creator is already registered for this entity type");
        }
    }

    /// @brief 描画オブジェクト作成関数を、未登録の場合のみ登録する
    /// @tparam T 対象の具象エンティティ型 (IEntityIdentifier派生・非抽象)
    /// @param creator 作成関数
    /// @return 登録した場合はtrue. 登録済みの場合はfalse (変更なし)
    /// @throw std::invalid_argument creatorが空の場合
    /// @note 静的初期化子からの自動登録 (`GraphicsAutoRegistrar`) 用の冪等版.
    ///       自動登録と明示初期化が二重に走っても無害になる
    template <typename T>
    static bool TryRegister(TypedFactoryFunction<T> creator) {
        return RegisterImpl(std::type_index(typeid(T)),
                            EraseType<T>(std::move(creator)));
    }

    /// @brief 描画オブジェクト作成関数の登録を解除する
    /// @tparam T 対象の具象エンティティ型
    /// @return 解除した場合はtrue. 未登録の場合はfalse
    template <typename T>
    static bool Unregister() {
        return UnregisterImpl(std::type_index(typeid(T)));
    }

    /// @brief 描画オブジェクト作成関数が登録済みか
    /// @tparam T 対象の具象エンティティ型
    /// @return 登録済みの場合はtrue
    template <typename T>
    static bool IsRegistered() {
        return IsRegisteredImpl(std::type_index(typeid(T)));
    }

    /// @brief レジストリのリビジョンを取得する
    /// @return 登録・解除のたびに単調増加する値
    /// @note レンダラが負キャッシュの破棄要否の判定に用いる
    static std::uint64_t Revision();

    /// @brief 指定の動的型に対応する作成関数を取得する
    /// @param type エンティティの動的型 (`typeid(*entity)`)
    /// @return 作成関数. 未登録の場合は空のstd::function
    /// @note `CreateEntityGraphics`のディスパッチ用
    static FactoryFunction FindCreator(std::type_index type);

 private:
    /// @brief 型付き作成関数を型消去ファクトリへ包む
    /// @param creator 型付き作成関数
    /// @return 型消去された作成関数
    /// @throw std::invalid_argument creatorが空の場合
    template <typename T>
    static FactoryFunction EraseType(TypedFactoryFunction<T> creator) {
        static_assert(std::is_base_of_v<entities::IEntityIdentifier, T>,
                      "T must implement IEntityIdentifier");
        static_assert(!std::is_abstract_v<T>,
                      "T must be a concrete type (dispatch matches typeid exactly)");
        if (!creator) {
            throw std::invalid_argument(
                    "Graphics creator function must not be empty");
        }
        return [creator = std::move(creator)](
                const std::shared_ptr<const entities::IEntityIdentifier>& entity,
                const std::shared_ptr<IOpenGL>& gl) {
            // ディスパッチが動的型の完全一致を保証する. IEntityIdentifierは
            // 仮想基底のためstatic_castは使えず、dynamic_pointer_castで降ろす
            return creator(std::dynamic_pointer_cast<const T>(entity), gl);
        };
    }

    /// @brief 登録の実体
    /// @param type 対象の動的型
    /// @param creator 型消去済みの作成関数
    /// @return 登録した場合はtrue. 登録済みの場合はfalse (変更なし.
    ///         例外にするかは呼び出し側のRegister/TryRegisterが決める)
    static bool RegisterImpl(std::type_index, FactoryFunction);

    /// @brief 解除の実体
    static bool UnregisterImpl(std::type_index);

    /// @brief 登録確認の実体
    static bool IsRegisteredImpl(std::type_index);
};



/// @brief 静的初期化での自動登録に用いるレジストラ
/// @tparam T 対象の具象エンティティ型
/// @note 拡張モジュールやユーザーコードの「公開APIと同じ翻訳単位」に静的
///       インスタンスとして置くことで、その機能を利用するビルドでは明示的な
///       登録呼び出しなしに描画が配線される. 静的ライブラリではAPIが参照されない
///       翻訳単位はリンクされないため、登録専用の翻訳単位には置かないこと.
/// @note 登録は冪等 (`TryRegister`) のため、明示初期化と二重に走っても無害
/// @note 使用例 (拡張・ユーザーコードの.cpp内):
///       ```cpp
///       namespace {
///       const igesio::graphics::GraphicsAutoRegistrar<MyMeshEntity>
///               kMyMeshGraphicsRegistrar{&CreateMyMeshGraphics};
///       }  // namespace
///       ```
template <typename T>
class GraphicsAutoRegistrar {
 public:
    /// @brief コンストラクタ (未登録の場合のみ登録する)
    /// @param creator 作成関数
    explicit GraphicsAutoRegistrar(
            GraphicsRegistry::TypedFactoryFunction<T> creator) {
        GraphicsRegistry::TryRegister<T>(std::move(creator));
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_GRAPHICS_REGISTRY_H_
