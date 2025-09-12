/**
 * @file entities/curves/line.cpp
 * @brief Line (Type 110): 線分/半直線/直線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-10
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/line.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "igesio/common/tolerance.h"

namespace {

namespace i_ent = igesio::entities;
using Line = i_ent::Line;
using Vector3d = igesio::Vector3d;

}  // namespace



/**
 * コンストラクタ
 */

Line::Line(const RawEntityDE& de_record,
           const IGESParameterVector& parameters,
           const pointer2ID& de2id,
           const uint64_t iges_id)
    : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

Line::Line(const Vector3d& start_point,
           const Vector3d& terminate_point,
           const LineType line_type)
    : Line(RawEntityDE::ByDefault(EntityType::kLine,
                                  static_cast<int>(line_type)),
           IGESParameterVector{
               start_point[0], start_point[1], start_point[2],
               terminate_point[0], terminate_point[1], terminate_point[2]}) {
    if (start_point[0] == terminate_point[0] &&
        start_point[1] == terminate_point[1] &&
        start_point[2] == terminate_point[2]) {
        throw igesio::DataFormatError("Start point and terminate point cannot be the same.");
    }
}

i_ent::LineType Line::GetLineType() const {
    return static_cast<LineType>(form_number_);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector Line::GetMainPDParameters() const {
    IGESParameterVector params{
        start_point_[0], start_point_[1], start_point_[2],
        terminate_point_[0], terminate_point_[1], terminate_point_[2]};

    // pd_parameters_のフォーマットを適用
    // CircularArcの場合はPD部の要素数は常に同じためそのまま適用
    for (size_t i = 0; i < std::min(params.size(), pd_parameters_.size()); ++i) {
        try {
            params.set_format(i, pd_parameters_.get_format(i));
        } catch (const std::invalid_argument&) {
            // 変換元のフォーマットが正しくない場合は更新しない
        }
    }
    return params;
}

size_t Line::SetMainPDParameters(const pointer2ID& de2id) {
    // パラメータの数が6以上であることを確認
    // Lineの6つのパラメータ + 追加のポインタ
    auto& pd = pd_parameters_;
    if (pd.size() < 6) {
        throw igesio::DataFormatError(
            "Line entity requires at least 6 parameters in PD section.");
    }

    // パラメータを設定
    start_point_ = Vector3d(pd.access_as<double>(0),
                            pd.access_as<double>(1),
                            pd.access_as<double>(2));
    terminate_point_ = Vector3d(pd.access_as<double>(3),
                                pd.access_as<double>(4),
                                pd.access_as<double>(5));

    return 6;
}

igesio::ValidationResult Line::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 始点と終点が同じでないことを確認
    if (start_point_[0] == terminate_point_[0] &&
        start_point_[1] == terminate_point_[1] &&
        start_point_[2] == terminate_point_[2]) {
        errors.emplace_back("Start point and terminate point cannot be the same.");
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * ICurve implementation
 */

std::array<double, 2> Line::GetParameterRange() const {
    auto line_type = static_cast<LineType>(form_number_);
    switch (line_type) {
        case LineType::kSegment:
            return {0.0, 1.0};
        case LineType::kRay:
            return {0.0, std::numeric_limits<double>::infinity()};
        case LineType::kLine:
            return {-std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity()};
        default:
            // 不明なFormの場合は、最も基本的なForm 0として扱う
            return {0.0, 1.0};
    }
}

bool Line::IsClosed() const { return false; }

std::optional<Vector3d> Line::TryGetDefinedPointAt(const double t) const {
    const auto range = GetParameterRange();
    // パラメータtが定義域内にあるかチェック
    if (t < range[0] || t > range[1]) {
        return std::nullopt;
    }

    // C(t) = P1 + t * (P2 - P1)
    return start_point_ + t * (terminate_point_ - start_point_);
}

std::optional<Vector3d> Line::TryGetDefinedTangentAt(const double t) const {
    const auto range = GetParameterRange();
    // パラメータtが定義域内にあるかチェック
    if (t < range[0] || t > range[1]) {
        return std::nullopt;
    }

    const Vector3d tangent_vec = terminate_point_ - start_point_;

    // 縮退している場合は接線を定義できない
    if (tangent_vec.norm() < kGeometryTolerance) {
        return std::nullopt;
    }

    // 接線ベクトルはパラメータtによらず一定。単位ベクトルとして返す。
    return tangent_vec.normalized();
}

std::optional<Vector3d> Line::TryGetDefinedNormalAt(const double t) const {
    const auto range = GetParameterRange();
    // パラメータtが定義域内にあるかチェック
    if (t < range[0] || t > range[1]) {
        return std::nullopt;
    }

    /// 直線においては、本来法線ベクトルは一意に定義されないが、
    /// ここでは接線を90度回転させた、定義空間においてz=0のベクトルを法線として返す
    Vector3d tangent_vec = terminate_point_ - start_point_;
    return Vector3d(-tangent_vec.y(), tangent_vec.x(), 0.0).normalized();
}

std::optional<Vector3d> Line::TryGetPointAt(const double t) const {
    return TransformPoint(TryGetDefinedPointAt(t));
}

std::optional<Vector3d> Line::TryGetTangentAt(const double t) const {
    return TransformPoint(TryGetDefinedTangentAt(t));
}

std::optional<Vector3d> Line::TryGetNormalAt(const double t) const {
    return TransformPoint(TryGetDefinedNormalAt(t));
}



/**
 * 描画用
 */

std::pair<const Vector3d&, const Vector3d&> Line::GetAnchorPoints() const {
    return {start_point_, terminate_point_};
}
