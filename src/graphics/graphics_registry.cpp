/**
 * @file graphics/graphics_registry.cpp
 * @brief エンティティ型と描画オブジェクト作成関数のレジストリ
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/graphics_registry.h"

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include "igesio/entities/meshes/mesh_entity.h"
#include "igesio/graphics/meshes/triangle_mesh_graphics.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;

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
        // 組み込みの描画対応はここでseedする (コアの組み込み分は静的レジストラに
        // 頼らず、遅延初期化で確実に登録する). seedはリビジョンを増やさない
        // (レンダラの初期値0と整合し、起動直後の不要な負キャッシュ破棄を避ける).
        // NOTE: ここでTryRegister等を呼ぶとGetStorage()の再帰初期化になるため、
        //       必ずローカルのs.creatorsへ直接挿入すること
        s.creators.emplace(
            std::type_index(typeid(i_ent::MeshEntity)),
            [](const std::shared_ptr<const i_ent::IEntityIdentifier>& entity,
               const std::shared_ptr<i_graph::IOpenGL>& gl)
                    -> std::unique_ptr<i_graph::IEntityGraphics> {
                return std::make_unique<i_graph::TriangleMeshGraphics>(
                    std::dynamic_pointer_cast<const i_ent::MeshEntity>(entity),
                    gl);
            });
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
