/**
 * @file graphics/shader_registry.cpp
 * @brief シェーダー定義 (ソースコード+メタ情報) のレジストリ
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/shader_registry.h"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "./shaders/curves.h"
#include "./shaders/surfaces.h"

namespace {

namespace i_graph = igesio::graphics;

/// @brief ユーザー登録シェーダーへ採番する最初のID値
/// @note 組み込みID (0〜10) およびセンチネル (0xFFFFFFFE/0xFFFFFFFF) と
///       重ならないよう余裕を持たせて確保する
constexpr std::uint32_t kFirstUserIdValue = 256;

/// @brief レジストリのストレージ
struct RegistryStorage {
    /// @brief ID値と定義のマッピング
    /// @note unordered_mapは要素への参照が安定なため、Get()の返すポインタは
    ///       以後の登録で無効化されない
    std::unordered_map<std::uint32_t, i_graph::ShaderInfo> infos;
    /// @brief 名前とIDの逆引き (一意性の保証とFindに使用)
    std::unordered_map<std::string, i_graph::ShaderId> by_name;
    /// @brief 登録のたびに単調増加するリビジョン
    std::uint64_t revision = 0;
    /// @brief 次に採番するユーザーID値
    std::uint32_t next_user_id = kFirstUserIdValue;
};

/// @brief ストレージを取得する (組み込みシェーダーをseed済み)
/// @note 関数内static (Meyersシングルトン). 静的初期化子からの登録でも
///       初期化順に依存せず安全に参照できる. seedはリビジョンを増やさない
///       (レンダラの初期値0と整合し、起動直後の不要な再コンパイル検査を避ける)
RegistryStorage& GetStorage() {
    static RegistryStorage storage = [] {
        RegistryStorage s;
        auto seed = [&s](const i_graph::ShaderId id, i_graph::ShaderInfo info) {
            s.by_name.emplace(info.name, id);
            s.infos.emplace(id.Value(), std::move(info));
        };
        for (auto& [id, info] : i_graph::shaders::GetBuiltinCurveShaderInfos()) {
            seed(id, std::move(info));
        }
        for (auto& [id, info] : i_graph::shaders::GetBuiltinSurfaceShaderInfos()) {
            seed(id, std::move(info));
        }
        // センチネル (シェーダーコード無し). ToStringの表示と名前の予約のために
        // 登録する (codeが不完全なためコンパイル対象にはならない)
        seed(i_graph::ShaderId::kComposite,
             {"Composite", {}, false, i_graph::ShaderDrawCategory::kAlways});
        seed(i_graph::ShaderId::kNone,
             {"None", {}, false, i_graph::ShaderDrawCategory::kAlways});
        return s;
    }();
    return storage;
}

}  // namespace



i_graph::ShaderId i_graph::ShaderRegistry::Register(ShaderInfo info) {
    if (info.name.empty()) {
        throw std::invalid_argument("Shader name must not be empty");
    }
    if (info.code.IsIncomplete()) {
        throw std::invalid_argument(
                "Shader code is incomplete: vertex and fragment are required, "
                "and tcs/tes must be specified together");
    }
    auto& storage = GetStorage();
    if (storage.by_name.find(info.name) != storage.by_name.end()) {
        // 組み込み名 ("GeneralCurve"等) を含む登録済みの名前は使用できない
        // (組み込みシェーダーの差し替え禁止と名前の一意性の保証)
        throw std::invalid_argument(
                "Shader name is already registered: " + info.name);
    }

    const ShaderId id{storage.next_user_id++};
    storage.by_name.emplace(info.name, id);
    storage.infos.emplace(id.Value(), std::move(info));
    ++storage.revision;
    return id;
}

std::optional<i_graph::ShaderId> i_graph::ShaderRegistry::Find(
        const std::string_view name) {
    const auto& storage = GetStorage();
    const auto it = storage.by_name.find(std::string(name));
    if (it == storage.by_name.end()) return std::nullopt;
    return it->second;
}

const i_graph::ShaderInfo* i_graph::ShaderRegistry::Get(const ShaderId id) {
    const auto& storage = GetStorage();
    const auto it = storage.infos.find(id.Value());
    if (it == storage.infos.end()) return nullptr;
    return &it->second;
}

std::uint64_t i_graph::ShaderRegistry::Revision() {
    return GetStorage().revision;
}

std::vector<i_graph::ShaderId> i_graph::ShaderRegistry::AllIds() {
    const auto& storage = GetStorage();
    std::vector<ShaderId> ids;
    ids.reserve(storage.infos.size());
    for (const auto& [value, info] : storage.infos) {
        ids.emplace_back(value);
    }
    return ids;
}
