/**
 * @file extensions/machines/machine/primitives.cpp
 * @brief 機械定義用のプリミティブ形状の三角形メッシュ生成
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/machine/primitives.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/units.h"

namespace igesio::extensions::machines {

namespace {

using igesio::Vector3d;
using igesio::numerics::TriangleMeshd;

/// @brief 円柱の分割数の下限
constexpr int kMinCylinderSegments = 3;

/// @brief メッシュの構築用クラス (頂点・法線・インデックスを順に追加する)
struct MeshBuilder {
    /// @brief 頂点位置
    std::vector<Vector3d> positions;
    /// @brief 頂点法線 (positionsと同数)
    std::vector<Vector3d> normals;
    /// @brief 三角形インデックス
    std::vector<std::uint32_t> indices;

    /// @brief 頂点を追加し、その番号を返す
    std::uint32_t AddVertex(const Vector3d& position, const Vector3d& normal) {
        positions.push_back(position);
        normals.push_back(normal);
        return static_cast<std::uint32_t>(positions.size() - 1);
    }

    /// @brief 三角形を追加する (反時計回りが表)
    void AddTriangle(const std::uint32_t a, const std::uint32_t b,
                     const std::uint32_t c) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    /// @brief 四角形を2つの三角形として追加する (a→b→c→dが反時計回り)
    void AddQuad(const std::uint32_t a, const std::uint32_t b,
                 const std::uint32_t c, const std::uint32_t d) {
        AddTriangle(a, b, c);
        AddTriangle(a, c, d);
    }

    /// @brief 構築したデータからメッシュを作る
    TriangleMeshd Build() const {
        TriangleMeshd mesh;
        const auto count = static_cast<Eigen::Index>(positions.size());
        mesh.positions.resize(3, count);
        mesh.normals.resize(3, count);
        for (Eigen::Index i = 0; i < count; ++i) {
            mesh.positions.col(i) = positions[static_cast<std::size_t>(i)];
            mesh.normals.col(i) = normals[static_cast<std::size_t>(i)];
        }
        mesh.indices = indices;
        return mesh;
    }
};

/// @brief 値が正でなければ例外を投げる
/// @param value 検査する値
/// @param name 例外文言に含める名前
void RequirePositive(const double value, const char* name) {
    if (!(value > 0.0)) {
        throw std::invalid_argument(
                std::string("Primitive: ") + name + " must be positive (got "
                + std::to_string(value) + ")");
    }
}

/// @brief 直方体の1面 (法線方向`normal`、面内の2軸`u`・`v`) を追加する
/// @param builder 構築先
/// @param half 各軸方向の半辺長
/// @param normal 面法線 (座標軸の±方向の単位ベクトル)
/// @param u 面内の第1軸 (単位ベクトル)
/// @param v 面内の第2軸 (単位ベクトル. u × v = normal となる向き)
void AddBoxFace(MeshBuilder& builder,
                const Vector3d& half, const Vector3d& normal,
                const Vector3d& u, const Vector3d& v) {
    // 面の中心から u・v の±方向へ半辺長だけ広げた4隅 (反時計回り)
    const Vector3d center = normal.cwiseProduct(half);
    const Vector3d du = u.cwiseProduct(half);
    const Vector3d dv = v.cwiseProduct(half);
    const std::uint32_t a = builder.AddVertex(center - du - dv, normal);
    const std::uint32_t b = builder.AddVertex(center + du - dv, normal);
    const std::uint32_t c = builder.AddVertex(center + du + dv, normal);
    const std::uint32_t d = builder.AddVertex(center - du + dv, normal);
    builder.AddQuad(a, b, c, d);
}

/// @brief 円柱の上面または下面を追加する
/// @param builder 構築先
/// @param ring 円周上の点 (z = 0 の平面上. 上面・下面でz座標を置き換える)
/// @param z 面のz座標
/// @param upward 上面 (法線+z) ならtrue、下面 (法線-z) ならfalse
void AddCylinderCap(MeshBuilder& builder,
                    const std::vector<Vector3d>& ring,
                    const double z, const bool upward) {
    const Vector3d normal(0.0, 0.0, upward ? 1.0 : -1.0);
    const std::uint32_t center = builder.AddVertex(Vector3d(0.0, 0.0, z), normal);
    std::vector<std::uint32_t> rim;
    rim.reserve(ring.size());
    for (const Vector3d& p : ring) {
        rim.push_back(builder.AddVertex(Vector3d(p.x(), p.y(), z), normal));
    }
    const std::size_t n = ring.size();
    for (std::size_t k = 0; k < n; ++k) {
        const std::uint32_t next = rim[(k + 1) % n];
        // 上面は+zから見て反時計回り、下面は-zから見て反時計回り (=上から時計回り)
        if (upward) {
            builder.AddTriangle(center, rim[k], next);
        } else {
            builder.AddTriangle(center, next, rim[k]);
        }
    }
}

}  // namespace



TriangleMeshd MakeBoxMesh(const Vector3d& size) {
    RequirePositive(size.x(), "size.x");
    RequirePositive(size.y(), "size.y");
    RequirePositive(size.z(), "size.z");
    const Vector3d half = size / 2.0;

    MeshBuilder builder;
    const Vector3d x = Vector3d::UnitX();
    const Vector3d y = Vector3d::UnitY();
    const Vector3d z = Vector3d::UnitZ();
    // 各面の面内軸は`u × v = 法線`となる組を選ぶ (外向きに反時計回り)
    AddBoxFace(builder, half, x, y, z);
    AddBoxFace(builder, half, -x, z, y);
    AddBoxFace(builder, half, y, z, x);
    AddBoxFace(builder, half, -y, x, z);
    AddBoxFace(builder, half, z, x, y);
    AddBoxFace(builder, half, -z, y, x);
    return builder.Build();
}

TriangleMeshd MakeCylinderMesh(const double radius, const double height,
                               const int segments) {
    RequirePositive(radius, "radius");
    RequirePositive(height, "height");
    if (segments < kMinCylinderSegments) {
        throw std::invalid_argument(
                "Primitive: segments must be at least "
                + std::to_string(kMinCylinderSegments) + " (got "
                + std::to_string(segments) + ")");
    }
    const auto n = static_cast<std::size_t>(segments);
    const double half_height = height / 2.0;

    // 円周上の点と半径方向の法線を1度だけ計算し、側面と上下面で同じ値を使う
    // (位置が一致し、閉じたメッシュとして扱えるようにする)
    std::vector<Vector3d> ring;
    std::vector<Vector3d> radial;
    ring.reserve(n);
    radial.reserve(n);
    for (std::size_t k = 0; k < n; ++k) {
        const double angle = kFullTurn * static_cast<double>(k) / static_cast<double>(n);
        radial.emplace_back(std::cos(angle), std::sin(angle), 0.0);
        ring.push_back(radial.back() * radius);
    }

    MeshBuilder builder;
    // 側面: 下リングと上リング (法線は半径方向)
    std::vector<std::uint32_t> bottom;
    std::vector<std::uint32_t> top;
    bottom.reserve(n);
    top.reserve(n);
    for (std::size_t k = 0; k < n; ++k) {
        bottom.push_back(builder.AddVertex(
                Vector3d(ring[k].x(), ring[k].y(), -half_height), radial[k]));
        top.push_back(builder.AddVertex(
                Vector3d(ring[k].x(), ring[k].y(), half_height), radial[k]));
    }
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t next = (k + 1) % n;
        builder.AddQuad(bottom[k], bottom[next], top[next], top[k]);
    }
    AddCylinderCap(builder, ring, half_height, true);
    AddCylinderCap(builder, ring, -half_height, false);
    return builder.Build();
}

TriangleMeshd MakePrimitiveMesh(const PrimitiveSpec& spec) {
    if (spec.kind == PrimitiveSpec::Kind::kCylinder) {
        return MakeCylinderMesh(spec.radius, spec.height);
    }
    return MakeBoxMesh(spec.size);
}

}  // namespace igesio::extensions::machines
