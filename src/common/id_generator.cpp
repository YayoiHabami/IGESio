/**
 * @file common/id_generator.cpp
 * @brief IDを生成するためのクラスを定義する
 * @author Yayoi Habami
 * @date 2025-06-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/id_generator.h"

#include <iomanip>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "igesio/common/errors.h"



std::string igesio::ToString(const ObjectType type) {
    switch (type) {
        case ObjectType::kEntityFromIGES:
            return "Entity (from IGES)";
        case ObjectType::kEntityNew:
            return "Entity (in program)";
        case ObjectType::kEntityGraphics:
            return "Entity Graphics";
        case ObjectType::kIgesData:
            return "IGES Data";
        case ObjectType::kAssembly:
            return "Assembly";
        default:
            return "Unknown";
    }
}

bool igesio::Identifier::operator==(const Identifier& other) const {
    // prefixとsuffixの両方が等しい場合に等しいとみなす
    return GetIDPrefix() == other.GetIDPrefix()
        && GetIDSuffix() == other.GetIDSuffix();
}

bool igesio::Identifier::operator!=(const Identifier& other) const {
    return !(*this == other);
}



/**
 * 時刻生成・変換関連
 */

namespace {

using std::chrono::system_clock;
using chrono_s = std::chrono::seconds;
using chrono_ms = std::chrono::milliseconds;

/// @brief UTCの日時を表す構造体
struct UTCTime {
    uint16_t year;         // 12bits (0-4095)
    uint8_t  month;        //  4bits (1-12)
    uint8_t  day;          //  5bits (1-31)
    uint8_t  hour;         //  5bits (0-23)
    uint8_t  minute;       //  6bits (0-59)
    uint8_t  second;       //  6bits (0-59)
    uint16_t millisecond;  // 10bits (0-999)
};

/// @brief UTC時間を64ビット整数にパックする
/// @param time UTC時間
/// @return 下位48ビットにパックされたUTC時間
/// @note <null:16><year:12><month:4><day:5><hour:5><minute:6><second:6><millisecond:10>
uint64_t PackUTCTime(const UTCTime& time) {
    uint64_t packed_data = 0;

    packed_data |= static_cast<uint64_t>(time.year)        << 36;
    packed_data |= static_cast<uint64_t>(time.month)       << 32;
    packed_data |= static_cast<uint64_t>(time.day)         << 27;
    packed_data |= static_cast<uint64_t>(time.hour)        << 22;
    packed_data |= static_cast<uint64_t>(time.minute)      << 16;
    packed_data |= static_cast<uint64_t>(time.second)      << 10;
    packed_data |= static_cast<uint64_t>(time.millisecond);

    return packed_data;
}

/// @brief 64ビット整数からUTC時間をアンパックする
/// @param packed_data 下位48ビットにパックされたUTC時間
/// @return UTC時間
UTCTime UnpackUTCTime(const uint64_t packed_data) {
    UTCTime time;

    time.year        = (packed_data >> 36) & 0xFFF;  // 12 bits
    time.month       = (packed_data >> 32) & 0xF;    // 4 bits
    time.day         = (packed_data >> 27) & 0x1F;   // 5 bits
    time.hour        = (packed_data >> 22) & 0x1F;   // 5 bits
    time.minute      = (packed_data >> 16) & 0x3F;   // 6 bits
    time.second      = (packed_data >> 10) & 0x3F;   // 6 bits
    time.millisecond =  packed_data        & 0x3FF;  // 10 bits

    return time;
}

/// @brief 現在のUTC時間を取得する
/// @return UTC時間構造体
UTCTime GetCurrentUTCTime() {
    // 現在時刻をミリ秒精度で取得
    system_clock::time_point now = system_clock::now();
    auto now_ms = std::chrono::time_point_cast<chrono_ms>(now);
    // ミリ秒部分を計算
    auto ms = (now_ms - std::chrono::time_point_cast<chrono_s>(now_ms)).count();

    // UTCのtm構造体に変換
    std::tm utc_tm;
    auto time_t_now = system_clock::to_time_t(now_ms);
#ifdef _WIN32  // Windowsのみgmtime_sを使用
    gmtime_s(&utc_tm, &time_t_now);
#else  // その他の環境ではgmtime_rを使用
    gmtime_r(&time_t_now, &utc_tm);
#endif

    return UTCTime{
        static_cast<uint16_t>(utc_tm.tm_year + 1900),
        static_cast<uint8_t>(utc_tm.tm_mon + 1),
        static_cast<uint8_t>(utc_tm.tm_mday),
        static_cast<uint8_t>(utc_tm.tm_hour),
        static_cast<uint8_t>(utc_tm.tm_min),
        static_cast<uint8_t>(utc_tm.tm_sec),
        static_cast<uint16_t>(ms)
    };
}

/// @brief UTC時間構造体をtime_pointに変換する
/// @param utc_time UTC時間構造体
/// @return time_point
/// @note 夏時間は考慮しない. また、無効な日時の場合はエポックを返す
system_clock::time_point ConvertToTimePoint(const UTCTime& utc_time) {
    std::tm tm;
    tm.tm_year = utc_time.year - 1900;
    tm.tm_mon  = utc_time.month - 1;
    tm.tm_mday = utc_time.day;
    tm.tm_hour = utc_time.hour;
    tm.tm_min  = utc_time.minute;
    tm.tm_sec  = utc_time.second;
    tm.tm_isdst = -1;  // 夏時間情報なし

    std::time_t time_t_value = mktime(&tm);
    if (time_t_value == -1) {
        // 無効な時間の場合、エポックを返す
        return system_clock::time_point::min();
    }
    chrono_s sec(time_t_value);
    chrono_ms ms(utc_time.millisecond);
    return system_clock::time_point(sec + ms);
}

/// @brief time_pointをUTC時間構造体に変換する
/// @param time_point time_point
/// @return UTC時間構造体
UTCTime ConvertFromTimePoint(const system_clock::time_point& time_point) {
    // time_tに変換してtm構造体を取得
    auto time_t_value = system_clock::to_time_t(time_point);
    std::tm utc_tm;
#ifdef _WIN32  // Windowsのみgmtime_sを使用
    gmtime_s(&utc_tm, &time_t_value);
#else  // その他の環境ではgmtime_rを使用
    gmtime_r(&time_t_value, &utc_tm);
#endif

    auto duration = time_point.time_since_epoch();
    auto ms = std::chrono::duration_cast<chrono_ms>(duration).count() % 1000;

    return UTCTime{
        static_cast<uint16_t>(utc_tm.tm_year + 1900),
        static_cast<uint8_t>(utc_tm.tm_mon + 1),
        static_cast<uint8_t>(utc_tm.tm_mday),
        static_cast<uint8_t>(utc_tm.tm_hour),
        static_cast<uint8_t>(utc_tm.tm_min),
        static_cast<uint8_t>(utc_tm.tm_sec),
        static_cast<uint16_t>(ms)
    };
}

/// @brief UTC時間構造体を文字列に変換する
/// @param time UTC時間構造体
/// @return 文字列 (フォーマット: "YYYYMMDDThhmmss.sss")
std::string UTCTimeToString(const UTCTime& time) {
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << time.year
        << std::setw(2) << static_cast<int>(time.month)
        << std::setw(2) << static_cast<int>(time.day) << "T"
        << std::setw(2) << static_cast<int>(time.hour)
        << std::setw(2) << static_cast<int>(time.minute)
        << std::setw(2) << static_cast<int>(time.second) << "."
        << std::setw(3) << time.millisecond;
    return oss.str();
}

}  // namespace



/**
 * Identifier関連の実装
 */

namespace {

/// @brief ID用の乱数生成エンジン (std::mt19937_64のシード生成用)
std::random_device gen_seed;
/// @brief ID用の乱数生成器 (メルセンヌ・ツイスタ)
std::mt19937_64 generator(gen_seed());
/// @brief `generator`の排他制御用ミューテックス
std::mutex generator_mutex;

/// @brief 下位n_bitsビットがランダムな値の64ビット整数を生成する
/// @param n_bits 生成するビット数 (0-64)
/// @return 下位n_bitsビットがランダムな値、残りの上位ビットが0の64ビット整数
/// @throw igesio::ImplementationError n_bitsが64を超える場合
uint64_t GenerateRandomLowerBits(const size_t n_bits) {
    if (n_bits > 64) {
        throw igesio::ImplementationError("n_bits must be <= 64");
    }
    if (n_bits == 0) return 0;

    // 64ビットのランダムな値を生成
    std::lock_guard<std::mutex> lock(generator_mutex);
    uint64_t random_value = generator();
    if (n_bits == 64) {
        return random_value;
    }
    // 下位n_bitsビットを抽出
    uint64_t mask = (static_cast<uint64_t>(1) << n_bits) - 1;
    return random_value & mask;
}

/// @brief Identifierを可読形式の文字列に変換する
/// @param id 変換するIdentifier
/// @return 可読形式の文字列
/// @note フォーマット:
///       - Entity (from IGES):
///         "<obj-type>-<iges-int-id>-<de-pointer>-<entity-type>-<timestamp>"
///       - Entity (in program), EntityGraphics:
///         "<obj-type>-<random>-<entity-type>-<timestamp>"
///       - IgesData, Assembly:
///         "<obj-type>-<random1>-<random2>-<timestamp>"
std::string ToReadableString(const igesio::Identifier& id) {
    auto obj_type = id.GetObjectType();
    std::ostringstream oss;

    oss << igesio::ToString(obj_type) << "-";

    if (obj_type == igesio::ObjectType::kEntityFromIGES) {
        // Entity (from IGES)
        oss << id.GetIGESIntID().value_or(0) << "-";
        oss << id.GetDEPointer().value_or(0) << "-";
        oss << id.GetEntityType().value_or(0) << "-";
    } else if (obj_type == igesio::ObjectType::kEntityNew ||
               obj_type == igesio::ObjectType::kEntityGraphics) {
        // Entity (in program) または EntityGraphics
        oss << std::hex << (id.GetIDPrefix() & 0x00FFFFFFFFFFFFFF) << "-";
        oss << std::dec << id.GetEntityType().value_or(0) << "-";
    } else {
        // IgesData または Assembly
        oss << std::hex << (id.GetIDPrefix() & 0x00FFFFFFFFFFFFFF) << "-";
        oss << ((id.GetIDSuffix() >> 48) & 0x000000000000FFFF) << "-";
    }

    // タイムスタンプ
    auto timestamp = id.GetTimestamp();
    auto utc_time = ConvertFromTimePoint(timestamp);
    oss << UTCTimeToString(utc_time);

    return oss.str();
}

/// @brief Identifierを文字列に変換する
/// @param id 変換するIdentifier
/// @param readable_format 可読形式で出力する場合はtrue (デフォルト: false)
/// @return 文字列
/// @note フォーマット (readable_formatがfalseの場合):
///       O: ObjectType, I: IGESIntID, D: DEPointer, E: EntityType, T: Timestamp,
///       R: Random (いずれも16進数):
///       - Entity (from IGES):
///         "OO-IIIIIIII-DDDDDD-EEEE-TTTTTTTTTTTT"
///       - Entity (in program), EntityGraphics:
///         "OO-RRRRRRRRRRRRRR-EEEE-TTTTTTTTTTTT"
///       - IgesData, Assembly:
///         "OO-RRRRRRRRRRRRRR-RRRR-TTTTTTTTTTTT"
/// @note フォーマット (readable_formatがtrueの場合):
///       - Entity (from IGES):
///         "<obj-type>-<iges-int-id>-<de-pointer>-<entity-type>-<timestamp>"
///       - Entity (in program), EntityGraphics:
///         "<obj-type>-<random>-<entity-type>-<timestamp>"
///       - IgesData, Assembly:
///         "<obj-type>-<random1>-<random2>-<timestamp>"
std::string ToString(const igesio::Identifier& id,
                     const bool readable_format = false) {
    if (readable_format) return ToReadableString(id);

    std::ostringstream oss;

    // 上2桁はObjectType (16進数)
    auto obj_type = id.GetObjectType();
    oss << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(obj_type) << "-";

    if (obj_type == igesio::ObjectType::kEntityFromIGES) {
        // Entity (from IGES)
        // OO-IIIIIIII-DDDDDD-EEEE-TTTTTTTTTTTT

        // 次の8桁はIGESIntID (16進数)
        auto iges_int_id = id.GetIGESIntID().value_or(0);
        oss << std::setw(8) << std::setfill('0') << iges_int_id << "-";
        // 次の6桁はDEPointer (16進数)
        auto de_pointer = id.GetDEPointer().value_or(0);
        oss << std::setw(6) << std::setfill('0') << de_pointer << "-";
        // 次の4桁はEntityType (16進数)
        auto entity_type = id.GetEntityType().value_or(0);
        oss << std::setw(4) << std::setfill('0') << entity_type << "-";
    } else if (obj_type == igesio::ObjectType::kEntityNew ||
               obj_type == igesio::ObjectType::kEntityGraphics) {
        // Entity (in program) または EntityGraphics
        // OO-RRRRRRRRRRRRRR-EEEE-TTTTTTTTTTTT

        // 次の14桁はランダム部分 (16進数)
        oss << std::setw(14) << std::setfill('0')
            << (id.GetIDPrefix() & 0x00FFFFFFFFFFFFFF) << "-";
        // 次の4桁はEntityType (16進数)
        auto entity_type = id.GetEntityType().value_or(0);
        oss << std::setw(4) << std::setfill('0') << entity_type << "-";
    } else {
        // IgesData または Assembly
        // OO-RRRRRRRRRRRRRR-RRRR-TTTTTTTTTTTT

        // 残りの14桁はランダム部分 (16進数)
        oss << std::setw(14) << std::setfill('0')
            << (id.GetIDPrefix() & 0x00FFFFFFFFFFFFFF) << "-";

        // 次の4桁はランダム部分 (16進数)
        oss << std::setw(4) << std::setfill('0')
            << ((id.GetIDSuffix() >> 48) & 0x000000000000FFFF) << "-";
    }

    // タイムスタンプ (12桁の16進数)
    oss << std::setw(12) << std::setfill('0')
        << (id.GetIDSuffix() & 0x0000FFFFFFFFFFFF);

    return oss.str();
}



/// @brief 一意な識別子を生成・管理するクラス
/// @note 各オブジェクト、およびIDGeneratorが本クラスのポインタを保持し、
///       必要に応じて識別子情報を取得する. IDは64ビット整数2つのペアで表される.
///       各ビットフィールドの構成は後述する. また、オブジェクトごとにint型のIDも保持する.
///       こちらは主にIGESParameterVectorなど、IGESファイルとの入出力との互換性を保つ
///       ために使用される (IGESファイルではint型でエンティティを参照するため).
/// @note Entity (from IGES):
///       - <obj-type:8><iges-int-id:32><de-pointer:24>-<entity-type:16><timestamp:48>
/// @note Entity (in program), EntityGraphics:
///       - <obj-type:8><random:56>-<entity-type:16><timestamp:48>
/// @note IgesData, Assembly:
///       - <obj-type:8><random:56>-<random:16><timestamp:48>
class IdentifierImpl : public igesio::Identifier {
    /// @brief 一意なIDを表すペア (prefix, suffix)
    std::pair<uint64_t, uint64_t> unique_id;
    /// @brief int型のID
    int int_id;

    /// @brief suffixを設定する
    /// @param suffix_upper_16_bits suffixの上位16ビット
    void SetSuffix(uint16_t suffix_upper_16_bits) {
        uint64_t packed_time = PackUTCTime(GetCurrentUTCTime());
        uint64_t suffix = (static_cast<uint64_t>(suffix_upper_16_bits) << 48) | packed_time;
        unique_id.second = suffix;
    }

 public:
    /// @brief コンストラクタ (Entity from IGES file)
    /// @param iges_int_id 読み込み元のIGESファイルに対応するIgesDataのint型ID
    /// @param de_pointer DEレコードのシーケンス番号
    /// @param entity_type エンティティのタイプ
    /// @param int_id_value int型のID (IDGeneratorで生成された値)
    IdentifierImpl(const uint32_t iges_int_id, const uint32_t de_pointer,
                   const uint16_t entity_type, const int int_id_value)
            : int_id(int_id_value) {
        // prefix: <obj-type:8><iges-int-id:32><de-pointer:24>
        uint64_t prefix = (static_cast<uint64_t>(igesio::ObjectType::kEntityFromIGES) << 56)
                        | (static_cast<uint64_t>(iges_int_id) << 24)
                        | (static_cast<uint64_t>(de_pointer) & 0x00FFFFFF);
        unique_id.first = prefix;
        SetSuffix(entity_type);
    }

    /// @brief コンストラクタ (Entity created in program, EntityGraphics)
    /// @param obj_type オブジェクトの種類 (kEntityNewまたはkEntityGraphics)
    /// @param entity_type エンティティのタイプ
    /// @param int_id_value int型のID (IDGeneratorで生成された値)
    /// @throw igesio::ImplementationError obj_typeが不正な場合
    IdentifierImpl(const igesio::ObjectType obj_type, const uint16_t entity_type,
                   const int int_id_value)
            : int_id(int_id_value) {
        if (obj_type != igesio::ObjectType::kEntityNew &&
            obj_type != igesio::ObjectType::kEntityGraphics) {
            throw igesio::ImplementationError(
                "obj_type must be kEntityNew or kEntityGraphics");
        }

        // prefix: <obj-type:8><random:56>
        uint64_t random_bits = GenerateRandomLowerBits(56);
        uint64_t prefix = (static_cast<uint64_t>(obj_type) << 56)
                        | random_bits;
        unique_id.first = prefix;
        SetSuffix(entity_type);
    }

    /// @note IgesData, Assembly
    IdentifierImpl(const igesio::ObjectType obj_type, const int int_id_value)
            : int_id(int_id_value) {
        if (obj_type != igesio::ObjectType::kIgesData &&
            obj_type != igesio::ObjectType::kAssembly) {
            throw std::invalid_argument(
                "obj_type must be kIgesData or kAssembly");
        }

        // prefix: <obj-type:8><random:56>
        uint64_t random_bits = GenerateRandomLowerBits(56);
        uint64_t prefix = (static_cast<uint64_t>(obj_type) << 56)
                        | random_bits;
        unique_id.first = prefix;

        // suffix: <random:16><timestamp:48>
        uint16_t upper = static_cast<uint16_t>(GenerateRandomLowerBits(16));
        SetSuffix(upper);
    }

    /// @brief コピーコンストラクタ
    IdentifierImpl(const IdentifierImpl& other)
        : unique_id(other.unique_id), int_id(other.int_id) {}
    /// @brief コピー代入演算子
    IdentifierImpl& operator=(const IdentifierImpl& other) {
        if (this != &other) {
            unique_id = other.unique_id;
            int_id = other.int_id;
        }
        return *this;
    }

    /// @brief ムーブコンストラクタ
    IdentifierImpl(IdentifierImpl&& other) noexcept
        : unique_id(std::move(other.unique_id)), int_id(other.int_id) {}
    /// @brief ムーブ代入演算子
    IdentifierImpl& operator=(IdentifierImpl&& other) noexcept {
        if (this != &other) {
            unique_id = std::move(other.unique_id);
            int_id = other.int_id;
        }
        return *this;
    }

    /// @brief デストラクタ
    ~IdentifierImpl() override = default;

    const std::pair<uint64_t, uint64_t>& GetUniqueID() const override {
        return unique_id;
    }
    uint64_t GetIDPrefix() const override { return unique_id.first; }
    uint64_t GetIDSuffix() const override { return unique_id.second; }
    int GetIntID() const override { return int_id; }

    igesio::ObjectType GetObjectType() const override {
        // prefixの上位8ビットを取得してObjectTypeにキャスト
        return static_cast<igesio::ObjectType>((GetIDPrefix() >> 56) & 0xFF);
    }
    std::optional<uint32_t> GetIGESIntID() const override {
        // オブジェクトがIGESファイルから読み込まれたエンティティでない場合はnulloptを返す
        if (GetObjectType() != igesio::ObjectType::kEntityFromIGES) {
            return std::nullopt;
        }

        // prefix9-40ビットを取得してuint32_tにキャスト
        return static_cast<uint32_t>((GetIDPrefix() >> 24) & 0x00000000FFFFFFFF);
    }
    std::optional<uint32_t> GetDEPointer() const override {
        // オブジェクトがIGESファイルから読み込まれたエンティティでない場合はnulloptを返す
        if (GetObjectType() != igesio::ObjectType::kEntityFromIGES) {
            return std::nullopt;
        }

        // prefixの下位24ビットを取得してuint32_tにキャスト
        return static_cast<uint32_t>(GetIDPrefix() & 0x0000000000FFFFFF);
    }
    std::optional<uint16_t> GetEntityType() const override {
        auto obj_type = GetObjectType();

        // オブジェクトがID関連ではない場合はnulloptを返す
        if (obj_type != igesio::ObjectType::kEntityFromIGES &&
            obj_type != igesio::ObjectType::kEntityNew &&
            obj_type != igesio::ObjectType::kEntityGraphics) {
            return std::nullopt;
        }

        // suffixの上位16ビットを取得してuint16_tにキャスト
        return static_cast<uint16_t>((GetIDSuffix() >> 48) & 0xFFFF);
    }
    std::chrono::system_clock::time_point GetTimestamp() const override {
        // suffixの下位48ビットからタイムスタンプを復元
        auto packed_time = unique_id.second & 0x0000FFFFFFFFFFFF;
        UTCTime utc_time = UnpackUTCTime(packed_time);
        return ConvertToTimePoint(utc_time);
    }
};

}  // namespace



/**
 * ObjectID
 */

const std::shared_ptr<const igesio::Identifier>&
igesio::ObjectID::GetIdentifier() const {
    return identifier;
}

int igesio::ObjectID::ToInt() const {
    return identifier ? identifier->GetIntID() : kInvalidIntID;
}

bool igesio::ObjectID::IsSet() const {
    return identifier != nullptr;
}

bool igesio::ObjectID::operator==(const ObjectID& other) const {
    if (!identifier && !other.identifier) return true;
    if (!identifier || !other.identifier) return false;
    return *identifier == *(other.identifier);
}

bool igesio::ObjectID::operator!=(const ObjectID& other) const {
    return !(*this == other);
}

bool igesio::ObjectID::operator==(const std::shared_ptr<const Identifier>& other) const {
    if (!identifier && !other) return true;
    if (!identifier || !other) return false;
    return *identifier == *other;
}

bool igesio::ObjectID::operator!=(const std::shared_ptr<const Identifier>& other) const {
    return !(*this == other);
}

std::string igesio::ToString(const ObjectID& object_id, const bool readable_format) {
    if (!object_id.identifier) {
        return "Unset ID";
    }
    return ::ToString(*(object_id.identifier), readable_format);
}

std::ostream& igesio::operator<<(std::ostream& os, const ObjectID& object_id) {
    os << ToString(object_id);
    return os;
}



/**
 * IDGenerator
 */

std::size_t igesio::IDGenerator::PairHash::operator()(
        const std::pair<ObjectID, uint16_t>& p) const {
    auto h1 = std::hash<uint64_t>{}(p.first.GetIdentifier()->GetIDPrefix());
    auto h2 = std::hash<uint64_t>{}(p.first.GetIdentifier()->GetIDSuffix());
    auto h3 = std::hash<uint16_t>{}(p.second);

    return h1 ^ (h2 << 1) ^ (h3 << 2);
}

int igesio::IDGenerator::GenerateNewIntID() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 使用中の最大のint_id + 1がmax_int_id_未満であればそれを採用
    int new_int_id = kInvalidIntID + 1;
    if (!int_id_map_.empty()) {
        new_int_id = int_id_map_.rbegin()->first + 1;
    }
    if (new_int_id < static_cast<int>(max_int_id_)) {
        // とりあえずUnsetIDを登録しておく
        // (直後にGenerateNewIntIDを呼び出すと競合する可能性があるため)
        int_id_map_[new_int_id] = std::shared_ptr<Identifier>();
        return new_int_id;
    }

    // expired_int_ids_から最も値の小さいint_idを再利用
    if (!expired_int_ids_.empty()) {
        int reused_id = *expired_int_ids_.begin();
        expired_int_ids_.erase(expired_int_ids_.begin());
        return reused_id;
    }

    // expiredなIdentifierを見つけたらそのint_idを再利用
    for (auto it = int_id_map_.begin(); it != int_id_map_.end(); ++it) {
        if (it->second.expired()) {
            return it->first;
        }
    }

    // エラーを投げる
    throw std::runtime_error("Too many IDs in use; cannot generate new int_id");
}

void igesio::IDGenerator::Register(const std::shared_ptr<Identifier>& identifier) {
    std::lock_guard<std::mutex> lock(mutex_);
    int_id_map_[identifier->GetIntID()] = identifier;
}

igesio::ObjectID igesio::IDGenerator::Generate(const ObjectType obj_type) {
    int new_int_id = kInvalidIntID;
    std::shared_ptr<Identifier> identifier;
    try {
        new_int_id = GenerateNewIntID();
        identifier = std::make_shared<IdentifierImpl>(obj_type, new_int_id);
    } catch (...) {
        Release(new_int_id);
        throw;
    }

    Register(identifier);
    return ObjectID(identifier);
}

igesio::ObjectID igesio::IDGenerator::Generate(
        const ObjectType obj_type, const uint16_t entity_type) {
    int new_int_id = kInvalidIntID;
    std::shared_ptr<Identifier> identifier;
    try {
        new_int_id = GenerateNewIntID();
        identifier = std::make_shared<IdentifierImpl>(obj_type, entity_type, new_int_id);
    } catch (...) {
        Release(new_int_id);
        throw;
    }

    Register(identifier);
    return ObjectID(identifier);
}

igesio::ObjectID
igesio::IDGenerator::Reserve(const ObjectID& iges_id,
                             const uint16_t entity_type,
                             const uint16_t de_pointer) {
    if (!iges_id.identifier) {
        throw std::invalid_argument("Unset IGES ID cannot have reserved ID");
    } else if (iges_id.identifier->GetObjectType() != ObjectType::kIgesData) {
        throw std::invalid_argument("Parent object of kEntityFromIGES must be kIgesData");
    }

    // すでに予約済みの場合、そのIDを返す
    auto key = std::make_pair(iges_id, de_pointer);
    auto reserved_int_id = kInvalidIntID;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = reserved_ids_.find(key);
        if (it != reserved_ids_.end()) reserved_int_id = it->second;
    }
    if (reserved_int_id != kInvalidIntID) {
        // GetByIntID内でmutexを取得するため、ここではロックを解放してから呼び出す
        return GetByIntID(reserved_int_id);
    }

    // 新規IDを生成して登録
    auto int_id = GenerateNewIntID();
    std::shared_ptr<Identifier> identifier;
    try {
        identifier = std::make_shared<IdentifierImpl>(
                iges_id.ToInt(), de_pointer, entity_type, int_id);
        Register(identifier);
    } catch (...) {
        Release(int_id);
        throw;
    }

    // 登録済みIDとしてマーク
    std::lock_guard<std::mutex> lock(mutex_);
    reserved_ids_[key] = int_id;
    return ObjectID(identifier);
}

igesio::ObjectID
igesio::IDGenerator::GetReservedID(const ObjectID& iges_id,
                                   const uint16_t de_pointer) {
    int reserved_id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto key = std::make_pair(iges_id, de_pointer);
        auto it = reserved_ids_.find(key);
        if (it == reserved_ids_.end()) {
            throw std::invalid_argument(
                "ID not reserved for the given IGES ID ("
                + ::ToString(*(iges_id.identifier)) + ") and DE Pointer ("
                + std::to_string(de_pointer) + ")");
        }
        reserved_id = it->second;
    }
    return ObjectID(GetByIntID(reserved_id));
}

std::set<int> igesio::IDGenerator::GetAllIntIDs() {
    std::set<int> int_ids;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : int_id_map_) {
        int_ids.insert(pair.first);
    }

    return int_ids;
}

std::optional<igesio::ObjectID>
igesio::IDGenerator::TryGetByIntID(const int int_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = int_id_map_.find(int_id);
    if (it == int_id_map_.end() || it->second.expired()) {
        return std::nullopt;
    }

    return ObjectID(it->second.lock());
}

igesio::ObjectID
igesio::IDGenerator::GetByIntID(const int int_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = int_id_map_.find(int_id);
    if (it == int_id_map_.end()) {
        throw std::invalid_argument("ID " + std::to_string(int_id) + " not found");
    }

    if (it->second.expired()) {
        throw std::invalid_argument("ID " + std::to_string(int_id) + " is expired");
    }

    return ObjectID(it->second.lock());
}

void igesio::IDGenerator::Release(const int int_id) {
    if (int_id == kInvalidIntID) return;

    std::lock_guard<std::mutex> lock(mutex_);
    int_id_map_.erase(int_id);
    expired_int_ids_.insert(int_id);

    // 予約済みIDからも削除
    for (auto it = reserved_ids_.begin(); it != reserved_ids_.end(); ) {
        if (it->second == int_id) {
            it = reserved_ids_.erase(it);
            break;
        }
        ++it;
    }
}
