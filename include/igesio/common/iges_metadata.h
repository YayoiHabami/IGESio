/**
 * @file common/iges_metadata.h
 * @brief IGESに関するメタデータを定義するヘッダファイル
 * @author Yayoi Habami
 * @date 2025-04-13
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_COMMON_IGES_METADATA_H_
#define IGESIO_COMMON_IGES_METADATA_H_

#include <string>

/// @brief 本ライブラリのルート名前空間
/// @note 直接の子要素は、ライブラリ全体で使用する定数や列挙体、
///       およびエラークラスなど
namespace igesio {

/**
 * 定数定義
 */

/// @brief 1行のカラム数 (改行文字を除く)
/// @note 圧縮形式のデータセクションを除く. このセクションでは、
///       全ての行が73文字未満となる
/// @note 正確には、1行当たりのバイト数を表す. ファイルによっては
///       非ASCII文字が含まれる可能性があり、この場合、見かけ上
///       1行のカラム数が80文字以外となる可能性がある.
constexpr int kMaxColumn = 80;
/// @brief セクション判別文字のカラム位置
/// @note C,S,G,D,P,Tの文字がこの位置に存在する
/// @note 圧縮形式のデータセクションでは、ここに文字は存在しない
constexpr int kColIdentify = 73;
/// @brief Parameter Dataセクションにおける、Directory Entry
///        セクションへの逆ポインタが開始するカラム位置
constexpr int kColDEPointer = 65;
/// @brief Terminateセクションにおける、データ領域の長さ
constexpr int kColTerminateDataPart = 32;
/// @brief 固定幅のフィールドのカラム数
/// @note Directory Entryセクションのフィールドのカラム数、
///       Parameter Dataセクションの逆ポインタのカラム数など
constexpr int kFixedColWidth = 8;

/// @brief ポインター型の数値の最大値 (See Section 2.2.2.4)
constexpr int kMaxPointerValue = 99999999;
/// @brief ポインター型の数値の最小値 (See Section 2.2.2.4)
constexpr int kMinPointerValue = -99999999;



/// @brief IGESのセクション
/// @note フラグ部(圧縮形式のみ)、スタート部、グローバル部、
///       ディレクトリエントリ部、パラメータデータ部、ターミネート部
enum class SectionType {
    /// @brief フラグセクション (圧縮形式のみ)
    /// @note 圧縮形式のIGESファイルでは、最初の行にフラグ部が存在する.
    ///       それ以外の形式では存在しない
    kFlag = 0,
    /// @brief スタートセクション
    /// @note 人間が読むことの可能な形式で記述される、ファイルについての
    ///       情報を含むセクション
    kStart = 1,
    /// @brief グローバルセクション
    /// @note プリプロセッサで必要な、IGESファイルの読み込み全般に関する
    ///       情報を含むセクション
    kGlobal = 2,
    /// @brief ディレクトリエントリセクション
    /// @note 使用するエンティティに関する基本的な情報を含むセクション
    kDirectory = 3,
    /// @brief パラメータデータセクション
    /// @note 各エンティティの、パラメータを含む詳細な情報を含むセクション
    kParameter = 4,
    /// @brief ターミネートセクション
    /// @note IGESファイルの終端を示すセクション.
    ///       これ以降の行は無視される
    kTerminate = 5,
    /// @brief データセクション（圧縮形式のみ）
    /// @note 圧縮形式のIGESファイルでは、ディレクトリエントリセクションと
    ///       パラメータデータセクションを統合した、データセクションが存在する
    kData = 6
};



/**
 * データ型関連
 */

/// @brief 本ライブラリで使用する、IGESのパラメータを表現するために
///        使用するC++のデータ型を表す列挙型
/// @note 本ライブラリでは、IGES 5.3で使用される6つのデータ型を表現するために、
///       bool (logical), int (integer), double (real), ObjectID (pointer),
///       std::string (string, language statement) の5つのC++のデータ型を
///       使用する.
enum class CppParameterType {
    /// @brief bool (Logical)
    kBool,
    /// @brief int (Integer)
    kInt,
    /// @brief double (Real)
    kDouble,
    /// @brief ObjectID (Pointer)
    kPointer,
    /// @brief std::string (String, Language Statement)
    kString
};

/// @brief IGESのパラメータの型を表す列挙型
/// @note Logical, Integer, Real, Pointer, String, Language Statement
///       の型を持つ
enum class IGESParameterType {
    /// @brief Logical型
    /// @note [0-1]
    kLogical,
    /// @brief Integer型
    /// @note [\+|\-]?[0-9]+
    kInteger,
    /// @brief Real型
    /// @note [\+|\-]?([0-9]+\.([0-9]*)?|\.[0-9]+)([DE][\+|\-]?[0-9]+)?
    kReal,
    /// @brief Pointer型
    /// @note [0-9]+
    kPointer,
    /// @brief String型
    /// @note [0-9]H[文字列]
    kString,
    /// @brief Language Statement型
    /// @note [文字列]
    kLanguageStatement
};



/**
 * 関数宣言
 */

/// @brief IGESのセクションの種類を文字列に変換する
/// @param section_type セクションの種類
/// @return セクションの種類を表す文字列
std::string SectionTypeToString(const SectionType&);

/// @brief CppParameterTypeとIGESParameterTypeが互換性を持つかを確認する
/// @param cpp_type CppParameterType
/// @param iges_type IGESParameterType
/// @return 互換性がある場合はtrue, それ以外はfalse
/// @note kBoolとkLogicalは互換性があるが、kBoolとkIntegerは互換性がない、など
bool IsCompatibleParameterType(const CppParameterType, const IGESParameterType);

/// @brief CppParameterTypeとIGESParameterTypeが互換性を持つかを確認する
/// @param iges_type IGESParameterType
/// @param cpp_type CppParameterType
/// @return 互換性がある場合はtrue, それ以外はfalse
/// @note kBoolとkLogicalは互換性があるが、kBoolとkIntegerは互換性がない、など
bool IsCompatibleParameterType(const IGESParameterType, const CppParameterType);

}  // namespace igesio

#endif  // IGESIO_COMMON_IGES_METADATA_H_
