/**
 * @file graphics/curves/point_graphics.cpp
 * @brief 点の描画用クラス
 * @author Yayoi Habami
 * @date 2025-10-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/point_graphics.h"

#include <memory>
#include <utility>
#include <vector>

#include "igesio/entities/curves/algorithms/curve_line_intersection.h"

namespace {

using igesio::graphics::PointGraphics;

}  // namespace



PointGraphics::PointGraphics(
        const std::shared_ptr<const entities::Point>& entity,
        const std::shared_ptr<IOpenGL>& gl)
        : EntityGraphics(entity, gl, ShaderType::kPoint, false) {
    Synchronize();
}

PointGraphics::~PointGraphics() {
    Cleanup();
}

PointGraphics::PointGraphics(PointGraphics&& other) noexcept
        : EntityGraphics(std::move(other)),
          vbo_(other.vbo_) {
    // ムーブ元のリソース所有権を放棄させ、デストラクタで解放されないようにする
    other.vbo_ = 0;
}

PointGraphics& PointGraphics::operator=(PointGraphics&& other) noexcept {
    if (this != &other) {
        // 基底クラスのムーブ代入演算子を呼び出す
        EntityGraphics::operator=(std::move(other));

        // メンバをムーブ
        vbo_ = other.vbo_;

        // ムーブ元のリソース所有権を放棄
        other.vbo_ = 0;
    }
    return *this;
}

void PointGraphics::Cleanup() {
    EntityGraphics::Cleanup();
    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
}

bool PointGraphics::IsDrawable() const {
    if (!EntityGraphics::IsDrawable()) return false;

    // 追加の条件をチェック
    return vbo_ != 0;
}

bool PointGraphics::CanIntersect() const {
    return entity_ != nullptr;
}

std::vector<igesio::graphics::RayHit> PointGraphics::Intersect(
        const Ray& ray, const RayIntersectionParams& params) const {
    if (!entity_) return {};

    const Vector3d p1 = ray.origin + ray.direction;
    constexpr auto kRay = numerics::BoundingBox::DirectionType::kRay;

    // 描画位置に一致させる: model = GetWorldTransform(), VBO = 定義空間座標
    const igesio::Matrix4d m = GetWorldTransformD();
    const Vector3d world_pt =
            (m * entity_->GetDefinedPosition().homogeneous()).hnormalized();

    const auto hit = entities::IntersectPointWithLine(
            world_pt, ray.origin, p1, kRay, params.curve_hit_tolerance);
    if (hit) return {{hit->position, hit->t_line}};
    return {};
}

igesio::graphics::SelectionSamples PointGraphics::GetSelectionSamples(
        const SelectionSampleParams&) const {
    if (!entity_) return {};

    // 描画位置に一致させる: model = GetWorldTransform(), 座標 = 定義空間
    const igesio::Matrix4d m = GetWorldTransformD();
    const Vector3d world_pt =
            (m * entity_->GetDefinedPosition().homogeneous()).hnormalized();

    SelectionSamples result;
    result.points.push_back(world_pt);
    return result;
}



/**
 * protected
 */

void PointGraphics::DrawImpl(
        gl::Uint shader, const std::pair<float, float>& viewport) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, 1);
    gl_->BindVertexArray(0);
}

void PointGraphics::DoSynchronize() {
    // 既存のリソースを開放
    Cleanup();

    // OpenGLバッファのセットアップ
    // TODO: Subfigure Definitionが設定されている場合の処理
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(gl::kArrayBuffer, vbo_);
    Vector3f position = entity_->GetDefinedPosition().cast<float>();
    gl_->BufferData(gl::kArrayBuffer, sizeof(float) * 3, position.data(), gl::kStaticDraw);
    draw_mode_ = gl::kPoints;

    gl_->VertexAttribPointer(0, 3, gl::kFloat, gl::kFalse, 3 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);
    gl_->BindBuffer(gl::kArrayBuffer, 0);
    gl_->BindVertexArray(0);
}
