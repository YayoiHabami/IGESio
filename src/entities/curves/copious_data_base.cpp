/**
 * @file entities/curves/copious_data_base.cpp
 * @brief Copious Data (Type 106) の基底クラス
 * @author Yayoi Habami
 * @date 2025-08-19
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/copious_data_base.h"

#include <limits>
#include <utility>
#include <vector>

#include "igesio/common/tolerance.h"

namespace {

namespace i_ent = igesio::entities;
using i_ent::CopiousDataBase;

/// @brief CopiousDataBaseの仮のPDレコードを作成する
/// @param type CopiousDataType
/// @return 2組の座標値のみを持つIGESParameterVector
igesio::IGESParameterVector
GetTemporaryPDParameters(const i_ent::CopiousDataType type) {
    int ip = i_ent::GetIP(type);
    // 仮に2組の座標値を設定 (IP=1の場合は4つ、IP=2の場合は6つ、IP=3の場合は12つ)
    int count = 2;
    int num_coords = (ip == 3) ? 12 : 2 * (ip + 1);

    igesio::IGESParameterVector params;
    params.reserve((ip == 1) ? 3 + num_coords : 2 + num_coords);
    params.push_back(ip);     // IP
    params.push_back(count);  // 座標値の数
    if (ip == 1) params.push_back(0.0);  // Z座標値 (IP=1の場合のみ)

    for (int i = 0; i < num_coords; ++i) {
        params.push_back(0.0);  // 座標値
    }

    return params;
}

}  // namespace



int i_ent::GetIP(const CopiousDataType type) {
    int ip = static_cast<int>(type) % 10;
    if (type >= CopiousDataType::kCenterlineByPoints) {
        return 1;
    }
    if (ip < 1 || ip > 3) {
        throw igesio::DataFormatError(
            "Invalid CopiousDataType: " + std::to_string(static_cast<int>(type)));
    }
    return ip;
}



/**
 * コンストラクタ
 */

CopiousDataBase::CopiousDataBase(const RawEntityDE& de_record,
                                 const IGESParameterVector& parameters,
                                 const pointer2ID& de2id,
                                 const uint64_t iges_id)
    : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);

    if (auto result = ValidatePD(); !result.is_valid) {
        throw igesio::DataFormatError(
            "Invalid parameters for CopiousDataBase: " + result.Message());
    }
}

CopiousDataBase::CopiousDataBase(const CopiousDataType type,
                                 const Matrix3Xd& coordinates,
                                 const Matrix3Xd& addition)
    : EntityBase(RawEntityDE::ByDefault(EntityType::kCopiousData,
                                        static_cast<int>(type)),
                 GetTemporaryPDParameters(type), {}) {
    InitializePD({});

    // 座標値を設定
    coordinates_ = coordinates;
    addition_ = addition;

    if (auto result = ValidatePD(); !result.is_valid) {
        throw igesio::DataFormatError(
            "Invalid parameters for CopiousDataBase: " + result.Message());
    }
}

igesio::ValidationResult
CopiousDataBase::ValidatePD() const {
    std::vector<ValidationError> errors;

    // typeの妥当性を確認
    int ip;
    try {
        ip = i_ent::GetIP(GetDataType());
    } catch (const igesio::DataFormatError& e) {
        errors.emplace_back(e.what());
    }

    // 2組以上の座標値の組が必要
    if (coordinates_.cols() < 2) {
        errors.emplace_back(
            "At least two tuples are required for CopiousDataBase.");
    }

    // IP=1,2の場合、additionは空であること
    if ((ip == 1 || ip == 2) && addition_.cols() > 1) {
        errors.emplace_back(
            "Addition must be empty for CopiousDataType with IP=1 or 2.");
    }

    if (ip == 1) {
        // IP=1の場合: すべてのZ座標値が同じであること
        auto z_value = coordinates_(2, 0);
        for (size_t i = 1; i < coordinates_.cols(); ++i) {
            if (!IsApproxEqual(coordinates_(2, i), z_value, kGeometryTolerance)) {
                errors.emplace_back(
                    "All Z coordinates must be the same for CopiousDataType with IP=1."
                    "index " + std::to_string(i) + " has Z=" +
                    std::to_string(coordinates_(2, i)) + ", expected " +
                    std::to_string(z_value) + ".");
                break;
            }
        }
    } else if (ip == 2) {
        // IP=2の場合: 追加のチェック事項はない
    } else if (ip == 3) {
        // IP=3の場合: coordinatesとadditionの列数が同じであること
        if (coordinates_.cols() != addition_.cols()) {
            errors.emplace_back(
                "Coordinates and addition must have the same number of columns for "
                "CopiousDataType with IP=3.");
        }
    }

    return MakeValidationResult(std::move(errors));
}

i_ent::CopiousDataType CopiousDataBase::GetDataType() const {
    return static_cast<CopiousDataType>(form_number_);
}

int CopiousDataBase::GetIP() const {
    return igesio::entities::GetIP(GetDataType());
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector
CopiousDataBase::GetMainPDParameters() const {
    // データをIGESParameterVectorに変換
    IGESParameterVector params;
    size_t vec_size;
    if (GetIP() == 1) vec_size = 2 + 2 * GetCount();
    if (GetIP() == 2) vec_size = 1 + 3 * GetCount();
    if (GetIP() == 3) vec_size = 1 + 6 * GetCount();
    params.reserve(vec_size);

    // IP
    params.push_back(GetIP());
    // 座標値の数
    params.push_back(static_cast<int>(GetCount()));
    // Z座標値 (IP=1の場合)
    if (GetIP() == 1) params.push_back(coordinates_(2, 0));

    // 座標値
    for (size_t i = 0; i < GetCount(); ++i) {
        params.push_back(coordinates_(0, i));
        params.push_back(coordinates_(1, i));

        if (GetIP() >= 2) {
            params.push_back(coordinates_(2, i));
        }
        if (GetIP() == 3) {
            params.push_back(addition_(0, i));
            params.push_back(addition_(1, i));
            params.push_back(addition_(2, i));
        }
    }

    // pd_parameters_のフォーマットを適用
    // 要素数が同じ場合のみ適用する
    if (params.size() == pd_parameters_.size()) {
        for (size_t i = 0; i < pd_parameters_.size(); ++i) {
            try {
                params.set_format(i, pd_parameters_.get_format(i));
            } catch (const igesio::DataFormatError&) {
                // 変換元のフォーマットが正しくない場合は更新しない
            }
        }
    }
    return params;
}

size_t CopiousDataBase::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;
    auto ip = pd.access_as<int>(0);

    auto count = pd.access_as<int>(1);
    if (count < 2) {
        // 最低でも2組以上の座標値の組が必要
        throw igesio::DataFormatError(
            "Invalid number of parameters N for IP=" + std::to_string(ip) +
            ". Copious Data requires at least two tuples.");
    }
    auto required_size = 2 * count + 3;
    if (ip == 2) required_size = 3 * count + 2;
    if (ip == 3) required_size = 6 * count + 2;
    if (pd.size() < required_size) {
        // IP=1の場合は座標値のペア、IP=2の場合は座標値の3つ組、
        // IP=3の場合は座標値の6つ組が必要
        throw igesio::DataFormatError(
            "Invalid number of parameters for Copious Data with IP=" +
            std::to_string(ip) + ". Expected at least " +
            std::to_string(required_size) + " parameters.");
    }

    // 座標値を設定
    auto index = 1;
    auto z = (ip != 1) ? 0.0 : pd.access_as<double>(++index);
    coordinates_.resize(3, count);
    if (ip == 3) addition_.resize(3, count);
    for (size_t i = 0; i < count; ++i) {
        coordinates_(0, i) = pd.access_as<double>(++index);
        coordinates_(1, i) = pd.access_as<double>(++index);
        if (ip == 1) {
            coordinates_(2, i) = z;
        } else if (ip == 2) {
            coordinates_(2, i) = pd.access_as<double>(++index);
        } else if (ip == 3) {
            coordinates_(2, i) = pd.access_as<double>(++index);
            addition_(0, i) = pd.access_as<double>(++index);
            addition_(1, i) = pd.access_as<double>(++index);
            addition_(2, i) = pd.access_as<double>(++index);
        }
    }

    return index + 1;
}



/**
 * 描画用
 */

igesio::Vector3d
CopiousDataBase::GetCoordinate(const size_t i) const {
    if (i >= coordinates_.cols()) {
        throw std::out_of_range("Index out of range in CopiousDataBase.");
    }
    return coordinates_.col(i);
}

igesio::Vector3d
CopiousDataBase::GetAddition(const size_t i) const {
    if (i >= addition_.cols()) {
        throw std::out_of_range("Index out of range in CopiousDataBase.");
    }
    return addition_.col(i);
}

size_t CopiousDataBase::GetCount() const {
    return coordinates_.cols();
}

double CopiousDataBase::TotalLength() const {
    double total_length = 0.0;
    for (size_t i = 1; i < coordinates_.cols(); ++i) {
        total_length += (coordinates_.col(i) - coordinates_.col(i - 1)).norm();
    }
    return total_length;
}

std::optional<igesio::Vector3d>
CopiousDataBase::GetCoordinateAtLength(const double length) const {
    if (length < 0.0 || length > TotalLength()) {
        return std::nullopt;
    }

    double accumulated_length = 0.0;
    for (size_t i = 1; i < coordinates_.cols(); ++i) {
        // 各セグメントごとに長さを計算し、指定された長さに達するか確認
        double segment_length = (coordinates_.col(i) - coordinates_.col(i - 1)).norm();
        if (accumulated_length + segment_length >= length) {
            // 指定された長さがこのセグメント内にある場合、線形補間で座標を計算
            double t = (length - accumulated_length) / segment_length;
            return coordinates_.col(i - 1) * (1 - t) + coordinates_.col(i) * t;
        }
        accumulated_length += segment_length;
    }

    return std::nullopt;
}

std::pair<size_t, double>
CopiousDataBase::GetNearestVertexAt(const double length) const {
    // 始点より前の場合は、距離をinfとする
    if (length < 0.0) {
        return {0, std::numeric_limits<double>::infinity()};
    }

    double accumulated_length = 0.0;
    for (size_t i = 1; i < coordinates_.cols(); ++i) {
        double segment_length = (coordinates_.col(i) - coordinates_.col(i - 1)).norm();
        if (accumulated_length + segment_length >= length) {
            // 指定された長さがこのセグメント内にある場合、どちらの頂点が近いかを確認
            double dist_to_prev = length - accumulated_length;
            double dist_to_next = (accumulated_length + segment_length) - length;
            if (dist_to_prev <= dist_to_next) {
                return {i - 1, dist_to_prev};
            } else {
                return {i, dist_to_next};
            }
        }
        accumulated_length += segment_length;
    }

    // 終点より後の場合
    return {coordinates_.cols() - 1, std::numeric_limits<double>::infinity()};
}
