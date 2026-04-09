/**
 * @file models/global_param.h
 * @brief IGESファイルのグローバルセクションのデータを保持する構造体
 * @author Yayoi Habami
 * @date 2025-04-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_MODELS_GLOBAL_PARAM_H_
#define IGESIO_MODELS_GLOBAL_PARAM_H_

#include <limits>
#include <string>
#include <vector>

#include "igesio/common/serialization.h"
#include "igesio/common/iges_parameter_vector.h"



/// @brief IGESのデータを保持する構造体などを定義する
namespace igesio::models {

/// @brief 単位フラグおよび単位名 (グローバルパラメータ14 & 15)
/// @note IGES5.3では、本来単位フラグにおいて3を指定することで、単位名において
///       定義された単位 (MIL12またはIEEE260に準拠する単位) を使用することができる
///       (ここで定義される10種類以外の単位を使用できる).
///       しかしながら、種類が膨大になるため、ここでは以下の10種類以外の単位を受け付けない
/// @todo 単位フラグが3として指定された場合の処理を実装する
enum class UnitFlag {
    /// @brief インチ (`2HIN` or `4HINCH`)
    kInch = 1,
    /// @brief ミリメートル (`2HMM`)
    kMillimeter = 2,
    /// @brief 単位名 (グローバルパラメータ15) に従う
    kUnitName = 3,
    /// @brief フィート (`2HFT`)
    kFeet = 4,
    /// @brief マイル (`2HMI`)
    kMile = 5,
    /// @brief メートル (`1HM`)
    kMeter = 6,
    /// @brief キロメートル (`2HKM`)
    kKilometer = 7,
    /// @brief ミル (1/1000インチ) (`3HMIL`)
    kMil = 8,
    /// @brief ミクロン (1/1000ミリメートル) (`2HUM`)
    kMicron = 9,
    /// @brief センチメートル (`2HCM`)
    kCentimeter = 10,
    /// @brief マイクロインチ (1/1000000インチ) (`3HUIN`)
    kMicroInch = 11,
};

/// @brief IGESファイルのバージョンフラグ (グローバルパラメータ23)
/// @note 1よりも小さい値を見つけた場合は3 (IGES 2.0)を、11よりも大きい値を見つけた
///       場合は11 (IGES 5.3) を割り当てる.
enum class VersionFlag {
    /// @brief バージョン1.0
    /// @note *Initial Graphics Exchange Specification (IGES), Version 1.0*, NBSIR 80-1978 (R),
    ///       U.S. National Bureau of Standards, 1980. Out of print.
    kVersion1_0 = 1,
    /// @brief ANSI Y14.26M -1981
    /// @note *Digital Representation for Communication of Product Definition Data*, Parts 1, 2,
    ///       and 3, (Y14.26M- 1981), American National Standards Institute, 1981. Permanently
    ///       out of print.
    kANSI81 = 2,
    /// @brief IGES 2.0
    /// @note *Initial Graphics Exchange Specification (IGES), Version 2.0*, NBSIR 82-2631 (AF),
    ///       U.S. National Bureau of Standards, 1982. Available from the National Technical
    ///       Information Service (NTIS) as PB83-137448.
    kVersion2_0 = 3,
    /// @brief IGES 3.0
    /// @note *Initial Graphics Exchange Specification (IGES), Version 3.0*, NBSIR 86-3359,
    ///       U.S. National Bureau of Standards, 1986. Available from the National Technical
    ///       Information Service (NTIS) as PB86-199759.
    kVersion3_0 = 4,
    /// @brief ASME/ANSI Y14.26M -1987
    /// @note *Digital Representation for Communication of Product Definition Data*,
    ///       (ASME/ANSI Y14.26M-1987), The American Society of Mechanical Engineers or
    ///       the American National Standards Institute, 1987.
    kASME87 = 5,
    /// @brief IGES 4.0
    /// @note *Initial Graphics Exchange Specification (IGES), Version 4.0*, NBSIR 88-3813,
    ///       U.S. National Bureau of Standards, 1988. Available from the National Technical
    ///       Information Service (NTIS) as PB88-235452.
    kVersion4_0 = 6,
    /// @brief ASME Y14.26M -1989
    /// @note *Digital Representation for Communication of Product Definition Data*, (ASME
    ///       Y14.26M- 1989), The American Society of Mechanical Engineers or the American
    ///       National Standards Institute, 1989.
    kASME89 = 7,
    /// @brief IGES 5.0
    /// @note *Initial Graphics Exchange Specification (IGES), Version 5.0*, NISTIR 4412,
    ///       U.S. National Institute of Standards and Technology, 1990.
    kVersion5_0 = 8,
    /// @brief IGES 5.1
    /// @note *Initial Graphics Exchange Specification (IGES), Version 5.1*, USPRO/IPO,
    ///       September, 1991.
    kVersion5_1 = 9,
    /// @brief USPRO/IPO100 IGES5.2
    /// @note *Digital Representation for Communication of Product Definition Data*, USPRO/
    ///       IPO-100 IGES 5.2, U.S. Product Data Association, 1993
    kUSPRO93 = 10,
    /// @brief IGES 5.3
    /// @note *Initial Graphics Exchange Specification (IGES), Version 5.3*, USPRO/IPO,
    ///       September, 1996.
    kVersion5_3 = 11
};

/// @brief 製図標準フラグ (グローバルパラメータ24)
/// @note ファイルが準拠する製図標準を指定する. ない場合は0 (kNone) を指定する.
enum class DraftingStandardFlag {
    /// @brief 標準の指定なし
    kNone = 0,
    /// @brief ISO (国際標準化機構)
    kISO = 1,
    /// @brief AFNOR (フランス規格協会)
    kAFNOR = 2,
    /// @brief ANSI (米国国家規格協会)
    kANSI = 3,
    /// @brief BSI (英国規格協会)
    kBSI = 4,
    /// @brief CSA (カナダ規格協会)
    kCSA = 5,
    /// @brief DIN (ドイツ規格協会)
    kDIN = 6,
    /// @brief JIS (日本工業規格)
    kJIS = 7,
};

/// @brief パラメータ区切り文字 (グローバルパラメータ1) のデフォルト値
constexpr char kDefaultParamDelim = ',';
/// @brief レコード区切り文字 (グローバルパラメータ2) のデフォルト値
constexpr char kDefaultRecordDelim = ';';
/// @brief モデル空間のスケール (グローバルパラメータ13) のデフォルト値
constexpr double kDefaultModelSpaceScale = 1.0;
/// @brief 単位フラグ (グローバルパラメータ14) のデフォルト値
constexpr UnitFlag kDefaultUnitFlag = UnitFlag::kInch;
/// @brief 線の太さの最大数 (グローバルパラメータ16) のデフォルト値
/// @note IGES仕様では1だが、実用上の理由から5に拡張している
constexpr int kDefaultLineWeightGradations = 5;
/// @brief 最大線の太さ (グローバルパラメータ17) のデフォルト値
/// @note IGES仕様では規定されていないが、本ライブラリでは10をデフォルト値として設定している
constexpr double kDefaultMaxLineWeight = 10.0;
/// @brief モデル内の最大座標値 (グローバルパラメータ20) のデフォルト値
/// @note デフォルト値は0.0であり、「最大座標値は未指定」とみなす
constexpr double kDefaultMaxCoordinate = 0.0;
/// @brief 著者名 (グローバルパラメータ21) のデフォルト値
/// @note デフォルト値はNULL（ここでは空文字列で表現）であり、「未指定」と解釈される
constexpr std::string_view kDefaultAuthor = "";
/// @brief 作成者の組織 (グローバルパラメータ22) のデフォルト値
/// @note デフォルト値はNULL（ここでは空文字列で表現）であり、「未指定」と解釈される
constexpr std::string_view kDefaultAuthorOrg = "";
/// @brief バージョンフラグ (グローバルパラメータ23) のデフォルト値
/// @note 本来のデフォルト値は3 (IGES 2.0) であるが、ここでは
///       11 (IGES 5.3) をデフォルト値として設定している
constexpr VersionFlag kDefaultSpecificationVersion = VersionFlag::kVersion5_3;
/// @brief 製図標準フラグ (グローバルパラメータ24) のデフォルト値
constexpr DraftingStandardFlag kDefaultDraftingStandardFlag = DraftingStandardFlag::kNone;
/// @brief モデルが作成または最終更新された日時 (グローバルパラメータ25) のデフォルト値
/// @note デフォルト値はNULL（ここでは空文字列で表現）であり、「未指定」と解釈される
constexpr std::string_view kDefaultDateTimeModified = "";
/// @brief アプリケーションプロトコル/サブセット (グローバルパラメータ26) のデフォルト値
/// @note デフォルト値はNULL（ここでは空文字列で表現）であり、「未指定」と解釈される
constexpr std::string_view kDefaultProtocolIdentifier = "";

/// @brief IGES ファイルのグローバルセクションを表す構造体
/// @note briefの後ろの括弧は、何番目のグローバルパラメータであるかを示す. 例えば、
///       record_delim (2) の場合、"Global Parameter 2"と呼称されることがある
/// @note いずれもIGES5.3においては必須パラメータであり、デフォルト値を設定している
///       ものを除き、全て明示的に指定する必要がある. ただし、パラメータ8から11まで
///       (浮動小数点数の桁数など) は、IGES5.3ではデフォルト値は設定されていないが、
///       C++側の制約があるため、デフォルト値として設定している.
struct GlobalParam {
    /// @brief パラメータ区切り文字 (1)
    char param_delim = kDefaultParamDelim;
    /// @brief レコード区切り文字 (2)
    char record_delim = kDefaultRecordDelim;
    /// @brief 送信システムからの製品識別 (3)
    /// @note 送信者がこの製品を参照するために使用する名前または識別子
    std::string product_id;
    /// @brief ファイル名 (4)
    /// @note 交換ファイルの名前
    std::string file_name;
    /// @brief ネイティブシステムID (5)
    /// @note 当該ファイルを作成したネイティブソフトウェアを識別するための情報
    std::string native_system_id;
    /// @brief プリプロセッサバージョン (6)
    /// @note このファイルを作成したプリプロセッサのバージョンまたはリリース日
    std::string preprocessor_version;
    /// @brief 整数表現のためのバイナリビット数 (7)
    /// @note 整数表現に存在するビット数 (=> 整数パラメータの有効値の範囲を制限)
    int integer_bits = kDefaultIntegerBits;
    /// @brief 送信システム上の単精度浮動小数点数で表現可能な最大10の累乗 (8)
    int single_precision_power_max = kDefaultSinglePrecisionPowerMax;
    /// @brief 送信システム上の単精度浮動小数点数の有効桁数 (9)
    int single_precision_digits = kDefaultSinglePrecisionDigits;
    /// @brief 送信システム上の倍精度浮動小数点数で表現可能な最大10の累乗 (10)
    int double_precision_power_max = kDefaultDoublePrecisionPowerMax;
    /// @brief 送信システム上の倍精度浮動小数点数の有効桁数 (11)
    int double_precision_digits = kDefaultDoublePrecisionDigits;
    /// @brief 受信システムの製品識別 (12)
    /// @note 受信システムのソフトウェアがこの製品を参照するために使用する名前
    ///       または識別子. デフォルト値は `product_id` (3).
    std::string receiving_system_id;
    /// @brief モデル空間スケール (13)
    /// @note モデル空間と実空間のの比率を示す. 例えば `0.125` は、1モデル空間単位が
    ///       8実空間単位に等しいことを示す.
    double model_space_scale = kDefaultModelSpaceScale;
    /// @brief 単位フラグ (14) & 単位名 (15)
    /// @note 本来は、単位フラグと単位名は別々に指定することができるが、
    ///       ここでは、単位フラグ3を指定して単位名で上記10種類以外の単位を
    ///       選択したケースを取り扱わない.
    UnitFlag units_flag = kDefaultUnitFlag;
    /// @brief 線の太さの最大数 (16)
    /// @note 線の太さの等分割数、ゼロよりも大きな値を指定する必要がある.
    int line_weight_gradations = kDefaultLineWeightGradations;
    /// @brief 最大線の太さの幅 (17)
    /// @note ファイル内で可能な最も太い線の実際の幅 (モデル単位).
    double max_line_weight;
    /// @brief 交換ファイル生成日時 (18)
    /// @note このファイルが作成された時刻を`YYYYMMDD.HHNNSS`または`YYMMDD.HHNNSS`形式
    ///       で示す. `YY`を指定した場合、`19YY`として解釈される.
    std::string date_time_generation;
    /// @brief モデルの最小ユーザー意図解像度 (19)
    /// @note 受信システムが識別可能とみなすべき、モデル空間単位での座標感の最小距離を
    ///       示す. 例えば`0.001`のとき、ポストプロセッサはファイル内の座標位置が`.001`
    ///       モデル空間単位未満の差異について「一致」とみなす.
    double min_resolution;
    /// @brief モデル内の最大座標値 (20)
    /// @note 変換後にこのモデルに存在するすべての座標データの絶対値の上限を示す.
    ///       例えば`1000.0`のとき、全ての座標に対して|X|, |Y|, |Z| <= 1000.0
    ///       である必要がある.
    /// @note デフォルト値は0.0であり、「最大座標値は未指定」とみなす.
    double max_coordinate = kDefaultMaxCoordinate;
    /// @brief 著者名 (21)
    /// @note デフォルト値はNULL（ここでは空文字列で表現）であり、「未指定」と解釈される.
    std::string author_name = std::string(kDefaultAuthor);
    /// @brief 作成者の組織 (22)
    /// @note デフォルト値はNULL（ここでは空文字列で表現）であり、「未指定」と解釈される.
    std::string author_organization = std::string(kDefaultAuthorOrg);
    /// @brief このファイルが準拠する仕様のバージョン (23)
    VersionFlag specification_version = kDefaultSpecificationVersion;
    /// @brief このファイルが準拠するドラフティング標準のフラグ値 (24)
    DraftingStandardFlag drafting_standard_flag = kDefaultDraftingStandardFlag;
    /// @brief モデルが作成または最終更新された日時 (25)
    /// @note このファイルが作成または最終更新された時刻を`YYYYMMDD.HHNNSS`または
    ///       `YYMMDD.HHNNSS`形式で示す. `YY`を指定した場合、`19YY`として解釈される.
    /// @note デフォルト値はNULL（ここでは空文字列で表現）であり、「未指定」と解釈される.
    std::string date_time_modified = std::string(kDefaultDateTimeModified);
    /// @brief アプリケーションプロトコル、アプリケーションサブセット、
    ///        ミルスペック、またはユーザー定義プロトコルまたはサブセットの記述子 (26)
    /// @note デフォルト値はNULL（ここでは空文字列で表現）であり、「未指定」と解釈される.
    std::string protocol_identifier = std::string(kDefaultProtocolIdentifier);

    /// @brief シリアライズ (IGES形式の文字列化) に必要なパラメータを取得する
    /// @return シリアライズに必要なパラメータ
    SerializationConfig GetSerializationConfig() const;
};

/// @brief 描画に関連するグローバルパラメータ
struct GraphicsGlobalParam {
    /// @brief モデル空間のスケール
    /// @note グローバルパラメータ13
    double model_space_scale;
    /// @brief 線の太さの最大数
    /// @note グローバルパラメータ16
    int line_weight_gradations;
    /// @brief 最大線の太さの幅
    /// @note グローバルパラメータ17
    double max_line_weight;

    /// @brief コンストラクタ
    /// @param model_space_scale モデル空間のスケール
    /// @param line_weight_gradations 線の太さの最大数
    /// @param max_line_weight 最大線の太さの幅
    GraphicsGlobalParam(const double model_space_scale,
                        const int line_weight_gradations,
                        const double max_line_weight)
        : model_space_scale(model_space_scale),
          line_weight_gradations(line_weight_gradations),
          max_line_weight(max_line_weight) {}

    /// @brief コンストラクタ
    /// @param global_param グローバルパラメータ
    explicit GraphicsGlobalParam(const GlobalParam& global_param)
        : model_space_scale(global_param.model_space_scale),
          line_weight_gradations(global_param.line_weight_gradations),
          max_line_weight(global_param.max_line_weight) {}

    /// @brief 線の表示太さを計算する
    /// @param line_weight_number 線の表示太さの番号 (12th DEパラメータ)
    /// @return 線の表示太さ
    /// @note 線の表示太さは、`line_weight_number` (12th DEパラメータ)
    ///       を用いて、line_weight_number * max_line_weight /
    ///       line_weight_gradations で計算される.
    double GetLineWeight(const int line_weight_number) const {
        return line_weight_number * max_line_weight / line_weight_gradations;
    }
};



/**
 * Enumクラス用変換
 */

/// @brief 単位フラグの数値からUnitFlagを取得する
/// @param unit_flag 単位フラグの数値
/// @return 単位フラグの数値に対応するUnitFlag
/// @throw igesio::TypeConversionError 単位フラグの数値が無効な場合
UnitFlag ToUnitFlagEnum(const int);

/// @brief 単位名 (H文字列) からUnitFlagを取得する
/// @param unit_name 単位名 (H文字列)
/// @return 単位名 (H文字列) に対応するUnitFlag
/// @throw igesio::TypeConversionError 単位名 (H文字列) が無効な場合
UnitFlag ToUnitFlagEnum(const std::string&);

/// @brief UnitFlagから単位名 (H文字列) を取得する
/// @param unit_flag 単位フラグ
/// @return 単位フラグに対応する単位名 (H文字列)
/// @throw igesio::TypeConversionError 単位フラグが無効な場合、
///        フラグ3を指定した場合
std::string ToUnitName(const UnitFlag);

/// @brief バージョンフラグの数値からVersionFlagを取得する
/// @param version_flag バージョンフラグの数値
/// @return バージョンフラグの数値に対応するVersionFlag
VersionFlag ToVersionFlagEnum(const int) noexcept;

/// @brief 製図標準フラグの数値からDraftingStandardFlagを取得する
/// @param drafting_standard_flag 製図標準フラグの数値
/// @return 製図標準フラグの数値に対応するDraftingStandardFlag
/// @throw igesio::TypeConversionError 製図標準フラグの数値が無効な場合
DraftingStandardFlag ToDraftingStandardFlagEnum(const int);



/**
 * GlobalParam用変換
 */

/// @brief グローバルセクションのパラメータを設定する
/// @param p_delim パラメータ区切り文字 (パラメータ1)
/// @param r_delim レコード区切り文字 (パラメータ2)
/// @param prm 残りのグローバルセクションのパラメータ (パラメータ3~26).
///        この関数においては、最低限パラメータ3~20まで存在すればよい.
/// @return グローバルセクションのパラメータ
/// @throw igesio::TypeConversionError 型変換エラーが発生した場合
/// @throw igesio::SectionFormatError 指定されたパラメータ数が不足している場合
/// @throw igesio::NotImplementedError 単位フラグが3で、単位名がIGES5.3
///        で定義されている10種類以外の単位の場合
GlobalParam SetGlobalSectionParams(const char, const char,
                                   const std::vector<std::string>&);

/// @brief GlobalParamをIGESParameterVectorに変換する
/// @param param グローバルセクションのパラメータ
/// @param file_name ファイル名 (パラメータ4)、指定した場合は上書きする
/// @return IgesParameterVectorに変換されたグローバルセクションのパラメータ
IGESParameterVector ToVector(const GlobalParam&,
                             const std::string& = std::string());

}  // namespace igesio::models

#endif  // IGESIO_MODELS_GLOBAL_PARAM_H_
