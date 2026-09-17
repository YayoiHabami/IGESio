/**
 * @file tests/extensions/machines/scene/scene_for_testing.h
 * @brief machines拡張のテストで共有するシーン構築のフィクスチャ
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note シーン構築、アニメーション生成、表示オブジェクトのテストが同じ
 *       ルートとシーンの組を使えるようにする. セットアップは`motion_for_testing.h`
 *       のものを組み合わせる.
 */
#ifndef TESTS_EXTENSIONS_MACHINES_SCENE_SCENE_FOR_TESTING_H_
#define TESTS_EXTENSIONS_MACHINES_SCENE_SCENE_FOR_TESTING_H_

#include <memory>
#include <string_view>

#include "igesio/models/assembly.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/scene/machine_scene.h"
#include "../simulation/motion_for_testing.h"

namespace scene_test {

/// @brief 呼び出し側のルートと構築済みのシーンの組
/// @note ルートはシーンより先に宣言し、シーンの破棄後に破棄する
struct BuiltScene {
    /// @brief 呼び出し側のルート
    std::shared_ptr<igesio::models::Assembly> root =
            igesio::models::MakeAssembly("root");
    /// @brief シーン
    igesio::extensions::machines::MachineScene scene;
};

/// @brief セットアップからシーンを作る
/// @param setup 加工セットアップ
/// @param options 構築の設定
inline BuiltScene MakeScene(
        const igesio::extensions::machines::MachiningSetup& setup,
        const igesio::extensions::machines::SceneBuildOptions& options = {}) {
    BuiltScene built;
    built.scene.Build(setup, built.root, options);
    return built;
}

/// @brief 動特性の無い機械 (`MinimalXyzAc`) のセットアップでシーンを作る
inline BuiltScene MakeSceneWithoutDynamics(
        const igesio::extensions::machines::SceneBuildOptions& options = {}) {
    return MakeScene(motion_test::MakeSetupWithoutDynamics(), options);
}

/// @brief 直接の子アセンブリを名前で探す
/// @return 無ければ`nullptr`
inline std::shared_ptr<igesio::models::Assembly> FindChild(
        const igesio::models::Assembly& parent, const std::string_view name) {
    for (const auto& child : parent.GetChildAssemblies()) {
        if (child->Metadata().name == name) return child;
    }
    return nullptr;
}

}  // namespace scene_test

#endif  // TESTS_EXTENSIONS_MACHINES_SCENE_SCENE_FOR_TESTING_H_
