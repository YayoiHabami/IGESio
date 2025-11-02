/**
 * @file numerics/bounding_box.cpp
 * @brief ジオメトリエンティティ用のバウンディングボックス計算
 * @author Yayoi Habami
 * @date 2025-10-29
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/numerics/bounding_box.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/numerics/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
using igesio::Vector3d;
using igesio::numerics::BoundingBox;
using DT = BoundingBox::DirectionType;

constexpr double kInf = std::numeric_limits<double>::infinity();

}  // namespace



/**
 * コンストラクタ
 */

BoundingBox::BoundingBox()
    : control_(Vector3d::Zero()),
      directions_{Vector3d::UnitX(), Vector3d::UnitY(), Vector3d::UnitZ()},
      sizes_{0.0, 0.0, 0.0} {
}

BoundingBox::BoundingBox(
        const Vector3d& control,
        const std::array<Vector3d, 3>& directions,
        const std::array<double, 3>& sizes,
        const std::array<bool, 3>& is_line) {
    SetControl(control);
    SetDirections(directions);
    SetSizes(sizes, is_line);
}

BoundingBox::BoundingBox(const Vector3d& control,
                         const std::array<double, 3>& sizes,
                         const std::array<bool, 3>& is_line) {
    SetControl(control);
    SetDirections({Vector3d::UnitX(), Vector3d::UnitY(), Vector3d::UnitZ()});
    SetSizes(sizes, is_line);
}

BoundingBox::BoundingBox(
        const Vector3d& control,
        const std::array<Vector3d, 2>& directions,
        const std::array<double, 2>& sizes,
        const std::array<bool, 2>& is_line)
        : BoundingBox(control,
                      {directions[0], directions[1],
                       directions[0].cross(directions[1]).normalized()},
                      {sizes[0], sizes[1], 0.0},
                      {is_line[0], is_line[1], false}) {}

BoundingBox::BoundingBox(const Vector3d& control,
                         const std::array<double, 2>& sizes,
                         const std::array<bool, 2>& is_line)
        : BoundingBox(control,
                      {Vector3d::UnitX(), Vector3d::UnitY()},
                      sizes, is_line) {}



/**
 * パラメータの取得・設定
 */

void BoundingBox::SetControl(const Vector3d& control) {
    // controlが無限大の成分を持つ場合はエラー
    for (int i = 0; i < 3; ++i) {
        if (std::isinf(control(i))) {
            throw std::invalid_argument(
                "BoundingBox: Control point cannot have infinite components, "
                "but got " + ToString(control.transpose()));
        }
    }
    control_ = control;
}

void BoundingBox::SetDirections(const std::array<Vector3d, 3>& directions) {
    const auto& [d0, d1, d2] = directions;

    // D0, D1, D2が単位ベクトルであることを確認
    if (!i_num::IsApproxEqual(d0.norm(), 1.0) || !i_num::IsApproxEqual(d1.norm(), 1.0) ||
        !i_num::IsApproxEqual(d2.norm(), 1.0)) {
        throw std::invalid_argument(
            "BoundingBox: Directions D0, D1 and D2 must be unit vectors, "
            "but got D0 norm = " + std::to_string(d0.norm()) +
            ", D1 norm = " + std::to_string(d1.norm()) +
            ", D2 norm = " + std::to_string(d2.norm()));
    }

    // D0・D1が直交していることを確認
    if (i_num::IsApproxZero(d0.norm()) || i_num::IsApproxZero(d1.norm()) ||
        !i_num::IsApproxZero(d0.dot(d1))) {
        // D0とD1は非ゼロかつ必ず直交していること
        throw std::invalid_argument(
            "BoundingBox: Directions D0 and D1 must be non-zero and orthogonal, "
            "but got D0 = " + ToString(d0.transpose()) +
            ", D1 = " + ToString(d1.transpose()));
    }
    directions_[0] = d0.normalized();
    directions_[1] = d1.normalized();

    // D2 = D0×D1 であることを確認
    auto d0xd1 = directions_[0].cross(directions_[1]);
    if (!i_num::IsApproxEqual(d0xd1, d2)) {
        throw std::invalid_argument(
            "BoundingBox: Direction D2 must be parallel to D0 x D1 with positive scale, "
            "but got D2 = " + ToString(d2.transpose()));
    }
    directions_[2] = d2;
}

void BoundingBox::SetDirections2D(const std::array<Vector3d, 2>& directions) {
    const auto& [d0, d1] = directions;

    SetDirections({d0, d1, d0.cross(d1).normalized()});
    sizes_[2] = 0.0;
}

std::array<double, 3> BoundingBox::GetSizes() const {
    // sizes_[i]が-∞の場合は∞に変換して返す
    std::array<double, 3> result;
    for (size_t i = 0; i < 3; ++i) {
        if (sizes_[i] == -kInf) {
            result[i] = kInf;
        } else {
            result[i] = sizes_[i];
        }
    }
    return result;
}

void BoundingBox::SetSizes(const std::array<double, 3>& sizes,
                           const std::array<bool, 3>& is_line) {
    for (size_t i = 0; i < 3; ++i) {
        SetSize(i, sizes[i], is_line[i]);
    }
}

void BoundingBox::SetSize(const size_t index, const double size, const bool is_line) {
    // indexの範囲チェック
    if (index >= 3) {
        throw std::out_of_range(
            "BoundingBox: Index must be between 0 and 2, but got " +
            std::to_string(index));
    }

    // sizesに負の値が含まれている場合はエラー
    if (size < 0.0) {
        throw std::invalid_argument(
            "BoundingBox: Size in " + std::to_string(index + 1) + " direction must be "
            "non-negative, but got " + std::to_string(size));
    } else if (index != 2 && i_num::IsApproxZero(size)) {
        // sizes[2]以外が0の場合はエラー
        throw std::invalid_argument(
            "BoundingBox: Size in " + std::to_string(index + 1) + " direction must be "
            "non-zero, but got " + std::to_string(size));
    }

    if (is_line) {
        sizes_[index] = -kInf;
    } else {
        sizes_[index] = size;
    }
}

void BoundingBox::SetSizes2D(const std::array<double, 2>& sizes,
                             const std::array<bool, 2>& is_line) {
    for (size_t i = 0; i < 2; ++i) {
        SetSize(i, sizes[i], is_line[i]);
    }
    sizes_[2] = 0.0;
}

std::array<BoundingBox::DirectionType, 3> BoundingBox::GetDirectionTypes() const {
    std::array<DirectionType, 3> direction_types;
    for (size_t i = 0; i < 3; ++i) {
        if (sizes_[i] == -kInf) {
            direction_types[i] = DirectionType::kLine;
        } else if (sizes_[i] == kInf) {
            direction_types[i] = DirectionType::kRay;
        } else {
            direction_types[i] = DirectionType::kSegment;
        }
    }
    return direction_types;
}

void BoundingBox::Translate(const Vector3d& vec) {
    // vecが無限大の成分を持つ場合はエラー
    if (std::isinf(vec.norm())) {
        throw std::invalid_argument(
            "BoundingBox: Translation vector cannot have infinite components, "
            "but got " + ToString(vec.transpose()));
    }

    control_ += vec;
}

void BoundingBox::Rotate(const Matrix3d& rot) {
    Rotate(rot, GetControl());
}

void BoundingBox::Rotate(const Matrix3d& rot, const Vector3d& center) {
    // rotがほぼIdentityであれば何もしない
    if (IsApproxEqual(rot, Matrix3d::Identity()))  return;

    // 回転行列の検証 (直交行列かつ行列式が1)
    auto should_be_identity = rot * rot.transpose();
    if (!i_num::IsApproxEqual(should_be_identity, Matrix3d::Identity()) ||
        !i_num::IsApproxEqual(rot.determinant(), 1.0)) {
        throw std::invalid_argument(
            "BoundingBox: Rotation matrix must be orthogonal with determinant 1, "
            "but got matrix:\n" + ToString(rot));
    }

    // centerが無限大の成分を持つ場合はエラー
    if (!center.allFinite()) {
        throw std::invalid_argument(
            "BoundingBox: Rotation center cannot have infinite components, "
            "but got " + ToString(center.transpose()));
    }

    // centerを中心に回転
    if (!(control_ - center).isZero(kGeometryTolerance)) {
        control_ = rot * (control_ - center) + center;
    }

    // 方向ベクトルを回転
    for (auto& dir : directions_) {
        dir = rot * dir;
    }
}

void BoundingBox::Transform(const Matrix3d& rot, const Vector3d& vec) {
    Rotate(rot);
    Translate(vec);
}

void BoundingBox::ExpandToInclude(const BoundingBox& other) {
    auto vertices = other.GetVertices();
    auto own_vertices = GetVertices();
    vertices.insert(vertices.end(), own_vertices.begin(), own_vertices.end());

    if (!IsAxisAligned()) {
        // 自身が軸平行でない場合、ローカル座標系に変換する
        auto w2l = GetWorldToLocalRotation();
        for (size_t i = 0; i < vertices.size(); ++i) {
            vertices[i] = w2l * (vertices[i] - control_);
        }
    } else {
        // 軸平行の場合は基準点を引くだけでOK
        for (size_t i = 0; i < vertices.size(); ++i) {
            vertices[i] -= control_;
        }
    }

    // 各成分の最小値・最大値を計算
    Vector3d min = {kInf, kInf, kInf};
    Vector3d max = {-kInf, -kInf, -kInf};
    for (const auto& vertex : vertices) {
        for (int i = 0; i < 3; ++i) {
            if (vertex(i) < min(i)) min(i) = vertex(i);
            if (vertex(i) > max(i)) max(i) = vertex(i);
        }
    }

    // エラーケース
    // - min=max=±∞
    // - minまたはmaxのいずれかの成分がNaN
    // - min=-∞かつmax<+∞ (D0~D2の反転が必要)
    for (int i = 0; i < 3; ++i) {
        if (std::isnan(min(i)) || std::isnan(max(i))) {
            throw std::invalid_argument(
                "BoundingBox::ExpandToInclude: Cannot expand "
                "to include box with NaN vertices.");
        }
        if (min(i) == max(i) && !std::isfinite(min(i))) {
            throw std::invalid_argument(
                "BoundingBox::ExpandToInclude: Cannot expand "
                "to include box with infinite vertices only.");
        }
        if (min(i) == -kInf && max(i) < kInf) {
            throw std::invalid_argument(
                "BoundingBox::ExpandToInclude: Cannot expand "
                "to include box requiring direction reversal.");
        }
    }

    // 基準点とサイズの更新
    for (size_t i = 0; i < 3; ++i) {
        if (max(i) == kInf && min(i) == -kInf) {
            // kLine (基準点は更新しない)
            sizes_[i] = -kInf;
        } else {
            sizes_[i] = max(i) - min(i);
            control_ += min(i) * directions_[i];
        }
    }
}



/**
 * 状態の取得
 */

bool BoundingBox::IsEmpty() const {
    return IsApproxZero(sizes_[0]) &&
           IsApproxZero(sizes_[1]) &&
           IsApproxZero(sizes_[2]);
}

bool BoundingBox::Is2D() const {
    return IsApproxZero(sizes_[2]);
}

bool BoundingBox::IsOnZPlane() const {
    if (!Is2D()) return false;

    // d2がz平面に垂直であることを確認
    Vector3d z_axis = Vector3d::UnitZ();
    if (!IsApproxOne(std::abs(directions_[2].dot(z_axis)))) {
        return false;
    }

    // p0.z() == 0 を確認
    if (!IsApproxZero(control_.z())) {
        return false;
    }

    return true;
}

bool BoundingBox::IsFinite() const {
    for (const auto& size : sizes_) {
        if (size == -kInf || size == kInf) {
            return false;
        }
    }
    return true;
}

std::vector<Vector3d> BoundingBox::GetVertices() const {
    auto n_vertices = Is2D() ? 4 : 8;
    auto [s0, s1, s2] = GetSizes();
    auto [t0, t1, t2] = GetDirectionTypes();

    std::vector<Vector3d> v;
    v.reserve(n_vertices);
    for (size_t i = 0; i < n_vertices; ++i) {
        Vector3d vertex = control_;
        if (i % 2 == 1) {
            // D0方向の加算
            vertex += s0 * directions_[0];
        } else if (t0 == DirectionType::kLine) {
            // D0が無限直線の場合、D0方向の負の無限大成分を加算
            vertex += -s0 * directions_[0];
        }

        if (i % 4 >= 2) {
            // D1方向の加算
            vertex += s1 * directions_[1];
        } else if (t1 == DirectionType::kLine) {
            // D1が無限直線の場合、D1方向の負の無限大成分を加算
            vertex += -s1 * directions_[1];
        }

        if (i >= 4) {
            // D2方向の加算
            vertex += s2 * directions_[2];
        } else if (t2 == DirectionType::kLine) {
            // D2が無限直線の場合、D2方向の負の無限大成分を加算
            vertex += -s2 * directions_[2];
        }

        // NaNの除去: 上記計算ではinf*0によりNaNが発生する場合があるが、
        //            これを0に置き換える (0の場合s_iにより延伸される成分はないため)
        for (int dim = 0; dim < 3; ++dim) {
            if (std::isnan(vertex(dim))) vertex(dim) = 0.0;
        }

        v.push_back(vertex);
    }

    return v;
}

std::vector<Vector3d> BoundingBox::GetFiniteVertices() const {
    std::vector<Vector3d> all_vertices = GetVertices();
    std::vector<Vector3d> finite_vertices;
    for (const auto& vertex : all_vertices) {
        if (std::isfinite(vertex.x()) &&
            std::isfinite(vertex.y()) &&
            std::isfinite(vertex.z())) {
            finite_vertices.push_back(vertex);
        } else {
            // 1つでも無限大の成分があれば空のベクターを返す
            return {};
        }
    }
    return finite_vertices;
}



/**
 * 包含・交差判定
 */

bool BoundingBox::Contains(const Vector3d& point) const {
    Vector3d local_point;
    if (!IsAxisAligned()) {
        auto w2l = GetWorldToLocalRotation();
        local_point = w2l * (point - control_);
    } else {
        local_point = point - control_;
    }

    return ContainsLocalPoint(local_point);
}

bool BoundingBox::Contains(const BoundingBox& other) const {
    std::vector<Vector3d> vertices;

    Matrix3d w2l;
    if (!IsAxisAligned()) {
        w2l = GetWorldToLocalRotation();
    }

    // すべての頂点が包含されているか確認
    for (const auto& vertex_w : other.GetVertices()) {
        auto vertex = vertex_w - control_;
        if (!IsAxisAligned()) {
            vertex = w2l * vertex;
        }

        if (!ContainsLocalPoint(vertex)) {
            return false;
        }
    }
    return true;
}

bool BoundingBox::Intersects(const Vector3d& start_w, const Vector3d& end_w,
                             const DirectionType type) const {
    if (!start_w.allFinite() || !end_w.allFinite()) {
        throw std::invalid_argument(
            "BoundingBox: Start and end points cannot have infinite components or NaN values, "
            "but got start " + ToString(start_w.transpose()) +
            ", end " + ToString(end_w.transpose()));
    }
    if (i_num::IsApproxEqual(start_w, end_w)) {
        throw std::invalid_argument(
            "BoundingBox: Start and end points cannot be the same, "
            "but got " + ToString(start_w.transpose()));
    }

    // 直線の方向ベクトルと L(t) の値域を計算
    // L(t) = start + t * dir
    std::pair<double, double> t_range = {0.0, 1.0};  // デフォルトは線分
    if (type == DirectionType::kRay) {
        t_range.second = kInf;
    } else if (type == DirectionType::kLine) {
        t_range = {-kInf, kInf};
    }
    Vector3d start = start_w - control_;
    Vector3d end = end_w - control_;
    if (!IsAxisAligned()) {
        // 自身が軸平行でない場合、ローカル座標系に変換する
        auto w2l = GetWorldToLocalRotation();
        start = w2l * start;
        end = w2l * end;
    }
    Vector3d dir = end - start;

    // Slab法による交差判定
    // アルゴリズムについては docs/numerics/bounding_box.md#Slab法による交差判定 を参照
    double t_inf_min = -kInf;
    double t_inf_max = kInf;

    auto box_types = GetDirectionTypes();
    auto box_sizes = GetSizes();

    for (int i = 0; i < 3; ++i) {
        double i_min = 0.0;
        double i_max = box_sizes[i];
        if (box_types[i] == DirectionType::kLine) {
            i_min = -kInf;
            i_max = kInf;
        } else if (box_types[i] == DirectionType::kRay) {
            i_max = kInf;
        }

        if (i_num::IsApproxZero(dir(i))) {
            // 直線がスラブに平行
            if (!(IsApproxLEQ(i_min, start(i), kGeometryTolerance) &&
                  IsApproxLEQ(start(i), i_max, kGeometryTolerance))) {
                // スラブの外側にある場合は交差しない
                return false;
            }
            // スラブの内側にある場合はこの軸方向の制約はない
            // t_near, t_farは更新しない
        } else {
            double t1 = (i_min - start(i)) / dir(i);
            double t2 = (i_max - start(i)) / dir(i);

            double t_near_i = std::min(t1, t2);
            double t_far_i = std::max(t1, t2);

            t_inf_min = std::max(t_inf_min, t_near_i);
            t_inf_max = std::min(t_inf_max, t_far_i);
        }
    }

    if (!IsApproxLEQ(t_inf_min, t_inf_max)) {
        return false;
    }

    // 直線の種類に応じたパラメータ範囲と交差区間の積を計算
    double t_final_min = std::max(t_inf_min, t_range.first);
    double t_final_max = std::min(t_inf_max, t_range.second);

    return IsApproxLEQ(t_final_min, t_final_max);
}



/**
 * private functions
 */

bool BoundingBox::IsAxisAligned() const {
    return IsApproxEqual(directions_[0], Vector3d::UnitX()) &&
           IsApproxEqual(directions_[1], Vector3d::UnitY()) &&
           IsApproxEqual(directions_[2], Vector3d::UnitZ());
}

igesio::Matrix3d BoundingBox::GetLocalToWorldRotation() const {
    Matrix3d rot;
    rot.block<3, 1>(0, 0) = directions_[0];
    rot.block<3, 1>(0, 1) = directions_[1];
    rot.block<3, 1>(0, 2) = directions_[2];
    return rot;
}

igesio::Matrix3d BoundingBox::GetWorldToLocalRotation() const {
    Matrix3d rot;
    rot.block<3, 1>(0, 0) = directions_[0];
    rot.block<3, 1>(0, 1) = directions_[1];
    rot.block<3, 1>(0, 2) = directions_[2];
    return rot.inverse();
}

bool BoundingBox::ContainsLocalPoint(const Vector3d& local_point) const {
    auto types = GetDirectionTypes();
    auto sizes = GetSizes();
    for (size_t i = 0; i < 3; ++i) {
        // kLineの場合は常に包含
        if (types[i] == DirectionType::kLine) continue;

        // kRay, kSegmentの場合は0 <= local_point <= sizeを確認
        // sizes[i]が+∞の場合もこの条件で正しく動作する
        if (!(IsApproxLEQ(0.0, local_point(i), kGeometryTolerance) &&
              IsApproxLEQ(local_point(i), sizes[i], kGeometryTolerance))) {
            return false;
        }
    }
    return true;
}
