/**
 * @file tests/graphics/test_mesh_staging.cpp
 * @brief GPU転送用ステージング整形 (BuildInterleavedVertices) のテスト
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   igesio::graphics::BuildInterleavedVertices()
 *   - 正常系 (代表値): 全チャンネルを持つメッシュのレイアウト
 *     {x, y, z, nx, ny, nz, u, v} が正しいこと (float / double両方)
 *   - 正常系 (境界値): 空メッシュで空の列が返ること
 *   - 正常系 (退化): 法線・UVチャンネルが無い場合にゼロ埋めされること
 *
 * TODO: 事前条件を持たない純粋関数のためエラー系は対象外
 *       (チャンネル列数不整合の検査はnumerics::Validateの責務)。
 */
#include <gtest/gtest.h>

#include <vector>

#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/graphics/core/mesh_staging.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_num = igesio::numerics;

/// @brief float比較の許容誤差 (値はそのまま転写されるため厳しめにとる)
constexpr float kTol = 1e-6f;
/// @brief double→float変換の許容誤差 (単精度の丸め)
constexpr float kCastTol = 1e-6f;

/// @brief 全チャンネル (位置・法線・UV) を持つ2頂点メッシュを作成する
/// @note 各値はスロット位置の取り違えを検出できるよう全て異なる値にする
template <typename Scalar>
i_num::TriangleMeshT<Scalar> MakeFullMesh() {
    i_num::TriangleMeshT<Scalar> mesh;
    mesh.positions.resize(3, 2);
    mesh.positions << Scalar(1.0), Scalar(10.0),
                      Scalar(2.0), Scalar(20.0),
                      Scalar(3.0), Scalar(30.0);
    mesh.normals.resize(3, 2);
    mesh.normals << Scalar(0.1), Scalar(0.4),
                    Scalar(0.2), Scalar(0.5),
                    Scalar(0.3), Scalar(0.6);
    mesh.uvs.resize(2, 2);
    mesh.uvs << Scalar(0.25), Scalar(0.75),
                Scalar(0.5),  Scalar(1.0);
    mesh.indices = {0, 1, 0};
    return mesh;
}

}  // namespace



/**
 * 正常系 (代表値): レイアウト
 */

// floatメッシュ: 各頂点が {x,y,z,nx,ny,nz,u,v} の順に8個で並ぶ
TEST(BuildInterleavedVertices, FullChannels_LayoutMatchesShaderOrder) {
    const auto mesh = MakeFullMesh<float>();

    const auto v = i_graph::BuildInterleavedVertices(mesh);

    ASSERT_EQ(v.size(), 16u);  // 2頂点 × 8float
    // 頂点0
    EXPECT_NEAR(v[0], 1.0f, kTol);    // x
    EXPECT_NEAR(v[1], 2.0f, kTol);    // y
    EXPECT_NEAR(v[2], 3.0f, kTol);    // z
    EXPECT_NEAR(v[3], 0.1f, kTol);    // nx
    EXPECT_NEAR(v[4], 0.2f, kTol);    // ny
    EXPECT_NEAR(v[5], 0.3f, kTol);    // nz
    EXPECT_NEAR(v[6], 0.25f, kTol);   // u
    EXPECT_NEAR(v[7], 0.5f, kTol);    // v
    // 頂点1
    EXPECT_NEAR(v[8], 10.0f, kTol);
    EXPECT_NEAR(v[9], 20.0f, kTol);
    EXPECT_NEAR(v[10], 30.0f, kTol);
    EXPECT_NEAR(v[11], 0.4f, kTol);
    EXPECT_NEAR(v[12], 0.5f, kTol);
    EXPECT_NEAR(v[13], 0.6f, kTol);
    EXPECT_NEAR(v[14], 0.75f, kTol);
    EXPECT_NEAR(v[15], 1.0f, kTol);
}

// doubleメッシュ: 単精度へ変換されて同じレイアウトで並ぶ
TEST(BuildInterleavedVertices, DoubleMesh_CastsToFloatWithSameLayout) {
    const auto mesh = MakeFullMesh<double>();

    const auto v = i_graph::BuildInterleavedVertices(mesh);

    ASSERT_EQ(v.size(), 16u);
    EXPECT_NEAR(v[0], 1.0f, kCastTol);
    EXPECT_NEAR(v[5], 0.3f, kCastTol);
    EXPECT_NEAR(v[7], 0.5f, kCastTol);
    EXPECT_NEAR(v[8], 10.0f, kCastTol);
    EXPECT_NEAR(v[15], 1.0f, kCastTol);
}



/**
 * 正常系 (境界値・退化)
 */

// 空メッシュ: 空の列が返る
TEST(BuildInterleavedVertices, EmptyMesh_ReturnsEmptyVector) {
    const i_num::TriangleMeshf mesh;

    const auto v = i_graph::BuildInterleavedVertices(mesh);

    EXPECT_TRUE(v.empty());
}

// 法線・UVチャンネルなし: 対応スロット (3-5, 6-7) がゼロ埋めされる
TEST(BuildInterleavedVertices, MissingChannels_ZeroFilled) {
    i_num::TriangleMeshf mesh;
    mesh.positions.resize(3, 1);
    mesh.positions << 1.0f, 2.0f, 3.0f;

    const auto v = i_graph::BuildInterleavedVertices(mesh);

    ASSERT_EQ(v.size(), 8u);
    EXPECT_NEAR(v[0], 1.0f, kTol);
    EXPECT_NEAR(v[1], 2.0f, kTol);
    EXPECT_NEAR(v[2], 3.0f, kTol);
    for (int k = 3; k < 8; ++k) {
        EXPECT_NEAR(v[k], 0.0f, kTol) << "slot " << k;
    }
}
