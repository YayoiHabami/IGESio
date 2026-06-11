/**
 * @file tests/graphics/test_graphics_registry.cpp
 * @brief GraphicsRegistry (描画オブジェクト作成関数のレジストリ) の検証
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `GraphicsRegistry::Register` / `TryRegister` / `Unregister` /
 *         `IsRegistered` / `Revision` / `FindCreator`
 *       - `CreateEntityGraphics`のディスパッチ順 (レジストリ最優先・
 *         組み込み描画の差し替え)
 *       - `GraphicsAutoRegistrar` (静的初期化での自動登録)
 *       - レンダラの負キャッシュ自動破棄 (レジストリリビジョン検知)
 * @note TODO: スレッド安全性は契約外 (登録は描画開始前) のため対象外.
 */
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <utility>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/entities/non_iges_entity_base.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/factory.h"
#include "igesio/graphics/graphics_registry.h"
#include "igesio/graphics/renderer.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector2d;
using i_graph::GraphicsRegistry;
using i_graph::test::MockOpenGL;

/// @brief レジストリ登録テスト用の非IGESエンティティ (形状なしの最小実装)
class PinEntity : public i_ent::NonIgesEntityBase {
 public:
    /// @brief コンストラクタ
    PinEntity() = default;
};

/// @brief 静的初期化での自動登録テスト用のエンティティ (PinEntityとは別型)
class AutoPinEntity : public i_ent::NonIgesEntityBase {
 public:
    /// @brief コンストラクタ
    AutoPinEntity() = default;
};

/// @brief PinGraphicsの生成回数 (ファクトリ呼び出しの観測用)
int pin_graphics_created = 0;

/// @brief PinEntity用の最小描画クラス (GLリソースを持たない)
class PinGraphics : public i_graph::EntityGraphics<PinEntity> {
 public:
    /// @brief コンストラクタ
    /// @param entity 対象エンティティ
    /// @param gl OpenGLラッパー
    PinGraphics(const std::shared_ptr<const PinEntity>& entity,
                const std::shared_ptr<i_graph::IOpenGL>& gl)
            : EntityGraphics<PinEntity>(
                  entity, gl, i_graph::ShaderType::kGeneralCurve, false) {
        ++pin_graphics_created;
    }

 protected:
    /// @brief 同期 (GLリソースなしのため何もしない)
    void DoSynchronize() override {}

    /// @brief 描画 (GLリソースなしのため何もしない)
    void DrawImpl(igesio::graphics::gl::Uint,
                  const std::pair<float, float>&) const override {}
};

/// @brief PinGraphicsを生成する作成関数
std::unique_ptr<i_graph::IEntityGraphics> MakePinGraphics(
        const std::shared_ptr<const PinEntity>& entity,
        const std::shared_ptr<i_graph::IOpenGL>& gl) {
    return std::make_unique<PinGraphics>(entity, gl);
}

/// @brief スコープ終了時に指定型の登録を解除するガード
/// @tparam T 解除対象のエンティティ型
template <typename T>
class RegistryGuard {
 public:
    /// @brief デストラクタ (登録を解除する)
    ~RegistryGuard() { GraphicsRegistry::Unregister<T>(); }
};

/// @brief 静的初期化での自動登録 (テスト本体でIsRegisteredを確認する)
/// @note 公開APIと同じ翻訳単位に置く規約の実例. add_executable直下のTUのため
///       リンカ除去は起きない
const i_graph::GraphicsAutoRegistrar<AutoPinEntity> kAutoPinRegistrar{
        [](const std::shared_ptr<const AutoPinEntity>& entity,
           const std::shared_ptr<i_graph::IOpenGL>& gl)
                -> std::unique_ptr<i_graph::IEntityGraphics> {
            return nullptr;  // 登録自体の検証用 (生成はしない)
        }};

/// @brief スモーク用の単純な円弧 (M_entity=単位)
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return i_ent::MakeCircularArc(
        Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, 1.0), 0.0);
}

}  // namespace



/**
 * 正常系: 登録・ディスパッチ
 */

// 登録した型のエンティティはレジストリ経由で描画オブジェクトが生成される
TEST(GraphicsRegistryTest, RegisterAndDispatch) {
    RegistryGuard<PinEntity> guard;
    GraphicsRegistry::Register<PinEntity>(&MakePinGraphics);
    EXPECT_TRUE(GraphicsRegistry::IsRegistered<PinEntity>());

    auto gl = std::make_shared<MockOpenGL>();
    auto pin = std::make_shared<PinEntity>();
    pin_graphics_created = 0;
    auto graphics = i_graph::CreateEntityGraphics(pin, gl);

    ASSERT_NE(graphics, nullptr);
    EXPECT_EQ(pin_graphics_created, 1);
    EXPECT_NE(dynamic_cast<PinGraphics*>(graphics.get()), nullptr);
    EXPECT_EQ(graphics->GetEntityID(), pin->GetID());
}

// 未登録の非IGES型 (ICurve/ISurface/Pointのいずれでもない) はnullptr
TEST(GraphicsRegistryTest, UnregisteredTypeReturnsNullptr) {
    auto gl = std::make_shared<MockOpenGL>();
    auto pin = std::make_shared<PinEntity>();
    EXPECT_EQ(i_graph::CreateEntityGraphics(pin, gl), nullptr);
}

// 組み込み対応済みの具象型を登録すると既定の描画を差し替えられる
// (レジストリ最優先のディスパッチ順の確認. nullptr返却=描画不可へ差し替え)
TEST(GraphicsRegistryTest, RegisteredCreatorOverridesBuiltin) {
    auto gl = std::make_shared<MockOpenGL>();
    auto arc = MakeArc();

    // 組み込みでは生成される
    EXPECT_NE(i_graph::CreateEntityGraphics(arc, gl), nullptr);

    {
        RegistryGuard<i_ent::CircularArc> guard;
        GraphicsRegistry::Register<i_ent::CircularArc>(
                [](const std::shared_ptr<const i_ent::CircularArc>&,
                   const std::shared_ptr<i_graph::IOpenGL>&)
                        -> std::unique_ptr<i_graph::IEntityGraphics> {
                    return nullptr;
                });
        // レジストリが優先され、結果 (nullptr) が最終となる
        EXPECT_EQ(i_graph::CreateEntityGraphics(arc, gl), nullptr);
    }

    // 解除後は組み込みへ戻る
    EXPECT_NE(i_graph::CreateEntityGraphics(arc, gl), nullptr);
}

// 静的初期化子からの自動登録 (GraphicsAutoRegistrar) が有効になっている
TEST(GraphicsRegistryTest, AutoRegistrarRegistersAtStaticInitialization) {
    EXPECT_TRUE(GraphicsRegistry::IsRegistered<AutoPinEntity>());
}

// Revisionは登録・解除のたびに単調増加する
TEST(GraphicsRegistryTest, RevisionIncreasesOnRegisterAndUnregister) {
    const auto rev0 = GraphicsRegistry::Revision();
    {
        RegistryGuard<PinEntity> guard;
        GraphicsRegistry::Register<PinEntity>(&MakePinGraphics);
        EXPECT_GT(GraphicsRegistry::Revision(), rev0);
    }
    // ガードのUnregisterでさらに増加する
    EXPECT_GT(GraphicsRegistry::Revision(), rev0 + 1);
    // 未登録の解除では変化しない
    const auto rev1 = GraphicsRegistry::Revision();
    EXPECT_FALSE(GraphicsRegistry::Unregister<PinEntity>());
    EXPECT_EQ(GraphicsRegistry::Revision(), rev1);
}



/**
 * 異常系: 重複・空関数
 */

// 重複登録はRegisterでは例外、TryRegisterではfalse (変更なし)
TEST(GraphicsRegistryTest, DuplicateRegistration) {
    RegistryGuard<PinEntity> guard;
    GraphicsRegistry::Register<PinEntity>(&MakePinGraphics);

    EXPECT_THROW(GraphicsRegistry::Register<PinEntity>(&MakePinGraphics),
                 std::invalid_argument);
    EXPECT_FALSE(GraphicsRegistry::TryRegister<PinEntity>(&MakePinGraphics));
    EXPECT_TRUE(GraphicsRegistry::IsRegistered<PinEntity>());
}

// 空の作成関数はRegister/TryRegisterとも例外
TEST(GraphicsRegistryTest, EmptyCreatorThrows) {
    EXPECT_THROW(GraphicsRegistry::Register<PinEntity>(nullptr),
                 std::invalid_argument);
    EXPECT_THROW(GraphicsRegistry::TryRegister<PinEntity>(nullptr),
                 std::invalid_argument);
    EXPECT_FALSE(GraphicsRegistry::IsRegistered<PinEntity>());
}



/**
 * レンダラとの連携 (負キャッシュの自動破棄)
 */

// 未登録時の生成失敗 (負キャッシュ) が、後からの登録で自動的に再試行される
TEST(GraphicsRegistryRendererTest, RegistryRevisionInvalidatesNegativeCache) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    auto pin = std::make_shared<PinEntity>();
    root->AddEntity(pin);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // 未登録: 生成失敗が負キャッシュされる (ファクトリは呼ばれない)
    pin_graphics_created = 0;
    renderer.Draw();
    EXPECT_EQ(pin_graphics_created, 0);

    // 登録のみでレジストリリビジョンが変化し、手動通知なしで再試行される
    RegistryGuard<PinEntity> guard;
    GraphicsRegistry::Register<PinEntity>(&MakePinGraphics);
    renderer.Draw();
    EXPECT_EQ(pin_graphics_created, 1);

    // 生成済みオブジェクトは温存される (再生成されない)
    renderer.Draw();
    EXPECT_EQ(pin_graphics_created, 1);
}
