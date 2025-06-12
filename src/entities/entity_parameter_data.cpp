/**
 * @file entities/entity_parameter_data.cpp
 * @brief IGESのパラメータデータセクションを保持する構造体
 * @author Yayoi Habami
 * @date 2025-04-23
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/entity_parameter_data.h"

#include <atomic>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "igesio/utils/iges_string_utils.h"

namespace {

/// @brief igesio 名前空間のエイリアス
namespace iio = igesio;
/// @brief igesio::utils 名前空間のエイリアス
namespace iu = igesio::utils;
/// @brief igesio::entities 名前空間のエイリアス
namespace i_ent = igesio::entities;

}  // namespace



i_ent::RawEntityPD::RawEntityPD()
    : RawEntityPD(EntityType::kNull, 0, 0, std::vector<std::string>()) {}

i_ent::RawEntityPD::RawEntityPD(
        const EntityType type, const std::vector<std::string>& data)
    : RawEntityPD(type, 0, 0, data) {}

i_ent::RawEntityPD::RawEntityPD(
        const EntityType type, const unsigned int de_pointer,
        const unsigned int sequence_number,
        const std::vector<std::string>& data,
        const std::vector<IGESParameterType>& data_types)
    : type(type),
      de_pointer(de_pointer),
      sequence_number(sequence_number),
      data(data), data_types(data_types) {}

i_ent::RawEntityPD::RawEntityPD(
        const RawEntityPD& other)
    : type(other.type),
      de_pointer(other.de_pointer),
      sequence_number(other.sequence_number),
      data(other.data) {}

i_ent::RawEntityPD::RawEntityPD(
        RawEntityPD&& other) noexcept
    : type(other.type),
      de_pointer(other.de_pointer),
      sequence_number(other.sequence_number),
      data(std::move(other.data)) {}

i_ent::RawEntityPD& i_ent::RawEntityPD::operator=(
        const RawEntityPD& other) {
    if (this != &other) {
        type = other.type;
        de_pointer = other.de_pointer;
        sequence_number = other.sequence_number;
        data = other.data;
    }
    return *this;
}

i_ent::RawEntityPD& i_ent::RawEntityPD::operator=(
        RawEntityPD&& other) noexcept {
    if (this != &other) {
        type = other.type;
        de_pointer = other.de_pointer;
        sequence_number = other.sequence_number;
        data = std::move(other.data);
    }
    return *this;
}



i_ent::RawEntityPD i_ent::ToRawEntityPD(
        const std::vector<std::string>& lines,
        const char p_delim, const char r_delim) {
    i_ent::RawEntityPD ep;

    // シーケンス番号を取得
    auto sequence_number = iu::GetSequenceNumber(lines[0]);
    // DEポインタを取得
    auto de_pointer = iu::GetDEPointer(lines[0]);

    // 各行のデータ部のみを取得
    std::vector<std::string> data_lines {};
    for (const auto& line : lines) {
        // データ部のみを取得し、追加
        std::string data = iu::GetDataPart(line, SectionType::kParameter);
        data_lines.push_back(data);
    }
    // データ部をパラメータ区切り文字で分割
    std::vector<std::string> data = iu::ParseFreeFormattedData(
            data_lines, p_delim, r_delim);

    // エンティティタイプを取得
    auto type = i_ent::ToEntityType(std::stoi(data[0]));
    if (!type.has_value()) {
        throw iio::TypeConversionError(
                "Invalid entity type: " + std::to_string(std::stoi(data[0])) +
                " on line " + std::to_string(ep.sequence_number) +
                " of the Parameter Data section");
    }

    return i_ent::RawEntityPD(
        type.value(), de_pointer, sequence_number,
        // 1つ目の要素はエンティティタイプなので除外
        std::vector<std::string>(data.begin() + 1, data.end()));
}



/**
 * エンティティのパラメータデータの解析
 */

namespace {

/// @brief エンティティのパラメータをカウントする関数の戻り値の型
using t_out = std::tuple<std::size_t, std::size_t, std::size_t>;

/// @brief PD部の追加パラメータの個数をカウントする
/// @param data エンティティタイプを除いた、パラメータ区切り文字で分割したパラメータ
/// @param count エンティティで指定されるパラメータの数
/// @return エンティティで指定されるパラメータに続く、以下のパラメータの個数を返す
///   1. 関連付け/テキストエンティティへのポインタ (冒頭の個数指示部 (NA) 含む)
///   2. プロパティまたは属性テーブルエンティティへのポインタ (同様に (NV) 含む)
std::pair<std::size_t, std::size_t>
CountAdditionalParameters(const std::vector<std::string>& data, const std::size_t count) {
    if (data.size() < count) {
        // countよりもサイズが小さい場合はエラー
        throw iio::SectionFormatError(
            "The number of parameters is less than the count: " +
                std::to_string(count) + " < " + std::to_string(data.size()));
    } else if (data.size() == count) {
        // countとサイズが同じ場合は、追加パラメータはない
        return {0, 0};
    }

    // 関連付け/テキストエンティティへのポインタの個数を取得
    int num_a = iio::FromIgesInteger(data[count]);
    if (num_a < 0 || data.size() < count + num_a + 1) {
        // dataのサイズが小さい場合はエラー
        throw iio::SectionFormatError(
            "The size of data is too small, even if NA (=" + std::to_string(num_a) +
            ") is specified: " + std::to_string(data.size()) + " < " +
            std::to_string(count + num_a + 1));
    }

    // プロパティまたは属性テーブルエンティティへのポインタの個数を取得
    int num_v = iio::FromIgesInteger(data[count + num_a + 1]);
    if (num_v < 0 || data.size() < count + num_a + num_v + 2) {
        // dataのサイズが小さい場合はエラー
        throw iio::SectionFormatError(
            "The size of data is too small, even if NV (=" + std::to_string(num_v) +
            ") is specified: " + std::to_string(data.size()) + " < " +
            std::to_string(count + num_a + num_v + 2));
    }

    return {num_a, num_v};
}

/// @brief Null Entity (Type 0) のパラメタカウンタ
t_out E000ParamCount(const std::vector<std::string>& data) {
    // Type 0のPD部は無視される
    auto [na, nv] = CountAdditionalParameters(data, 0);
    return {0, na, nv};
}
/// @brief Null Entity (Type 0) の物理的従属子要素のポインタ
std::vector<unsigned int> E000Children(const i_ent::RawEntityPD& data) {
    // Type 0のPD部は無視される
    return {};
}




/// @brief 物理的に従属する子要素のDEポインタを取得する
/// @param data エンティティデータ
/// @return 物理的に従属する子要素のDEポインタ
std::vector<unsigned int> GetPhysicallyDependentChildDEPointer(
        const i_ent::RawEntityPD& data) {
    switch (data.type) {
        case i_ent::EntityType::kNull: return E000Children(data);
        // TODO: 他のエンティティタイプの処理を追加
        default:
            break;
    }
    // 上記以外は未実装
    throw iio::NotImplementedError(
            "The procedure to get pointers of "
            "physically dependent children for the entity type " +
            std::to_string(static_cast<int>(data.type)) + " is not implemented.");
}

}  // namespace

std::tuple<std::size_t, std::size_t, std::size_t>
i_ent::GetEntityParameterCount(
        const EntityType type, const std::vector<std::string>& data) {
    switch (type) {
        case EntityType::kNull: return E000ParamCount(data);
    }
    // 上記以外は未実装
    throw iio::NotImplementedError(
            "The procedure to count parameters for the entity type " +
            std::to_string(static_cast<int>(type)) + " is not implemented.");
}

std::vector<unsigned int> i_ent::GetChildDEPointer(
        const RawEntityPD& data,
        const i_ent::SubordinateEntitySwitch dependency) {
    if (dependency == SubordinateEntitySwitch::kIndependent) {
        // 独立した子要素はないため、空のベクタを返す
        return {};
    }

    // 物理的に従属する子要素のDEポインタを取得
    std::vector<unsigned int> physical = {};
    if (dependency == i_ent::SubordinateEntitySwitch::kPhysicallyDependent ||
        dependency == i_ent::SubordinateEntitySwitch::kPhysicallyAndLogicallyDependent) {
        // 物理的に従属する子要素のDEポインタを取得
        physical = GetPhysicallyDependentChildDEPointer(data);
    }

    // 論理的に従属する子要素のDEポインタを取得
    std::vector<unsigned int> logical = {};
    if (dependency == i_ent::SubordinateEntitySwitch::kLogicallyDependent ||
        dependency == i_ent::SubordinateEntitySwitch::kPhysicallyAndLogicallyDependent) {
        // 論理的に従属する子要素のDEポインタを取得
        throw iio::NotImplementedError(
                "The procedure to get pointers of "
                "logically dependent child entities is not implemented.");
    }

    // 結合して返す
    if (logical.empty()) {
        return physical;
    } else if (physical.empty()) {
        return logical;
    } else {
        physical.insert(physical.end(), logical.begin(), logical.end());
        return physical;
    }
}
