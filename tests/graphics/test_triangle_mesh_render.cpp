/**
 * @file tests/graphics/test_triangle_mesh_render.cpp
 * @brief TriangleMeshGraphics (MeshEntityの描画) の検証
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - GraphicsRegistryへの組み込みseed (MeshEntity → TriangleMeshGraphics)
 *       - CreateEntityGraphics経由の生成 (登録操作なしで成立すること)
 *       - レンダラでの描画 (法線あり/なしメッシュ)・編集後の自動再同期
 * @note 検証はMockOpenGLの呼び出し回数のデルタで行う (絶対回数は実装詳細).
 */
#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/numerics/mesh/algorithms.h"
#include "igesio/entities/mesh_entity.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/factory.h"
#include "igesio/graphics/graphics_registry.h"
#include "igesio/graphics/meshes/triangle_mesh_graphics.h"
#include "igesio/graphics/renderer.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
namespace i_num = igesio::numerics;
using i_graph::test::MockOpenGL;

/// @brief z=0平面上の単位正方形メッシュ (法線なし) を生成する
i_num::TriangleMeshd MakeQuadMesh() {
    i_num::TriangleMeshd mesh;
    mesh.positions.resize(3, 4);
    mesh.positions << 0.0, 1.0, 1.0, 0.0,
                      0.0, 0.0, 1.0, 1.0,
                      0.0, 0.0, 0.0, 0.0;
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

/// @brief 法線つきの単位正方形メッシュを生成する
i_num::TriangleMeshd MakeQuadMeshWithNormals() {
    auto mesh = MakeQuadMesh();
    i_num::RecomputeNormals(mesh);
    return mesh;
}

}  // namespace



// MeshEntityの描画は組み込みでレジストリへseed済み (登録操作は不要)
TEST(TriangleMeshRenderTest, BuiltinSeedIsRegistered) {
    EXPECT_TRUE(i_graph::GraphicsRegistry::IsRegistered<i_ent::MeshEntity>());
}

// CreateEntityGraphicsがTriangleMeshGraphicsを生成し、同期でGPU転送が走る
TEST(TriangleMeshRenderTest, CreateEntityGraphicsBuildsMeshGraphics) {
    auto gl = std::make_shared<MockOpenGL>();
    auto entity = std::make_shared<i_ent::MeshEntity>(MakeQuadMeshWithNormals());

    auto graphics = i_graph::CreateEntityGraphics(entity, gl);
    ASSERT_NE(graphics, nullptr);
    EXPECT_NE(dynamic_cast<i_graph::TriangleMeshGraphics*>(graphics.get()),
              nullptr);
    EXPECT_EQ(graphics->GetShaderType(), i_graph::ShaderType::kGeneralSurface);
    // synchronize=true (既定) でVAO生成・バッファ転送まで行われる
    EXPECT_GT(gl->gen_vertex_arrays_calls, 0);
    EXPECT_GT(gl->buffer_data_calls, 0);
    EXPECT_TRUE(graphics->IsDrawable());
}

// レンダラ経由でメッシュが描画される (Scene追加+Drawのみ)
TEST(TriangleMeshRenderTest, RendererDrawsMeshEntity) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->AddEntity(std::make_shared<i_ent::MeshEntity>(
            MakeQuadMeshWithNormals()));
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();
    EXPECT_GT(gl->draw_elements_calls, 0);
}

// 法線なしメッシュも描画できる (PrewarmCpuが面積重み平均で補う)
TEST(TriangleMeshRenderTest, RendererDrawsMeshWithoutNormals) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->AddEntity(std::make_shared<i_ent::MeshEntity>(MakeQuadMesh()));
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();
    EXPECT_GT(gl->draw_elements_calls, 0);
}

// SetMesh (形状編集) が手動通知なしで次回Drawの再同期として反映される
TEST(TriangleMeshRenderTest, MeshEditTriggersResyncOnNextDraw) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    auto entity = std::make_shared<i_ent::MeshEntity>(MakeQuadMeshWithNormals());
    root->AddEntity(entity);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();
    const int uploads_before = gl->buffer_data_calls;

    // 編集なしの再Drawでは再転送されない
    renderer.Draw();
    EXPECT_EQ(gl->buffer_data_calls, uploads_before);

    // SetMeshによる形状編集 → 次回Drawで再転送される
    auto larger = MakeQuadMeshWithNormals();
    larger.positions *= 2.0;
    entity->SetMesh(std::move(larger));
    renderer.Draw();
    EXPECT_GT(gl->buffer_data_calls, uploads_before);
}
