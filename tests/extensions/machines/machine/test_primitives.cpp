/**
 * @file tests/extensions/machines/machine/test_primitives.cpp
 * @brief machines拡張のプリミティブ形状メッシュ (machine/primitives) のテスト
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeBoxMesh / MakeCylinderMesh / MakePrimitiveMesh
 *       - 正常系: 頂点数・三角形数、発散定理による体積、AABB、`numerics::Validate`
 *       - 法線: 三角形の幾何法線と頂点法線の一致、外向き (重心方向との内積が正)、
 *         円柱の側面が半径方向・上下面が±z
 *       - 閉性: 位置の一致する頂点同士で有向辺が対で相殺すること
 *         (平面部は頂点を共有しないため、位置をキーに照合する)
 *       - 分岐: `MakePrimitiveMesh`のkBox/kCylinder
 *       - 異常系: 非正の寸法、分割数の下限 (2は例外・3は受理)
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/numerics/meshes/algorithms/inspection.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/primitives.h"

namespace {

namespace mc = igesio::extensions::machines;
namespace i_num = igesio::numerics;
using igesio::Vector3d;
using i_num::TriangleMeshd;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief 体積の許容誤差 (三角形ごとの符号付き体積の総和のため緩める)
constexpr double kVolumeTol = 1e-6;
/// @brief 位置をキー化するときの量子化 (1e-6 mm)
constexpr double kPositionQuantum = 1e6;

/// @brief 三角形の3頂点を返す
std::array<Vector3d, 3> Triangle(const TriangleMeshd& mesh, const std::size_t t) {
    return {mesh.positions.col(mesh.indices[3 * t]),
            mesh.positions.col(mesh.indices[3 * t + 1]),
            mesh.positions.col(mesh.indices[3 * t + 2])};
}

/// @brief 発散定理による符号付き体積 (外向きなら正)
double SignedVolume(const TriangleMeshd& mesh) {
    double volume = 0.0;
    for (std::size_t t = 0; t < mesh.TriangleCount(); ++t) {
        const auto [a, b, c] = Triangle(mesh, t);
        volume += a.dot(b.cross(c)) / 6.0;
    }
    return volume;
}

/// @brief 各三角形について、幾何法線が重心方向を向き、3頂点の法線が幾何法線と
///        揃っている (内積が`min_alignment`以上) ことを検証する
/// @param mesh 検証するメッシュ
/// @param min_alignment 頂点法線と幾何法線の内積の下限. 平面部のみのメッシュでは
///        1に近い値を、滑らかな側面 (円柱) では隣接面の角度のcosを与える
void ExpectOutwardNormals(const TriangleMeshd& mesh, const double min_alignment) {
    ASSERT_TRUE(mesh.HasNormals());
    for (std::size_t t = 0; t < mesh.TriangleCount(); ++t) {
        const auto [a, b, c] = Triangle(mesh, t);
        const Vector3d geometric = (b - a).cross(c - a).normalized();
        const Vector3d centroid = (a + b + c) / 3.0;
        EXPECT_GT(geometric.dot(centroid), 0.0) << "triangle " << t;
        for (std::size_t k = 0; k < 3; ++k) {
            const Vector3d normal = mesh.normals.col(mesh.indices[3 * t + k]);
            EXPECT_NEAR(normal.norm(), 1.0, kTol);
            EXPECT_GE(normal.dot(geometric), min_alignment)
                    << "triangle " << t << " vertex " << k << ": "
                    << normal.transpose() << " vs " << geometric.transpose();
        }
    }
}

/// @brief 位置の一致する頂点を同一視して、有向辺が対で相殺することを検証する
void ExpectClosed(const TriangleMeshd& mesh) {
    // 位置 → 正準頂点番号
    std::map<std::array<long long, 3>, int> canonical;
    const auto key_of = [&](const std::uint32_t index) {
        const Vector3d p = mesh.positions.col(index);
        const std::array<long long, 3> key = {
                std::llround(p.x() * kPositionQuantum),
                std::llround(p.y() * kPositionQuantum),
                std::llround(p.z() * kPositionQuantum)};
        const auto found = canonical.find(key);
        if (found != canonical.end()) return found->second;
        const int id = static_cast<int>(canonical.size());
        canonical.emplace(key, id);
        return id;
    };
    // 有向辺 (i→j) の出現数から (j→i) の出現数を引く. 閉じていれば全て0
    std::map<std::pair<int, int>, int> balance;
    for (std::size_t t = 0; t < mesh.TriangleCount(); ++t) {
        const std::array<int, 3> v = {key_of(mesh.indices[3 * t]),
                                      key_of(mesh.indices[3 * t + 1]),
                                      key_of(mesh.indices[3 * t + 2])};
        for (std::size_t k = 0; k < 3; ++k) {
            const int from = v[k];
            const int to = v[(k + 1) % 3];
            if (from < to) {
                ++balance[{from, to}];
            } else {
                --balance[{to, from}];
            }
        }
    }
    for (const auto& [edge, count] : balance) {
        EXPECT_EQ(count, 0) << "edge " << edge.first << "-" << edge.second;
    }
}

}  // namespace



// ---- 直方体 ----

TEST(PrimitivesTest, Box_VolumeAndBounds) {
    const TriangleMeshd mesh = mc::MakeBoxMesh(Vector3d(10.0, 20.0, 30.0));
    EXPECT_EQ(mesh.VertexCount(), 24u);
    EXPECT_EQ(mesh.TriangleCount(), 12u);
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);
    EXPECT_NEAR(SignedVolume(mesh), 6000.0, kVolumeTol);

    const Vector3d lo = mesh.positions.rowwise().minCoeff();
    const Vector3d hi = mesh.positions.rowwise().maxCoeff();
    EXPECT_TRUE(lo.isApprox(Vector3d(-5.0, -10.0, -15.0), kTol)) << lo.transpose();
    EXPECT_TRUE(hi.isApprox(Vector3d(5.0, 10.0, 15.0), kTol)) << hi.transpose();
    ExpectClosed(mesh);
}

TEST(PrimitivesTest, Box_NormalsFaceOutward) {
    const TriangleMeshd mesh = mc::MakeBoxMesh(Vector3d(10.0, 20.0, 30.0));
    ExpectOutwardNormals(mesh, 1.0 - kTol);
    // 各頂点の法線は座標軸の±方向で、その面の頂点座標の対応成分が半辺長に一致する
    const Vector3d half(5.0, 10.0, 15.0);
    for (Eigen::Index i = 0; i < static_cast<Eigen::Index>(mesh.VertexCount()); ++i) {
        const Vector3d n = mesh.normals.col(i);
        const Vector3d p = mesh.positions.col(i);
        EXPECT_NEAR(n.cwiseAbs().maxCoeff(), 1.0, kTol);
        EXPECT_NEAR(n.cwiseAbs().sum(), 1.0, kTol);
        EXPECT_NEAR(p.dot(n), n.cwiseAbs().dot(half), kTol) << "vertex " << i;
    }
}



// ---- 円柱 ----

TEST(PrimitivesTest, Cylinder_VolumeAndClosure) {
    const int n = 48;
    const double r = 5.0;
    const double h = 20.0;
    const TriangleMeshd mesh = mc::MakeCylinderMesh(r, h, n);
    EXPECT_EQ(mesh.VertexCount(), static_cast<std::size_t>(4 * n + 2));
    EXPECT_EQ(mesh.TriangleCount(), static_cast<std::size_t>(4 * n));
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);

    // 内接多角柱の体積 (1/2) n r² h sin(2π/n)
    const double expected = 0.5 * n * r * r * h * std::sin(mc::kFullTurn / n);
    EXPECT_NEAR(SignedVolume(mesh), expected, kVolumeTol);
    ExpectClosed(mesh);
}

TEST(PrimitivesTest, Cylinder_NormalsAndRange) {
    const double r = 5.0;
    const double h = 20.0;
    const TriangleMeshd mesh = mc::MakeCylinderMesh(r, h, 48);
    // 側面の頂点法線 (半径方向) は三角形の幾何法線から最大で1分割角ずれる
    ExpectOutwardNormals(mesh, std::cos(mc::kFullTurn / 48));

    int side_count = 0;
    int cap_count = 0;
    for (Eigen::Index i = 0; i < static_cast<Eigen::Index>(mesh.VertexCount()); ++i) {
        const Vector3d n = mesh.normals.col(i);
        const Vector3d p = mesh.positions.col(i);
        EXPECT_TRUE(std::abs(p.z() - h / 2.0) < kTol || std::abs(p.z() + h / 2.0) < kTol)
                << "vertex " << i << " z = " << p.z();
        if (std::abs(n.z()) < kTol) {
            // 側面: 法線は半径方向 (位置のxy成分を正規化したもの)
            ++side_count;
            const Vector3d radial(p.x(), p.y(), 0.0);
            EXPECT_NEAR(radial.norm(), r, kTol);
            EXPECT_TRUE(n.isApprox(radial / r, kTol)) << "vertex " << i;
        } else {
            // 上下面: 法線は±zで、面のz座標と同符号
            ++cap_count;
            EXPECT_NEAR(std::abs(n.z()), 1.0, kTol);
            EXPECT_GT(n.z() * p.z(), 0.0) << "vertex " << i;
        }
    }
    EXPECT_EQ(side_count, 2 * 48);
    EXPECT_EQ(cap_count, 2 * (48 + 1));
    EXPECT_NEAR(mesh.positions.row(2).minCoeff(), -10.0, kTol);
    EXPECT_NEAR(mesh.positions.row(2).maxCoeff(), 10.0, kTol);
}



// ---- PrimitiveSpecからの生成 ----

TEST(PrimitivesTest, Primitive_DispatchesByKind) {
    mc::PrimitiveSpec box;
    box.kind = mc::PrimitiveSpec::Kind::kBox;
    box.size = Vector3d(2.0, 3.0, 4.0);
    const TriangleMeshd box_mesh = mc::MakePrimitiveMesh(box);
    EXPECT_EQ(box_mesh.VertexCount(), 24u);
    EXPECT_NEAR(SignedVolume(box_mesh), 24.0, kVolumeTol);

    mc::PrimitiveSpec cylinder;
    cylinder.kind = mc::PrimitiveSpec::Kind::kCylinder;
    cylinder.radius = 1.0;
    cylinder.height = 2.0;
    const TriangleMeshd cylinder_mesh = mc::MakePrimitiveMesh(cylinder);
    // 既定の分割数48
    EXPECT_EQ(cylinder_mesh.VertexCount(), static_cast<std::size_t>(4 * 48 + 2));
    EXPECT_NEAR(SignedVolume(cylinder_mesh),
                0.5 * 48 * 2.0 * std::sin(mc::kFullTurn / 48), kVolumeTol);
}



// ---- 異常系 ----

TEST(PrimitivesTest, Invalid_ArgumentsThrow) {
    EXPECT_THROW(mc::MakeBoxMesh(Vector3d(0.0, 1.0, 1.0)), std::invalid_argument);
    EXPECT_THROW(mc::MakeBoxMesh(Vector3d(1.0, -1.0, 1.0)), std::invalid_argument);
    EXPECT_THROW(mc::MakeBoxMesh(Vector3d(1.0, 1.0, 0.0)), std::invalid_argument);
    EXPECT_NO_THROW(mc::MakeBoxMesh(Vector3d(1e-9, 1e-9, 1e-9)));

    EXPECT_THROW(mc::MakeCylinderMesh(0.0, 1.0), std::invalid_argument);
    EXPECT_THROW(mc::MakeCylinderMesh(1.0, 0.0), std::invalid_argument);
    EXPECT_THROW(mc::MakeCylinderMesh(1.0, 1.0, 2), std::invalid_argument);
    EXPECT_NO_THROW(mc::MakeCylinderMesh(1.0, 1.0, 3));

    mc::PrimitiveSpec spec;
    spec.kind = mc::PrimitiveSpec::Kind::kCylinder;  // radius = height = 0
    EXPECT_THROW(mc::MakePrimitiveMesh(spec), std::invalid_argument);
    spec.kind = mc::PrimitiveSpec::Kind::kBox;  // size = 0
    EXPECT_THROW(mc::MakePrimitiveMesh(spec), std::invalid_argument);
}
