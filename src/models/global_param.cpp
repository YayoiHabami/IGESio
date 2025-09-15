/**
 * @file models/global_param.cpp
 * @brief IGESファイルのグローバルセクションのデータを保持する構造体
 * @author Yayoi Habami
 * @date 2025-04-21
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/models/global_param.h"

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "igesio/common/versions.h"
#include "igesio/utils/iges_string_utils.h"

namespace {

/// @brief igesio 名前空間のエイリアス
namespace iio = igesio;
/// @brief igesio::models 名前空間のエイリアス
namespace i_model = igesio::models;
/// @brief igesio::utils 名前空間のエイリアス
namespace iu = igesio::utils;

}  // namespace



iio::SerializationConfig
i_model::GlobalParam::GetSerializationConfig() const {
    return {
        integer_bits,
        single_precision_power_max,
        single_precision_digits,
        double_precision_power_max,
        double_precision_digits
    };
}



/**
 * Enumクラス用変換
 */

i_model::UnitFlag i_model::ToUnitFlagEnum(const int unit_flag) {
    if (unit_flag < 1 || unit_flag > 11) {
        throw iio::TypeConversionError("Invalid unit flag value: " +
                                       std::to_string(unit_flag));
    }
    return static_cast<i_model::UnitFlag>(unit_flag);
}

i_model::UnitFlag i_model::ToUnitFlagEnum(const std::string& unit_name) {
    if (unit_name == "2HIN" || unit_name == "4HINCH") {
        return i_model::UnitFlag::kInch;
    } else if (unit_name == "2HMM") {
        return i_model::UnitFlag::kMillimeter;
    } else if (unit_name == "3HMIL") {
        return i_model::UnitFlag::kMil;
    } else if (unit_name == "2HFT") {
        return i_model::UnitFlag::kFeet;
    } else if (unit_name == "2HMI") {
        return i_model::UnitFlag::kMile;
    } else if (unit_name == "1HM") {
        return i_model::UnitFlag::kMeter;
    } else if (unit_name == "2HKM") {
        return i_model::UnitFlag::kKilometer;
    } else if (unit_name == "3HUIN") {
        return i_model::UnitFlag::kMicroInch;
    } else if (unit_name == "2HUM") {
        return i_model::UnitFlag::kMicron;
    } else if (unit_name == "2HCM") {
        return i_model::UnitFlag::kCentimeter;
    } else {
        // 単位フラグが3の場合も含め、無効な場合は例外を投げる
        throw iio::TypeConversionError("Invalid unit name: " + unit_name);
    }
}

std::string i_model::ToUnitName(const i_model::UnitFlag unit_flag) {
    switch (unit_flag) {
        case i_model::UnitFlag::kInch: return "INCH";
        case i_model::UnitFlag::kMillimeter: return "MM";
        case i_model::UnitFlag::kFeet: return "FT";
        case i_model::UnitFlag::kMile: return "MI";
        case i_model::UnitFlag::kMeter: return "M";
        case i_model::UnitFlag::kKilometer: return "KM";
        case i_model::UnitFlag::kMil: return "MIL";
        case i_model::UnitFlag::kMicron: return "UM";
        case i_model::UnitFlag::kCentimeter: return "CM";
        case i_model::UnitFlag::kMicroInch: return "UIN";
        default:
            // 単位フラグが3の場合も含め、無効な場合は例外を投げる
            throw iio::TypeConversionError("Invalid unit flag value: " +
                                           std::to_string(static_cast<int>(unit_flag)));
    }
}

i_model::VersionFlag i_model::ToVersionFlagEnum(const int version_flag) noexcept {
    if (version_flag < 3) {
        return i_model::VersionFlag::kVersion2_0;
    } else if (version_flag > 11) {
        return i_model::VersionFlag::kVersion5_0;
    } else {
        return static_cast<i_model::VersionFlag>(version_flag);
    }
}

i_model::DraftingStandardFlag
i_model::ToDraftingStandardFlagEnum(const int drafting_standard_flag) {
    if (drafting_standard_flag < 0 || drafting_standard_flag > 7) {
        throw iio::TypeConversionError("Invalid drafting standard flag value: " +
                                       std::to_string(drafting_standard_flag));
    }
    return static_cast<i_model::DraftingStandardFlag>(drafting_standard_flag);
}



/**
 * GlobalParam用変換
 */

namespace {

/// @brief 数字のビット表現に関する設定値を取得
/// @param int_bits 整数のビット表現
/// @param float_p_max 単精度浮動小数点数の最大10の累乗
/// @param float_p_digits 単精度浮動小数点数の有効桁数
/// @param double_p_max 倍精度浮動小数点数の最大10の累乗
/// @param double_p_digits 倍精度浮動小数点数の有効桁数
/// @return 整数のビット表現に関する設定値 (上と同じ順序)
/// @throw igesio::TypeConversionError 型変換エラーが発生した場合
/// @note IGES5.3では規定されていないが、C++側の制約があるため、
///       デフォルト値を設定している
std::array<int, 5> GetNumericBitSettings(
        const std::string& int_bits,
        const std::string& float_p_max,
        const std::string& float_p_digits,
        const std::string& double_p_max,
        const std::string& double_p_digits) {
    // 整数
    auto int_bits_v = iio::FromIgesInteger(
            int_bits, iio::kDefaultIntegerBits);
    if (int_bits_v > iio::kDefaultIntegerBits) {
        int_bits_v = iio::kDefaultIntegerBits;
    }

    // 単精度浮動小数点数
    auto float_p_max_v = iio::FromIgesInteger(
            float_p_max, iio::kDefaultSinglePrecisionPowerMax);
    if (float_p_max_v > iio::kDefaultSinglePrecisionPowerMax) {
        float_p_max_v = iio::kDefaultSinglePrecisionPowerMax;
    }
    auto float_p_digits_v = iio::FromIgesInteger(
            float_p_digits, iio::kDefaultSinglePrecisionDigits);
    if (float_p_digits_v > iio::kDefaultSinglePrecisionDigits) {
        float_p_digits_v = iio::kDefaultSinglePrecisionDigits;
    }

    // 倍精度浮動小数点数
    auto double_p_max_v = iio::FromIgesInteger(
            double_p_max, iio::kDefaultDoublePrecisionPowerMax);
    if (double_p_max_v > iio::kDefaultDoublePrecisionPowerMax) {
        double_p_max_v = iio::kDefaultDoublePrecisionPowerMax;
    }
    auto double_p_digits_v = iio::FromIgesInteger(
            double_p_digits, iio::kDefaultDoublePrecisionDigits);
    if (double_p_digits_v > iio::kDefaultDoublePrecisionDigits) {
        double_p_digits_v = iio::kDefaultDoublePrecisionDigits;
    }

    return {int_bits_v, float_p_max_v, float_p_digits_v,
            double_p_max_v, double_p_digits_v};
}

/// @brief 単位フラグを取得する
/// @param unit_flag 単位フラグの数値
/// @param unit_name 単位名 (H文字列)
/// @return UnitFlag
/// @throw igesio::TypeConversionError 単位フラグの数値が無効な場合
/// @throw igesio::NotImplementedError 単位フラグが3かつ単位名が
///        IGES5.3で定義されている10種類以外の単位の場合
/// @note 単位フラグが3以外の場合、単に単位フラグに対応するUnitFlagを返す.
///       単位フラグが3の場合、単位名がIGES5.3で定義されている10種類の単位のいずれか
///       であることを確認し、それに対応するUnitFlagを返す.
i_model::UnitFlag GetUnitFlag(
        const std::string& unit_flag, const std::string& unit_name) {
    // 単位フラグのEnumを取得
    auto unit_flag_v = iio::FromIgesInteger(unit_flag, 1);
    if (unit_flag_v < 1 || unit_flag_v > 11) {
        throw iio::TypeConversionError("Invalid unit flag value: " + unit_flag);
    }
    auto unit_flag_enum = i_model::ToUnitFlagEnum(unit_flag_v);

    // 単位フラグが3以外の場合、フラグの値を返す
    if (unit_flag_enum != i_model::UnitFlag::kUnitName) return unit_flag_enum;

    // 単位フラグが3の場合、単位名を取得
    try {
        return i_model::ToUnitFlagEnum(unit_name);
    } catch (const iio::TypeConversionError& e) {
        // 単位名がIGES5.3で定義されている10種類の単位のいずれかでない場合、
        // 例外を投げる
        throw iio::NotImplementedError(
            "Conversion for unit flag 3 with unit name is not implemented: "
            + unit_name + ")");
    }
}

}  // namespace

i_model::GlobalParam i_model::SetGlobalSectionParams(
    const char p_delim, const char r_delim,
    const std::vector<std::string>& prm) {
    GlobalParam gs;

    // 区切り文字を設定（パラメータ1~2）
    gs.param_delim = p_delim;
    gs.record_delim = r_delim;

    // パラメータが18+2個 (最大座標値まで) は要求
    // まれに後の方のパラメータを指定していないファイルが存在するため
    if (prm.size() < 18) {
        throw iio::SectionFormatError(
            "Invalid number of global section parameters: " +
            std::to_string(prm.size()));
    }

    // 残りのパラメータを設定
    std::size_t i = 0;
    try {
        gs.product_id = iio::FromIgesString(prm[i++], std::nullopt);
        gs.file_name = iio::FromIgesString(prm[i++], std::nullopt);
        gs.native_system_id = iio::FromIgesString(prm[i++], std::nullopt);
        gs.preprocessor_version = iio::FromIgesString(prm[i++], std::nullopt);

        // 数値のビット表現
        auto bits = GetNumericBitSettings(
            prm[i], prm[i+1], prm[i+2], prm[i+3], prm[i+4]);
        i += 5;
        gs.integer_bits = bits[0];
        gs.single_precision_power_max = bits[1];
        gs.single_precision_digits = bits[2];
        gs.double_precision_power_max = bits[3];
        gs.double_precision_digits = bits[4];

        gs.receiving_system_id = iio::FromIgesString(prm[i++], gs.product_id);
        gs.model_space_scale = iio::FromIgesReal(prm[i++], kDefaultModelSpaceScale);
        gs.units_flag = GetUnitFlag(prm[i], prm[i+1]);
        i += 2;
        gs.line_weight_gradations = iio::FromIgesInteger(prm[i++], kDefaultLineWeightGradations);
        gs.max_line_weight = iio::FromIgesReal(prm[i++], std::nullopt);
        gs.date_time_generation = iio::FromIgesString(prm[i++], std::nullopt);
        gs.min_resolution = iio::FromIgesReal(prm[i++], std::nullopt);
        gs.max_coordinate = iio::FromIgesReal(prm[i++], kDefaultMaxCoordinate);

        // 以降は任意パラメータ
        if (i < prm.size())
            gs.author_name = iio::FromIgesString(prm[i++], std::string(kDefaultAuthor));
        if (i < prm.size())
            gs.author_organization = iio::FromIgesString(prm[i++],
                                                  std::string(kDefaultAuthorOrg));
        if (i < prm.size())
            gs.specification_version = ToVersionFlagEnum(iio::FromIgesInteger(prm[i++]));
        if (i < prm.size())
            gs.drafting_standard_flag = ToDraftingStandardFlagEnum(iio::FromIgesInteger(prm[i++]));
        if (i < prm.size())
            gs.date_time_modified = iio::FromIgesString(prm[i++],
                                                 std::string(kDefaultDateTimeModified));
        if (i < prm.size())
            gs.protocol_identifier = iio::FromIgesString(prm[i++],
                                                  std::string(kDefaultProtocolIdentifier));
    } catch (const iio::TypeConversionError& e) {
        // 型変換エラーが発生した場合は、パラメータ番号を付して再度投げる
        throw iio::TypeConversionError(
            std::string(e.what()) +
            " (in conversion of global parameter " + std::to_string(i+2) + ")");
    }

    return gs;
}




namespace {

/// @brief 現在の日時を文字列形式で取得
/// @return YYYYMMDD.HHMMSS形式の文字列
std::string GetCurrentTimeString() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    tm = *std::localtime(&time_t);
#endif

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900)
        << std::setw(2) << (tm.tm_mon + 1)
        << std::setw(2) << tm.tm_mday
        << "."
        << std::setw(2) << tm.tm_hour
        << std::setw(2) << tm.tm_min
        << std::setw(2) << tm.tm_sec;

    return oss.str();
}

}  // namespace

igesio::IGESParameterVector
i_model::ToVector(const GlobalParam& gs, const std::string& file_name) {
    igesio::IGESParameterVector prm;
    prm.reserve(26);
    using VF = igesio::ValueFormat;

    // Parameter 1: Parameter delimiter character
    prm.push_back(std::string(1, gs.param_delim), VF::String());
    // Parameter 2: Record delimiter character
    prm.push_back(std::string(1, gs.record_delim), VF::String());
    // Parameter 3: Product identification from sender
    auto product_id = gs.product_id;
    if (file_name.empty()) {
        prm.push_back(gs.product_id, VF::String());
    } else {
        prm.push_back(file_name, VF::String());
        product_id = file_name;
    }
    // Parameter 4: File name
    if (file_name.empty()) {
        prm.push_back(gs.file_name, VF::String());
    } else {
        prm.push_back(file_name, VF::String());
    }
    // Parameter 5: Native system ID
    prm.push_back(iio::GetLibraryName(), VF::String());
    // Parameter 6: Preprocessor version
    prm.push_back(iio::GetVersion(), VF::String());
    // Parameter 7: Number of binary bits for integer representation
    prm.push_back(iio::kDefaultIntegerBits, VF::Integer());
    // Parameter 8: Single-precision magnitude
    prm.push_back(iio::kDefaultSinglePrecisionPowerMax, VF::Integer());
    // Parameter 9: Single-precision significance
    prm.push_back(iio::kDefaultSinglePrecisionDigits, VF::Integer());
    // Parameter 10: Double-precision magnitude
    prm.push_back(iio::kDefaultDoublePrecisionPowerMax, VF::Integer());
    // Parameter 11: Double-precision significance
    prm.push_back(iio::kDefaultDoublePrecisionDigits, VF::Integer());
    // Parameter 12: Product identification for the receiver
    prm.push_back(product_id, VF::String());
    // Parameter 13: Model space scale
    prm.push_back(gs.model_space_scale, VF::Real());
    // Parameter 14: Unit flag
    prm.push_back(static_cast<int>(gs.units_flag), VF::Integer());
    // Parameter 15: Unit name
    prm.push_back(i_model::ToUnitName(gs.units_flag), VF::String());
    // Parameter 16: Maximum number of line weight gradations
    prm.push_back(gs.line_weight_gradations, VF::Integer());
    // Parameter 17: Width of maximum line weight in units
    prm.push_back(gs.max_line_weight, VF::Real());
    // Parameter 18: Date and time of exchange file generation
    // YYYYMMDD.HHMMSS format
    prm.push_back(GetCurrentTimeString(), VF::String());
    // Parameter 19: Minimum user-intended resolution
    prm.push_back(gs.min_resolution, VF::Real());
    // Parameter 20: Approximate maximum coordinate value
    prm.push_back(gs.max_coordinate, VF::Real());
    // Parameter 21: Author name
    // TODO: どこかから指定可能にしたい (Parameter 21, 22)
    prm.push_back(std::string(), VF::String(true));
    // Parameter 22: Author organization
    prm.push_back(std::string(), VF::String(true));
    // Parameter 23: Version flag
    prm.push_back(static_cast<int>(gs.specification_version), VF::Integer());
    // Parameter 24: Drafting standard flag
    prm.push_back(static_cast<int>(gs.drafting_standard_flag), VF::Integer());
    // Parameter 25: Date and time model was created or modified
    prm.push_back(gs.date_time_generation, VF::String(true));
    // Parameter 26: Application protocol/subset identifier
    prm.push_back(std::string(), VF::String(true));

    return prm;
}
