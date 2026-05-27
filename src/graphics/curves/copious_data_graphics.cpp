/**
 * @file graphics/copious_data_graphics.cpp
 * @brief CopiousDataの描画用クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/copious_data_graphics.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "igesio/entities/curves/algorithms/curve_line_intersection.h"

namespace {

using CopiousDataGraphics = igesio::graphics::CopiousDataGraphics;
using CDType = igesio::entities::CopiousDataType;

}  // namespace

CopiousDataGraphics::CopiousDataGraphics(
        const std::shared_ptr<const entities::CopiousDataBase> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

CopiousDataGraphics::~CopiousDataGraphics() {
    Cleanup();
}

CopiousDataGraphics::CopiousDataGraphics(CopiousDataGraphics&& other) noexcept
        : EntityGraphics(std::move(other)),
          vbo_(other.vbo_),
          vertex_count_(other.vertex_count_) {
    other.vbo_ = 0;
    other.vertex_count_ = 0;
}

CopiousDataGraphics& CopiousDataGraphics::operator=(
        CopiousDataGraphics&& other) noexcept {
    if (this != &other) {
        EntityGraphics::operator=(std::move(other));
        vbo_ = other.vbo_;
        vertex_count_ = other.vertex_count_;
        other.vbo_ = 0;
        other.vertex_count_ = 0;
    }
    return *this;
}

void CopiousDataGraphics::Cleanup() {
    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        gl_->DeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    vertex_count_ = 0;
}

bool CopiousDataGraphics::IsDrawable() const {
    if (!EntityGraphics::IsDrawable()) return false;
    return vbo_ != 0 && vertex_count_ > 0;
}

void CopiousDataGraphics::Synchronize() {
    Cleanup();

    const auto& coords = entity_->Coordinates();
    const auto count = entity_->GetCount();

    if (count == 0) return;

    std::vector<float> vertices;
    vertices.reserve(count * 3);
    for (int i = 0; i < count; ++i) {
        vertices.push_back(static_cast<float>(coords.col(i).x()));
        vertices.push_back(static_cast<float>(coords.col(i).y()));
        vertices.push_back(static_cast<float>(coords.col(i).z()));
    }

    auto type = entity_->GetDataType();
    if (CDType::kPlanarPoints <= type && type <= CDType::kSextuples) {
        // formが1-3の場合は、点群 (Copious Data) として描画
        draw_mode_ = GL_POINTS;
        // 点のサイズをシェーダーで制御可能にする
        gl_->Enable(GL_PROGRAM_POINT_SIZE);
    } else {
        // それ以外の場合は折れ線として描画
        draw_mode_ = GL_LINE_STRIP;
    }

    vertex_count_ = count;
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_->BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                    vertices.data(), GL_STATIC_DRAW);

    gl_->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);

    gl_->BindBuffer(GL_ARRAY_BUFFER, 0);
    gl_->BindVertexArray(0);
}

std::vector<igesio::graphics::RayHit> CopiousDataGraphics::Intersect(
        const Ray& ray, const RayIntersectionParams& params) const {
    if (!entity_) return {};

    // 折れ線 (form 11-13) は接線を持つため、通常の曲線判定 (基底) に委譲する
    const auto type = entity_->GetDataType();
    const bool is_point_series =
            (CDType::kPlanarPoints <= type && type <= CDType::kSextuples);
    if (!is_point_series) {
        return EntityGraphics::Intersect(ray, params);
    }

    // 点列 (form 1-3): 各頂点とレイの近接判定を行う
    // 描画位置に一致させる: model = GetWorldTransform(), VBO = 定義空間座標
    const Vector3d p1 = ray.origin + ray.direction;
    constexpr auto kRay = numerics::BoundingBox::DirectionType::kRay;
    const igesio::Matrix4d m = GetWorldTransform().cast<double>();
    const auto& coords = entity_->Coordinates();
    const auto count = entity_->GetCount();

    std::vector<RayHit> result;
    for (size_t i = 0; i < count; ++i) {
        const Vector3d v = coords.col(i);
        const Vector3d world_pt = (m * v.homogeneous()).hnormalized();
        const auto hit = entities::IntersectPointWithLine(
                world_pt, ray.origin, p1, kRay, params.curve_hit_tolerance);
        if (hit) result.push_back({hit->position, hit->t_line});
    }

    std::sort(result.begin(), result.end(),
              [](const RayHit& a, const RayHit& b) {
                  return a.distance < b.distance;
              });
    return result;
}

void CopiousDataGraphics::DrawImpl(
        GLuint, const std::pair<float, float>&) const {
    if (draw_mode_ == GL_POINTS) {
        // 点のサイズを5ピクセルに設定
        gl_->PointSize(5.0f);
    }

    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, vertex_count_);
    gl_->BindVertexArray(0);
}
