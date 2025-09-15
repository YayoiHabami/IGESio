/**
 * @file entities/pd.h
 * @brief IGESのパラメータデータセクションを保持する構造体
 * @author Yayoi Habami
 * @date 2025-04-10
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_PD_H_
#define IGESIO_ENTITIES_PD_H_

#include <string>
#include <tuple>
#include <vector>

#include "igesio/common/iges_metadata.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/de/de_field_wrapper.h"



namespace igesio::entities {

/// @brief 文字列ベースのエンティティ（全般）データ
/// @note パラメータデータセクションのデータのみを保持する.
///       各エンティティは当該セクションにおいて、`144,3,1,0,31;`
///       のようなデータを持つが、この1つめを`type`に、2つめ以降を`data`に
///       格納する.
/// @note コメント (パラメータ区切り文字以降の文字列) は格納しない
struct RawEntityPD {
    /// @brief エンティティタイプ
    EntityType type = EntityType::kNull;
    /// @brief DEポインタ
    /// @note Directory Entryセクションにおける、当該エンティティの
    ///       シーケンス番号（L1, d74-80）を示す
    /// @note RawEntityDE.sequence_numberと同じ値を持つ
    /// @note インスタンス作成時に決まっている値を保持する. すなわち、
    ///       IGESファイルから読み込んだ場合にのみ意味を持つ.
    ///       プログラム内での新規作成時など、それ以外の場合は0 (無効値) が設定される
    unsigned int de_pointer;
    /// @brief シーケンス番号
    /// @note パラメータ部における、当該エンティティの1行目のシーケンス番号
    /// @note インスタンス作成時に決まっている値を保持する. すなわち、
    ///       IGESファイルから読み込んだ場合にのみ意味を持つ.
    ///       プログラム内での新規作成時など、それ以外の場合は0 (無効値) が設定される
    unsigned int sequence_number;
    /// @brief エンティティデータ
    /// @note カンマで分割した文字列の各要素を格納する
    std::vector<std::string> data;
    /// @brief dataの各要素の型
    /// @note 分からない場合は設定しないこと (空のままにすること).
    std::vector<IGESParameterType> data_types;



    /// @brief コンストラクタ
    RawEntityPD();

    /// @brief コンストラクタ
    /// @param type エンティティタイプ
    /// @param data エンティティデータ
    /// @note 自作のエンティティを作成する際に使用する
    explicit RawEntityPD(const EntityType, const std::vector<std::string>& = {});

    /// @brief コンストラクタ
    /// @param type エンティティタイプ
    /// @param de_pointer DEポインタ
    /// @param sequence_number シーケンス番号
    /// @param data エンティティデータ
    /// @param data_types dataの各要素の型.
    ///        分からない場合は設定しないこと (空のままにすること).
    /// @note IGESファイルから読み込んだエンティティを作成する際にのみ使用する.
    ///       プログラム内でエンティティを作成する際は、2引数のコンストラクタを使用すること
    RawEntityPD(const EntityType, const unsigned int,
                const unsigned int, const std::vector<std::string>&,
                const std::vector<IGESParameterType>& = {});

    /// @brief コピーコンストラクタ
    /// @param other コピー元のRawEntityPD
    RawEntityPD(const RawEntityPD&);

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のRawEntityPD
    RawEntityPD(RawEntityPD&&) noexcept;

    /// @brief コピー代入演算子
    /// @param other コピー元のRawEntityPD
    /// @return コピー先のRawEntityPD
    RawEntityPD& operator=(const RawEntityPD&);

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のRawEntityPD
    /// @return ムーブ先のRawEntityPD
    RawEntityPD& operator=(RawEntityPD&&) noexcept;
};



/// @brief RawEntityPDを取得する
/// @param lines 1レコード分の文字列を格納したベクタ
/// @param p_delim パラメータ区切り文字
/// @param r_delim レコード区切り文字
/// @return RawEntityPD構造体
/// @throw igesio::LineFormatError 1行の長さが規定値以外の場合
/// @throw igesio::SectionFormatError 圧縮形式のデータセクションの行が指定された場合、
///        シーケンス番号部分が数値に変換できない場合、パラメータセクションでない場合、
///        その他与えられた文字列が自由形式として不正な場合
/// @throw igesio::TypeConversionError エンティティタイプの変換に失敗した場合
RawEntityPD
ToRawEntityPD(const std::vector<std::string>&, const char, const char);

/// @brief パラメータデータセクションにおける、エンティティのパラメータの数を取得する
/// @param type エンティティタイプ
/// @param data エンティティタイプを除いた、パラメータ区切り文字で分割したパラメータ
/// @return パラメータデータセクションにおいては、以下の3種類のパラメータがある.
///         それぞれのパラメータの数を返す（ただし、総和はdata.size()に等しい）
///    1. エンティティで指定されたパラメータ
///    2. それに続く、関連付け/テキストエンティティへのポインタ
///    3. それに続く、プロパティまたは属性テーブルエンティティへのポインタ
std::tuple<std::size_t, std::size_t, std::size_t>
GetEntityParameterCount(const EntityType, const std::vector<std::string>&);

/// @brief エンティティの子要素のDEポインタを取得する
/// @param data エンティティデータ
/// @param dependency エンティティの従属状態.
///    1. kIndependent: 何も返さない
///    2. kPhysicallyDependent: 物理的に従属する子要素のDEポインタを返す
///    3. kLogicallyDependent: 論理的に従属する子要素のDEポインタを返す
///    4. kPhysicallyAndLogicallyDependent: 物理的かつ論理的に従属する子要素のDEポインタを返す
/// @return エンティティの子要素のDEポインタ
std::vector<unsigned int>
GetChildDEPointer(const RawEntityPD&, const SubordinateEntitySwitch);

/// @brief RawEntityPDからIGESParameterVectorを作成する
/// @param pd パラメータデータ
/// @return IGESParameterVector
/// @note 各パラメータは、以下の基準に従って自動的に型変換される
///       (1) `[\+\-]?[0-9]+`: 整数型
///       (2) `[\+\-]?([0-9]+\.([0-9]*)?|\.[0-9]+)?([DE][+-]?[0-9]+)?`: 実数型
///       (3) `[0-9]+H[文字列]`: 文字列型 (`[文字列]`の長さと数字が同じ場合)
///       (4) それ以外: 言語ステートメント型
///       ただし、空文字列 (デフォルト値) の場合は、常に文字列型のデフォルト値として
///       扱う。要素の取得の際は、`IGESParameterVector::access_as<T>(index)`で
///       正しい型で上書きしてから使用すること
IGESParameterVector ToIGESParameterVector(const RawEntityPD&);

/// @brief IGESParameterVectorからRawEntityPDを作成する
/// @param type エンティティタイプ
/// @param id エンティティのID
/// @param vec IGESParameterVector
/// @param id2de IDからDEポインタへのマッピング
/// @return RawEntityPD
/// @note sequence_numberについては、出力時に設定されるため、0を設定する
/// @throw std::out_of_range idまたはvecで指定されるIDがid2deに存在しない場合
RawEntityPD
ToRawEntityPD(const EntityType, const uint64_t,
              const IGESParameterVector&, const id2pointer&);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_PD_H_
