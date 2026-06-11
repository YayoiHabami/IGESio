/**
 * @file graphics/core/pick_registry.h
 * @brief エンティティ型とピック関数 (レイ交差・範囲選択サンプル) のレジストリ
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note コア改変なしに新しいエンティティ型のピッキング (レイ交差) と範囲選択を
 *       追加するための登録機構. キーはエンティティの動的型 (std::type_index)
 *       であり、非IGESエンティティ・ユーザー定義IGESエンティティの両方を
 *       同じ仕組みでカバーする. GraphicsRegistry (描画) とは独立しており、
 *       描画とピックは別々に登録・差し替えできる.
 */
#ifndef IGESIO_GRAPHICS_CORE_PICK_REGISTRY_H_
#define IGESIO_GRAPHICS_CORE_PICK_REGISTRY_H_

#include <functional>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/graphics/core/i_entity_graphics.h"



namespace igesio::graphics {

/// @brief エンティティ型 (C++の動的型) とピック関数のレジストリ
/// @note `EntityGraphics<T>`のピック既定実装 (`CanIntersect`/`Intersect`/
///       `GetSelectionSamples`) は、組み込みの能力ディスパッチ (ICurve/ISurface)
///       より先に本レジストリを動的型の完全一致で引く. これにより、
///       (1) メッシュ等の新しい能力を持つ型のピックをコア改変なしに追加できる
///       (2) 組み込み対応済みの具象型を登録した場合は既定のピックを差し替えられる
///       (完全一致キーのため、他の型のピックには波及しない)
/// @note 優先順位は「描画クラスの仮想関数オーバーライド > 本レジストリ >
///       能力ディスパッチ」. `PointGraphics`等、ピックをオーバーライドする
///       描画クラスには本レジストリは適用されない. また、レジストリ照会は
///       `EntityGraphics<T>`の既定実装が行うため、`IEntityGraphics`を直接
///       継承した描画クラスにも適用されない (各自で実装すること).
/// @note `intersect`と`selection_samples`は独立に登録できる (片方のみの登録では、
///       もう片方は能力ディスパッチへフォールバックする).
/// @note `GraphicsRegistry`と異なりリビジョンは持たない (ピック結果はレンダラに
///       キャッシュされず、登録の変更が次のピック呼び出しから自然に反映されるため).
/// @note スレッド安全性は保証しない. 登録はピック開始前に完了させること.
///       内部ストレージは関数内static (Meyersシングルトン) のため、静的初期化子
///       からの登録 (PickAutoRegistrar) でも初期化順に依存せず安全.
class PickRegistry {
 public:
    /// @brief 型消去されたレイ交差関数
    /// @note 引数のエンティティは、登録した型のインスタンスであることが
    ///       保証されて渡される. 第2引数は親空間→ワールドの変換行列
    ///       (`GetSelectionSamples`既定実装の規約と同じ. エンティティ自身が
    ///       参照する変換は関数側でエンティティ評価に含めること).
    ///       レイはワールド空間 (direction正規化済み・kRayとして扱う) で渡され、
    ///       交差点はワールド座標・distance昇順で返すこと
    using IntersectFunction = std::function<std::vector<RayHit>(
            const entities::IEntityIdentifier&, const igesio::Matrix4d&,
            const Ray&, const RayIntersectionParams&)>;

    /// @brief 型消去された範囲選択サンプリング関数
    /// @note 引数の規約は`IntersectFunction`と同じ. サンプルはワールド座標で
    ///       返すこと
    using SelectionSampleFunction = std::function<SelectionSamples(
            const entities::IEntityIdentifier&, const igesio::Matrix4d&,
            const SelectionSampleParams&)>;

    /// @brief 型消去されたピック関数ペア
    /// @note いずれもnullptr可 (未対応の機能は能力ディスパッチへフォールバック)
    struct PickFunctions {
        /// @brief レイ交差関数 (空=レイピック未対応)
        IntersectFunction intersect;
        /// @brief 範囲選択サンプリング関数 (空=範囲選択未対応)
        SelectionSampleFunction selection_samples;
    };

    /// @brief 登録型で型付けされたピック関数ペア
    /// @tparam T 対象の具象エンティティ型
    template <typename T>
    struct TypedPickFunctions {
        /// @brief レイ交差関数 (空=レイピック未対応)
        std::function<std::vector<RayHit>(
                const T&, const igesio::Matrix4d&,
                const Ray&, const RayIntersectionParams&)> intersect;
        /// @brief 範囲選択サンプリング関数 (空=範囲選択未対応)
        std::function<SelectionSamples(
                const T&, const igesio::Matrix4d&,
                const SelectionSampleParams&)> selection_samples;
    };

    /// @brief ピック関数を登録する
    /// @tparam T 対象の具象エンティティ型 (IEntityIdentifier派生・非抽象)
    /// @param functions ピック関数ペア (少なくとも一方は非null)
    /// @throw std::invalid_argument 両方の関数が空の場合、またはTのピック関数が
    ///        登録済みの場合
    template <typename T>
    static void Register(TypedPickFunctions<T> functions) {
        if (!RegisterImpl(std::type_index(typeid(T)),
                          EraseType<T>(std::move(functions)))) {
            throw std::invalid_argument(
                    "Pick functions are already registered for this entity type");
        }
    }

    /// @brief ピック関数を、未登録の場合のみ登録する
    /// @tparam T 対象の具象エンティティ型 (IEntityIdentifier派生・非抽象)
    /// @param functions ピック関数ペア (少なくとも一方は非null)
    /// @return 登録した場合はtrue. 登録済みの場合はfalse (変更なし)
    /// @throw std::invalid_argument 両方の関数が空の場合
    /// @note 静的初期化子からの自動登録 (`PickAutoRegistrar`) 用の冪等版.
    ///       自動登録と明示初期化が二重に走っても無害になる
    template <typename T>
    static bool TryRegister(TypedPickFunctions<T> functions) {
        return RegisterImpl(std::type_index(typeid(T)),
                            EraseType<T>(std::move(functions)));
    }

    /// @brief ピック関数の登録を解除する
    /// @tparam T 対象の具象エンティティ型
    /// @return 解除した場合はtrue. 未登録の場合はfalse
    template <typename T>
    static bool Unregister() {
        return UnregisterImpl(std::type_index(typeid(T)));
    }

    /// @brief ピック関数が登録済みか
    /// @tparam T 対象の具象エンティティ型
    /// @return 登録済みの場合はtrue
    template <typename T>
    static bool IsRegistered() {
        return IsRegisteredImpl(std::type_index(typeid(T)));
    }

    /// @brief 指定の動的型に対応するピック関数を取得する
    /// @param type エンティティの動的型 (`typeid(*entity)`)
    /// @return ピック関数ペア. 未登録の場合は両方とも空のPickFunctions
    /// @note `EntityGraphics<T>`のピック既定実装のディスパッチ用
    static PickFunctions Find(std::type_index type);

 private:
    /// @brief 型付きピック関数を型消去ペアへ包む
    /// @param functions 型付きピック関数ペア
    /// @return 型消去されたピック関数ペア
    /// @throw std::invalid_argument 両方の関数が空の場合
    template <typename T>
    static PickFunctions EraseType(TypedPickFunctions<T> functions) {
        static_assert(std::is_base_of_v<entities::IEntityIdentifier, T>,
                      "T must implement IEntityIdentifier");
        static_assert(!std::is_abstract_v<T>,
                      "T must be a concrete type (dispatch matches typeid exactly)");
        if (!functions.intersect && !functions.selection_samples) {
            throw std::invalid_argument(
                    "At least one pick function must be provided");
        }
        PickFunctions erased;
        if (functions.intersect) {
            erased.intersect = [f = std::move(functions.intersect)](
                    const entities::IEntityIdentifier& entity,
                    const igesio::Matrix4d& world_transform,
                    const Ray& ray, const RayIntersectionParams& params) {
                // ディスパッチが動的型の完全一致を保証する
                return f(dynamic_cast<const T&>(entity), world_transform,
                         ray, params);
            };
        }
        if (functions.selection_samples) {
            erased.selection_samples = [f = std::move(functions.selection_samples)](
                    const entities::IEntityIdentifier& entity,
                    const igesio::Matrix4d& world_transform,
                    const SelectionSampleParams& params) {
                return f(dynamic_cast<const T&>(entity), world_transform, params);
            };
        }
        return erased;
    }

    /// @brief 登録の実体
    /// @param type 対象の動的型
    /// @param functions 型消去済みのピック関数ペア
    /// @return 登録した場合はtrue. 登録済みの場合はfalse (変更なし.
    ///         例外にするかは呼び出し側のRegister/TryRegisterが決める)
    static bool RegisterImpl(std::type_index, PickFunctions);

    /// @brief 解除の実体
    static bool UnregisterImpl(std::type_index);

    /// @brief 登録確認の実体
    static bool IsRegisteredImpl(std::type_index);
};



/// @brief 静的初期化での自動登録に用いるレジストラ
/// @tparam T 対象の具象エンティティ型
/// @note 拡張モジュールやユーザーコードの「公開APIと同じ翻訳単位」に静的
///       インスタンスとして置くことで、その機能を利用するビルドでは明示的な
///       登録呼び出しなしにピックが配線される. 静的ライブラリではAPIが参照されない
///       翻訳単位はリンクされないため、登録専用の翻訳単位には置かないこと.
/// @note 登録は冪等 (`TryRegister`) のため、明示初期化と二重に走っても無害
/// @note 使用例 (拡張・ユーザーコードの.cpp内):
///       ```cpp
///       namespace {
///       const igesio::graphics::PickAutoRegistrar<MyVoxelEntity>
///               kMyVoxelPickRegistrar{{&IntersectVoxelWithRay,
///                                      &SampleVoxelForSelection}};
///       }  // namespace
///       ```
template <typename T>
class PickAutoRegistrar {
 public:
    /// @brief コンストラクタ (未登録の場合のみ登録する)
    /// @param functions ピック関数ペア (少なくとも一方は非null)
    explicit PickAutoRegistrar(
            PickRegistry::TypedPickFunctions<T> functions) {
        PickRegistry::TryRegister<T>(std::move(functions));
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_PICK_REGISTRY_H_
