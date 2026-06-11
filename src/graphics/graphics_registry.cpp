/**
 * @file graphics/graphics_registry.cpp
 * @brief エンティティ型と描画オブジェクト作成関数のレジストリ
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/graphics_registry.h"

#include <typeindex>
#include <unordered_map>
#include <utility>

namespace {

namespace i_graph = igesio::graphics;

/// @brief レジストリのストレージ
struct RegistryStorage {
    /// @brief 動的型と作成関数のマッピング
    std::unordered_map<std::type_index,
                       i_graph::GraphicsRegistry::FactoryFunction> creators;
    /// @brief 登録・解除のたびに単調増加するリビジョン
    std::uint64_t revision = 0;
};

/// @brief ストレージを取得する
/// @note 関数内static (Meyersシングルトン). 静的初期化子からの登録
///       (GraphicsAutoRegistrar) でも初期化順に依存せず安全に参照できる
RegistryStorage& GetStorage() {
    static RegistryStorage storage = [] {
        RegistryStorage s;
        // 組み込みの描画対応 (MeshEntity等) はここでseedする
        // (コアの組み込み分は静的レジストラに頼らず、遅延初期化で確実に登録する)
        return s;
    }();
    return storage;
}

}  // namespace



bool i_graph::GraphicsRegistry::RegisterImpl(
        const std::type_index type, FactoryFunction creator) {
    auto& storage = GetStorage();
    if (storage.creators.find(type) != storage.creators.end()) {
        // 登録済み: TryRegisterはfalse、Registerは呼び出し側で例外にする
        return false;
    }
    storage.creators.emplace(type, std::move(creator));
    ++storage.revision;
    return true;
}

bool i_graph::GraphicsRegistry::UnregisterImpl(const std::type_index type) {
    auto& storage = GetStorage();
    if (storage.creators.erase(type) == 0) return false;
    ++storage.revision;
    return true;
}

bool i_graph::GraphicsRegistry::IsRegisteredImpl(const std::type_index type) {
    auto& storage = GetStorage();
    return storage.creators.find(type) != storage.creators.end();
}

std::uint64_t i_graph::GraphicsRegistry::Revision() {
    return GetStorage().revision;
}

i_graph::GraphicsRegistry::FactoryFunction
i_graph::GraphicsRegistry::FindCreator(const std::type_index type) {
    const auto& storage = GetStorage();
    auto it = storage.creators.find(type);
    if (it == storage.creators.end()) return {};
    return it->second;
}
