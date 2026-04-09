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

#include "igesio/numerics/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
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
           const ObjectID& iges_id)
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

std::optional<i_ent::CurveDerivatives>
Line::TryGetDerivatives(const double t, const unsigned int n) const {
    const auto range = GetParameterRange();
    // パラメータtが定義域内にあるかチェック
    if (t < range[0] || t > range[1]) {
        return std::nullopt;
    }

    CurveDerivatives result(n);
    result[0] = start_point_ + t * (terminate_point_ - start_point_);  // C(t)
    if (n >= 1) {
        result[1] = terminate_point_ - start_point_;  // C'(t)
    }
    // n >= 2についてはすべてゼロベクトルであり、Resizeで初期化済
    return result;
}

double Line::Length() const {
    switch (GetLineType()) {
        case LineType::kSegment:
            return (terminate_point_ - start_point_).norm();
        case LineType::kRay:
        case LineType::kLine:
            // 半直線および直線は無限長とみなす
            return std::numeric_limits<double>::infinity();
        default:
            return (terminate_point_ - start_point_).norm();
    }
}

double Line::Length(const double start, const double end) const {
    const auto range = GetParameterRange();
    // 引数の妥当性チェック
    if (start >= end) {
        throw std::invalid_argument("Start parameter must be less than end parameter.");
    }
    if (start < range[0] || end > range[1]) {
        throw std::invalid_argument("Parameters are out of range.");
    }
    if (std::abs(start) == std::numeric_limits<double>::infinity() ||
        std::abs(end) == std::numeric_limits<double>::infinity()) {
        // 一端が無限大の場合、長さは無限大
        return std::numeric_limits<double>::infinity();
    }

    // 線分の長さを計算
    return (terminate_point_ - start_point_).norm() * (end - start);
}

i_num::BoundingBox Line::GetDefinedBoundingBox() const {
    auto start = start_point_;
    auto end = terminate_point_;
    auto dir = terminate_point_ - start_point_;
    auto type = GetLineType();
    auto eps = 1e-10;  // 2軸方向の長さがゼロの場合に1軸に設定する微小値

    // 各軸方向の移動量を確認し、非ゼロかつkRay,kLineであれば無限大に設定
    std::array<double, 3> sizes = {
        std::abs(dir[0]), std::abs(dir[1]), std::abs(dir[2])};
    std::array<bool, 3> is_line = {false, false, false};
    for (size_t i = 0; i < 3; ++i) {
        if (i_num::IsApproxZero(sizes[i])) sizes[i] = 0.0;

        if (sizes[i] > 0.0 && type == LineType::kRay) {
            // 半直線
            sizes[i] = std::numeric_limits<double>::infinity();
        } else if (sizes[i] > 0.0 && type == LineType::kLine) {
            // 直線
            sizes[i] = std::numeric_limits<double>::infinity();
            is_line[i] = true;
        }
    }

    // 非ゼロの成分が1軸のみの場合
    if (sizes[0] == 0.0 && sizes[1] == 0.0) {
        // Z軸方向 -> D0をZ軸、D1をX軸、D2をY軸に変更
        std::array<Vector3d, 3> dirs =
                {Vector3d::UnitZ(), Vector3d::UnitX(), Vector3d::UnitY()};
        sizes = {sizes[2], eps, 0.0};
        is_line = {is_line[2], false, false};
        return i_num::BoundingBox(start, dirs, sizes, is_line);
    } else if ((sizes[1] == 0.0 && sizes[2] == 0.0) ||
               (sizes[2] == 0.0 && sizes[0] == 0.0)) {
        // XorY軸方向 -> YorX方向にepsを設定
        sizes = {std::max(sizes[0], eps), std::max(sizes[1], eps), 0.0};
    }

    auto min = start.cwiseMin(end);
    return i_num::BoundingBox(min, sizes, is_line);
}




/**
 * 描画用
 */

std::pair<const Vector3d&, const Vector3d&> Line::GetDefinedAnchorPoints() const {
    return {start_point_, terminate_point_};
}

std::pair<const Vector3d, const Vector3d> Line::GetAnchorPoints() const {
    return {Transform(start_point_, true).value(),
            Transform(terminate_point_, true).value()};
}
