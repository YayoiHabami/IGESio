/**
 * @file tests/graphics/test_surface_mesh.cpp
 * @brief 汎用曲面メッシュ生成 (BuildGeneralSurfaceMesh) のテスト
 * @author Yayoi Habami
 * @date 2026-06-08
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   igesio::graphics::BuildGeneralSurfaceMesh()
 *   - ① 退化点(apex)で三角形が落ちず穴が開かないこと
 *        (母線末端が軸上のSurfaceOfRevolution)
 *   - ② 母線/準線の角でリングが二重化され、稜線で法線が不連続になること
 *        (ハードエッジ化)
 *   - 退行: 滑らかな曲面では二重化が起きず、満杯のメッシュが生成されること
 *
 * NOTE:
 *   GL非依存の純粋関数のため、モックGLを介さず直接メッシュデータを検証する.
 *
 * TODO:
 *   - u方向に閉じた準線/母線 (閉ループ) の継ぎ目 (u=0/1) に当たる角は、
 *     パラメータ境界扱いのため二重化されない (現メッシャの既知の限界).
 *     u閉境界の溶接は別タスクであり、本テストの対象外.
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/entities/surfaces/tabulated_cylinder.h"
#include "igesio/graphics/surfaces/surface_mesh.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_graph = igesio::graphics;
using igesio::Vector2d;
using igesio::Vector3d;
using igesio::kPi;
using i_graph::GeneralSurfaceMesh;

/// @brief 分割数 (u, v); v方向はテストの可読性のため非対称にとる
constexpr int kUDiv = 12;
constexpr int kVDiv = 16;



/**
 * テスト用メッシュのファクトリ関数
 *
 * 回転軸はY軸とし、母線はXY平面 (Y軸を含む) 内に置く。
 * radius = |x|, height = y で、Y軸周りに回転させる。
 */

/// @brief 円錐 (片端apex): 母線 (0,2,0)[軸上=頂点]→(2,0,0)
GeneralSurfaceMesh BuildConeMesh() {
    auto axis = i_ent::MakeLine(Vector3d{0., 0., 0.}, Vector3d{0., 1., 0.});
    auto gen = i_ent::MakeLine(Vector3d{0., 2., 0.}, Vector3d{2., 0., 0.});
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    return i_graph::BuildGeneralSurfaceMesh(*surf, kUDiv, kVDiv);
}

/// @brief 球 (両端apex): 半円母線 中心(0,0) 半径2 (-π/2..π/2)、端点はY軸上
GeneralSurfaceMesh BuildSphereMesh() {
    auto axis = i_ent::MakeLine(Vector3d{0., -2., 0.}, Vector3d{0., 2., 0.});
    auto gen = i_ent::MakeCircularArc(Vector2d{0., 0.}, 2.0, -kPi / 2., kPi / 2.);
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    return i_graph::BuildGeneralSurfaceMesh(*surf, kUDiv, kVDiv);
}

/// @brief 段付きSoR (角2つ, apexなし): 母線 (1,0)-(1,2)-(2,2)-(2,4)
GeneralSurfaceMesh BuildSteppedSoRMesh() {
    auto axis = i_ent::MakeLine(Vector3d{0., 0., 0.}, Vector3d{0., 1., 0.});
    auto gen = i_ent::MakeLinearPath(std::vector<Vector3d>{
        Vector3d{1., 0., 0.}, Vector3d{1., 2., 0.},
        Vector3d{2., 2., 0.}, Vector3d{2., 4., 0.}});
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    return i_graph::BuildGeneralSurfaceMesh(*surf, kUDiv, kVDiv);
}

/// @brief 円筒 (滑らか, apex/角なし): 母線 (1,0)-(1,2) を回転
GeneralSurfaceMesh BuildCylinderMesh() {
    auto axis = i_ent::MakeLine(Vector3d{0., 0., 0.}, Vector3d{0., 1., 0.});
    auto gen = i_ent::MakeLine(Vector3d{1., 0., 0.}, Vector3d{1., 2., 0.});
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    return i_graph::BuildGeneralSurfaceMesh(*surf, kUDiv, kVDiv);
}

/// @brief L字準線の平行曲面 (角1つ): (-3,0,0)-(0,0,0)-(0,3,0) を+Zへ押し出し
GeneralSurfaceMesh BuildLTabCylMesh() {
    auto dir = i_ent::MakeLinearPath(std::vector<Vector3d>{
        Vector3d{-3., 0., 0.}, Vector3d{0., 0., 0.}, Vector3d{0., 3., 0.}});
    auto surf = i_ent::MakeExtrudedSurface(dir, Vector3d{0., 0., 5.});
    return i_graph::BuildGeneralSurfaceMesh(*surf, kUDiv, kVDiv);
}



/**
 * 検証ヘルパー
 */

/// @brief 頂点列colの法線の長さを返す
float NormalLength(const GeneralSurfaceMesh& m, const int col) {
    return m.mesh.normals.col(col).norm();
}

/// @brief 「位置がほぼ一致するが法線が有意に異なる」頂点ペアの数を数える
/// @note 折れ目の二重化リング (ハードエッジ) の証跡. v継ぎ目(同位置・同法線)や
///       通常頂点は数えない. apexを含む曲面では退化リングも該当するため、
///       本ヘルパでの②検証はapexを持たない曲面に限定する
int CountHardEdgePairs(const GeneralSurfaceMesh& m) {
    const int n = m.u_row_count * (m.v_div + 1);
    // 位置一致の閾値 (二重化リングは同一uのため位置は厳密一致)
    constexpr float kPosSq = 1e-6f;
    // 法線が10°以上異なれば「不連続」とみなす
    constexpr float kCosThresh = 0.985f;
    int count = 0;
    for (int a = 0; a < n; ++a) {
        for (int b = a + 1; b < n; ++b) {
            const float dist_sq =
                (m.mesh.positions.col(a) - m.mesh.positions.col(b))
                    .squaredNorm();
            if (dist_sq > kPosSq) continue;
            const float dot = m.mesh.normals.col(a).dot(m.mesh.normals.col(b));
            if (dot < kCosThresh) ++count;
        }
    }
    return count;
}

}  // namespace



/**
 * ① 退化点(apex)で穴が開かないこと
 */

// 円錐: apexの三角形が落ちず、満杯 (2·(rows-1)·v_div) の三角形が生成される
TEST(SurfaceMeshTest, FullTriangulationNoHoleAtApex_ForCone) {
    const auto mesh = BuildConeMesh();
    // 母線=直線 (角なし) のため行数は u_div+1
    EXPECT_EQ(mesh.u_row_count, kUDiv + 1);
    const std::size_t expected_tris =
        static_cast<std::size_t>(2 * (mesh.u_row_count - 1) * mesh.v_div);
    EXPECT_EQ(mesh.mesh.indices.size(), expected_tris * 3);
}

// 球(両極apex): 参照される全頂点の法線が長さ≈1 (ゼロ法線が混入しない)
TEST(SurfaceMeshTest, AllReferencedNormalsAreUnit_ForSphere) {
    const auto mesh = BuildSphereMesh();
    ASSERT_FALSE(mesh.mesh.indices.empty());
    for (const auto idx : mesh.mesh.indices) {
        EXPECT_NEAR(NormalLength(mesh, static_cast<int>(idx)), 1.0f, 1e-3f);
    }
}

// 球(両極apex): 満杯の三角形が生成される (両極で穴が開かない)
TEST(SurfaceMeshTest, FullTriangulationNoHoleAtApex_ForSphere) {
    const auto mesh = BuildSphereMesh();
    EXPECT_EQ(mesh.u_row_count, kUDiv + 1);  // 半円母線は角なし
    const std::size_t expected_tris =
        static_cast<std::size_t>(2 * (mesh.u_row_count - 1) * mesh.v_div);
    EXPECT_EQ(mesh.mesh.indices.size(), expected_tris * 3);
}



/**
 * ② 角でハードエッジ (二重化リング・法線不連続) が生成されること
 */

// 段付きSoR: 角の位置に「同位置・異法線」の頂点ペアが現れる
TEST(SurfaceMeshTest, HardEdgeRingAtCorner_ForSteppedSoR) {
    const auto mesh = BuildSteppedSoRMesh();
    // 角2つ -> 行数は基準(u_div+1)より増える
    EXPECT_GT(mesh.u_row_count, kUDiv + 1);
    EXPECT_GT(CountHardEdgePairs(mesh), 0);
}

// L字準線の平行曲面: 角の位置にハードエッジが現れる
TEST(SurfaceMeshTest, HardEdgeRingAtCorner_ForLShapedTabCyl) {
    const auto mesh = BuildLTabCylMesh();
    EXPECT_GT(mesh.u_row_count, kUDiv + 1);
    EXPECT_GT(CountHardEdgePairs(mesh), 0);
}



/**
 * 退行: 滑らかな曲面ではハードエッジが生成されないこと
 */

// 円筒: 二重化なし (行数は基準どおり)・ハードエッジペアなし・満杯メッシュ
TEST(SurfaceMeshTest, SmoothCylinderHasNoHardEdgeAndFullMesh) {
    const auto mesh = BuildCylinderMesh();
    EXPECT_EQ(mesh.u_row_count, kUDiv + 1);
    EXPECT_EQ(CountHardEdgePairs(mesh), 0);
    const std::size_t expected_tris =
        static_cast<std::size_t>(2 * (mesh.u_row_count - 1) * mesh.v_div);
    EXPECT_EQ(mesh.mesh.indices.size(), expected_tris * 3);
}
