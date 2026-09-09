/**
 * @file extensions/machines/core/rotation.cpp
 * @brief machines拡張の回転3形式の解釈と剛体変換の小関数
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/core/rotation.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

namespace igesio::extensions::machines {

namespace {

/// @brief ベクトルを正規化する
/// @param vector 対象のベクトル
/// @param what 例外メッセージに用いる対象名
/// @return 単位ベクトル
/// @throw std::invalid_argument ノルムが退化許容誤差未満の場合
igesio::Vector3d NormalizeOrThrow(const igesio::Vector3d& vector,
                                  const char* what) {
    const double norm = vector.norm();
    if (norm < kDegenerateTolerance) {
        throw std::invalid_argument(std::string(what) + " is a zero vector");
    }
    return vector / norm;
}

/// @brief 単位長を検査したうえで再正規化する
/// @param vector 対象のベクトル
/// @param name 例外メッセージに用いるキー名 (`"x_axis"`等)
/// @return 単位ベクトル
/// @throw std::invalid_argument ノルムが1から許容誤差を超えて外れる場合
igesio::Vector3d RequireUnit(const igesio::Vector3d& vector,
                             const char* name) {
    const double norm = vector.norm();
    if (std::abs(norm - 1.0) > kUnitVectorTolerance) {
        std::ostringstream message;
        message << name << ": not a unit vector (norm "
                << std::fixed << std::setprecision(6) << norm << ")";
        throw std::invalid_argument(message.str());
    }
    return vector / norm;
}

}  // namespace



igesio::Matrix3d RotationAboutAxis(const igesio::Vector3d& axis,
                                   const double angle_rad) {
    const igesio::Vector3d unit = NormalizeOrThrow(axis, "rotation axis");
    return Eigen::AngleAxisd(angle_rad, unit).toRotationMatrix();
}

igesio::Matrix3d RotationFromEulerIjk(const igesio::Vector3d& ijk_rad) {
    // 固定軸まわりにX→Y→Zの順で適用する (R = Rz(K) Ry(J) Rx(I))
    return RotationAboutAxis(igesio::Vector3d::UnitZ(), ijk_rad.z())
         * RotationAboutAxis(igesio::Vector3d::UnitY(), ijk_rad.y())
         * RotationAboutAxis(igesio::Vector3d::UnitX(), ijk_rad.x());
}

igesio::Matrix3d RotationFromColumns(const ColumnsSpec& columns) {
    igesio::Matrix3d rotation;
    rotation.col(0) = RequireUnit(columns.x_axis, "x_axis");
    rotation.col(1) = RequireUnit(columns.y_axis, "y_axis");
    rotation.col(2) = RequireUnit(columns.z_axis, "z_axis");

    const igesio::Matrix3d gram = rotation.transpose() * rotation;
    const double deviation =
            (gram - igesio::Matrix3d::Identity()).cwiseAbs().maxCoeff();
    if (deviation > kUnitVectorTolerance) {
        throw std::invalid_argument("not an orthonormal frame");
    }
    if (rotation.determinant() < 0.0) {
        throw std::invalid_argument("mirrored frame (negative determinant)");
    }
    return rotation;
}

igesio::Matrix3d ResolveRotation(const RotationSpec& spec) {
    return std::visit(
            [](const auto& form) -> igesio::Matrix3d {
                using Form = std::decay_t<decltype(form)>;
                if constexpr (std::is_same_v<Form, AxisAngleSpec>) {
                    return RotationAboutAxis(form.axis, form.angle_rad);
                } else if constexpr (std::is_same_v<Form, EulerIjkSpec>) {
                    return RotationFromEulerIjk(form.ijk_rad);
                } else {
                    return RotationFromColumns(form);
                }
            },
            spec);
}

igesio::Matrix4d MakeRigid(const igesio::Matrix3d& rotation,
                           const igesio::Vector3d& translation) {
    igesio::Matrix4d transform = igesio::Matrix4d::Identity();
    transform.block<3, 3>(0, 0) = rotation;
    transform.block<3, 1>(0, 3) = translation;
    return transform;
}

igesio::Matrix4d Translation(const igesio::Vector3d& translation) {
    return MakeRigid(igesio::Matrix3d::Identity(), translation);
}

igesio::Matrix4d RotationAboutLine(const igesio::Vector3d& direction,
                                   const igesio::Vector3d& point,
                                   const double angle_rad) {
    // 軸上の点pを不動点とする: x' = R (x - p) + p = R x + (p - R p)
    const igesio::Matrix3d rotation = RotationAboutAxis(direction, angle_rad);
    return MakeRigid(rotation, point - rotation * point);
}

igesio::Matrix4d RigidInverse(const igesio::Matrix4d& transform) {
    const igesio::Matrix3d rotation_t = RotationPart(transform).transpose();
    return MakeRigid(rotation_t, -rotation_t * TranslationPart(transform));
}

igesio::Matrix3d RotationPart(const igesio::Matrix4d& transform) {
    return transform.block<3, 3>(0, 0);
}

igesio::Vector3d TranslationPart(const igesio::Matrix4d& transform) {
    return transform.block<3, 1>(0, 3);
}

igesio::Vector3d ApplyPoint(const igesio::Matrix4d& transform,
                            const igesio::Vector3d& point) {
    return RotationPart(transform) * point + TranslationPart(transform);
}

igesio::Vector3d ApplyDirection(const igesio::Matrix4d& transform,
                                const igesio::Vector3d& direction) {
    return RotationPart(transform) * direction;
}

}  // namespace igesio::extensions::machines
