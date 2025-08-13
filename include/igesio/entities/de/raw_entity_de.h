/**
 * @file entities/de/raw_entity_de.h
 * @brief ディレクトリエントリセクションのパラメータを保持する構造体
 * @author Yayoi Habami
 * @date 2025-04-10
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DIRECTORY_ENTRY_PARAM_H_
#define IGESIO_ENTITIES_DIRECTORY_ENTRY_PARAM_H_

#include <array>
#include <optional>
#include <string>
#include <utility>

#include "igesio/entities/entity_type.h"



namespace igesio::entities {

/// @brief エンティティの従属状態
/// @note (0)独立、(1)物理的に従属、(2)論理的に従属、
///       (3)物理的かつ論理的に従属、の4種類
enum class SubordinateEntitySwitch {
    /// @brief 独立
    /// @note エンティティはファイル内の他のエンティティによって参照
    ///       (すなわちポイント) されていない. このとき、エンティティは
    ///       単独で存在できる.
    kIndependent = 0,
    /// @brief 物理的に従属
    /// @note このエンティティ(子)は、ファイル内の他のエンティティ(親)に
    ///       よって参照される. 子は親が存在しない限り存在できず、親の
    ///       変換(行列)は子にも適用される.
    /// @note AがBに従属するのは、BのパラメータデータエントリにおいてAが
    ///       参照されている場合に限られる. したがって、View (410)や
    ///       AssociativityInstance (402)による参照 (間接的な参照)
    ///       は従属とはみなされない.
    kPhysicallyDependent = 1,
    /// @brief 論理的に従属
    /// @note このエンティティ(子)は、ファイル内のAssociativityInstance (402)
    ///       などのグループ化エンティティ(親)により参照されるが、
    ///       単独で存在できる. 親の変換(行列)は子には適用されない.
    kLogicallyDependent = 2,
    /// @brief 物理的かつ論理的に従属
    /// @note kPhysicallyDependentとkLogicallyDependentを参照のこと.
    kPhysicallyAndLogicallyDependent = 3
};

/// @brief エンティティ用途フラグ
/// @note (0)ジオメトリ、(1)注釈、(2)定義、(3)その他、
///       (4)論理/位置、(5)2Dパラメトリック、(6)構造ジオメトリ
///       の7種類
enum class EntityUseFlag {
    /// @brief ジオメトリ
    /// @note エンティティは製品の構造のジオメトリを定義するために使用される
    kGeometry = 0,
    /// @brief 注釈
    /// @note エンティティはファイルに注釈や説明を追加するために使用される.
    ///       注釈や説明を形成するために使用される幾何学的エンティティも含まれる
    kAnnotation = 1,
    /// @brief 定義
    /// @note エンティティはファイルの定義構造で使用される. このクラスには、
    ///       300番台の全てのエンティティが含まれる.
    kDefinition = 2,
    /// @brief その他
    /// @note エンティティはファイル内の構造的特徴を定義するなど、他の目的で
    ///       使用される。ほとんどの場合は400番台のエンティティに該当するが、
    ///       いくつかの例外も存在する.
    kOther = 3,
    /// @brief 論理/位置
    /// @note エンティティは他のエンティティによって、論理的/位置的参照として使用
    ///       される. このように使用される可能性のあるエンティティには、主な用途が
    ///       参照として使用される場合のNode(134), ConnectPoint(132), Point(116)
    ///       などがある.
    /// @note 論理的コネクタとして使用される、2つの接続点のみで構成される複合曲線は、
    ///       そのエンティティ用途フラグを04に設定する。
    kLogicalPosition = 4,
    /// @brief 2Dパラメトリック
    /// @note このエンティティは二次元XYパラメータ空間に位置し、Z座標を
    ///       無視することにより三次元XYZ空間のサブセットとみなされる。定義空間から
    ///       パラメータ空間への変換行列は二次元でなければならない（つまり、
    ///       TransformationMatrix(124)においてT3=R13=R31=R32=R23=0.0かつR33=1.0）
    /// @note 加えて、座標は長さの単位を持たない. これは面上の曲線を定義するための
    ///       使用を意図している
    k2DParametric = 5,
    /// @brief 構造ジオメトリ
    /// @note エンティティはモデルや描画の準備における便宜のためにのみ使用され、
    ///       製品の構造のジオメトリを定義するためには使用されない。例として、
    ///       長方形の中心を見つけるために交差する2本の線がある
    /// @note 親がこのフラグを持つ場合、全ての子もこのフラグを持つ必要がある.
    ///       ただし、子がkDefinition(2)を持つ場合は除く.
    kStructuralGeometry = 6
};

/// @brief 階層の種類
/// @note (0)Global top down、(1)Global defer、(2)Use hierarchy property
///       の3種類
/// @note この値は階層構造内でのエンティティ間の関係を示す. どのエンティティの
///       ディレクトリエントリ属性 (以下) の制御が有効となるかを決定する:
///       > line font, view, entity level, blank status(表示状態), line weight,
///         color number;
enum class HierarchyType {
    /// @brief Global top down
    /// @note すべてのディレクトリエントリ属性は、このエンティティに物理的に従属する
    ///       エンティティに適用される
    kGlobalTopDown = 0,
    /// @brief Global defer
    /// @note いずれのディレクトリエントリ属性も、物理的に従属するエンティティには適用
    ///       されない. 物理的に従属するエンティティは、自身のディレクトリエントリ属性を
    ///       使用する
    kGlobalDefer = 1,
    /// @brief Use hierarchy property
    /// @note 各ディレクトリエントリ属性の個別設定が許可される. Property
    ///       (タイプ406、フォーム10) エンティティが、各ディレクトリエントリ属性が、
    ///       従属するエンティティのディレクトリエントリ属性を継承するかを決定する
    kUseHierarchyProperty = 2
};

/// @brief ディレクトリ部の各エンティティのステータス番号 (8桁) を表す構造体
struct EntityStatus {
    /// @brief 表示状態 (blank status; 1-2桁目)
    /// @note 00: 表示 (-> true); 01: 非表示 (-> false)
    bool blank_status;
    /// @brief 従属エンティティスイッチ (3-4桁目)
    /// @note 00: 独立; 01: 物理的に従属; 02: 論理的に従属; 03: (01)と(02)の両方
    SubordinateEntitySwitch subordinate_entity_switch;
    /// @brief エンティティ用途フラグ (5-6桁目)
    /// @note 00: ジオメトリ; 01: 注釈; 02: 定義; 03: その他;
    ///       04: 論理/位置; 05: 2Dパラメトリック; 06: 構造ジオメトリ
    EntityUseFlag entity_use_flag;
    /// @brief 階層 (7-8桁目)
    /// @note 00: Global top down; 01: Global defer; 02: Use hierarchy property
    HierarchyType hierarchy;

    /// @brief デフォルトコンストラクタ
    EntityStatus() : blank_status(true),
                     subordinate_entity_switch(SubordinateEntitySwitch::kIndependent),
                     entity_use_flag(EntityUseFlag::kGeometry),
                     hierarchy(HierarchyType::kGlobalTopDown) {}

    /// @brief コンストラクタ
    /// @param status ステータス番号 (8桁)
    /// @throw igesio::ParseError ステータス番号が無効な場合
    explicit EntityStatus(const std::string&);
};



/// @brief 構造 (ディレクトリエントリ L1, d17-24) のデフォルト値
constexpr int kDefaultStructure = 0;
/// @brief 線のフォントパターン (ディレクトリエントリ L1, d25-32) のデフォルト値
constexpr int kDefaultLineFontPattern = 0;
/// @brief エンティティレベル (ディレクトリエントリ L1, d33-40) のデフォルト値
constexpr int kDefaultLevel = 0;
/// @brief ビュー (ディレクトリエントリ L1, d41-48) のデフォルト値
constexpr int kDefaultView = 0;
/// @brief 変換行列 (ディレクトリエントリ L1, d49-56) のデフォルト値
constexpr int kDefaultTransformationMatrix = 0;
/// @brief ラベル表示関連付け (ディレクトリエントリ L1, d57-64) のデフォルト値
constexpr int kDefaultLabelDisplayAssociativity = 0;
/// @brief 色番号 (ディレクトリエントリ L2, d17-24) のデフォルト値
constexpr int kDefaultColorNumber = 0;
/// @brief フォーム番号 (ディレクトリエントリ L2, d33-40) のデフォルト値
constexpr int kDefaultFormNumber = 0;
/// @brief エンティティラベル (ディレクトリエントリ L2, d57-64) のデフォルト値
/// @note 本来はNULLだが、ここでは空文字列を指定する
constexpr std::string_view kDefaultEntityLabel = "";
/// @brief エンティティ添字番号 (ディレクトリエントリ L2, d65-72) のデフォルト値
constexpr int kDefaultEntitySubscriptNumber = 0;

/// @brief ディレクトリ部のパラメータを表す構造体
/// @note "L1, d1-8"は、そのデータが1行目の1-8桁目に格納されていることを示す
struct RawEntityDE {
    /// @brief エンティティタイプ (L1, d1-8)
    EntityType entity_type = EntityType::kNull;
    /// @brief パラメータデータへのポインタ (L1, d9-16)
    /// @note このエンティティの最初のパラメータデータレコードのシーケンス番号
    /// @note (ポインタ); RawEntityData.sequence_numberと同じ値を持つ
    unsigned int parameter_data_pointer;
    /// @brief 構造 (L1, d17-24)
    /// @note MacroInstance (306?), AssociativityInstance (402),
    ///       AttributeTableInstance (422)において有効.
    /// @note (数値orポインタ); 正値は無視される. 負の値の場合、その絶対値は
    ///       このエンティティのスキーマを指定する構造定義エンティティを参照する
    ///       DEへのポインタを示す
    int structure = kDefaultStructure;
    /// @brief 線のフォントパターン (L1, d25-32)
    /// @note (数値orポインタ); 正の値の場合、受信側システムにおいて対応するフォントが使用される.
    ///       0: default, 1: solid, 2: dashed, 3: phantom, 4: centerline, 5: dotted.
    ///       負の値の場合、その絶対値は線種定義エンティティ(304)を参照するDEへのポインタを示す.
    int line_font_pattern = kDefaultLineFontPattern;
    /// @brief エンティティレベル (L1, d33-40)
    /// @note (数値orポインタ); 正の値の場合、エンティティが存在するレベルの番号を示す.
    ///       負値の場合、その絶対値は定義レベルプロパティエンティティ(406, form1)
    ///       を参照するDEへのポインタを示す.
    int level = kDefaultLevel;
    /// @brief ビュー (L1, d41-48)
    /// @note option 1: エンティティが全てのビューに表示され、その表示特性が
    ///       全てのビューで同じである場合（値は0）
    /// @note option 2: エンティティが一つのビューにのみ表示される場合、
    ///       値はView (410) エンティティを参照する
    /// @note option 3: それ以外の場合、値はAssociativityInstance (402)のフォーム
    ///       3, 4, または19を参照する
    int view = kDefaultView;
    /// @brief 変換行列 (L1, d49-56)
    /// @note (0orポインタ); TransformationMatrix (124)エンティティを参照するか、
    ///       0（デフォルト）を取る. 0の場合、回転行列が単位行列、並進ベクトルがゼロを取る.
    int transformation_matrix = kDefaultTransformationMatrix;
    /// @brief ラベル表示関連付け (L1, d57-64)
    /// @note (0orポインタ); この値はAssociativityInstance（タイプ402、フォーム5）を参照し、
    ///       エンティティのラベルとサブスクリプトが異なるビューでどのように表示されるかを定義する.
    ///       または、0（デフォルト）を取る
    int label_display_associativity = kDefaultLabelDisplayAssociativity;
    /// @brief ステータス (L1, d65-72)
    EntityStatus status;
    /// @brief シーケンス番号 (L1, d74-80)
    /// @note パラメータ部でDEポインター（11P等）が参照する、エンティティの
    ///       ディレクトリ部における行番号
    unsigned int sequence_number;
    /// @brief 線の太さ番号 (L2, d9-16)
    /// @note この値はエンティティの表示に使用する太さ(または幅)を指定する. グローバルパラメータ
    ///       (GlobalParam; 以下GP) と併せて、表示の太さは次式のように決定される:
    ///       `line_weight_number * GP.max_line_weight / GP.line_weight_gradations`
    int line_weight_number;
    /// @brief 色番号 (L2, d17-24)
    /// @note エンティティの表示色を指定する. 非負の色番号は以下のように解釈される:
    ///       > 0: 色割り当てなし(デフォルト), 1: 黒, 2: 赤, 3: 緑, 4: 青,
    ///         5: 青, 6: 黄, 7: マゼンタ, 8: シアン, 9: 白
    /// @note 色番号が負の場合、その絶対値はColorDefinition (314)エンティティの
    ///       ポインタを示し、エンティティはその色を使用する.
    int color_number = kDefaultColorNumber;
    /// @brief パラメータ行数 (L2, d25-32)
    /// @note パラメータデータセクションにおける、レコードの行数を示す. レコード区切り文字を
    ///       含む行の後に続くコメント行も含む
    /// @note Null (0) エンティティを除き、ゼロよりも大きい値を取る. Nullエンティティのみ
    ///       0も指定することが可能
    int parameter_line_count;
    /// @brief フォーム番号 (L2, d33-40)
    /// @note いくつかのエンティティは、フォーム番号により異なるパラメータを持つ. この指定に
    ///       利用する. 複数のフォームを持たないエンティティ、およびデフォルトではゼロである
    int form_number = kDefaultFormNumber;
    /// @brief エンティティラベル (L2, d57-64)
    /// @note エンティティのアプリケーション指定の英数字識別子/名前であり、
    ///       エンティティ添字番号と組み合わせて、(アプリケーション指定の)エンティティ識別子
    ///       を形成する.
    std::string entity_label = std::string(kDefaultEntityLabel);
    /// @brief エンティティ添字番号 (L2, d65-72)
    /// @note エンティティラベルの数値修飾子
    int entity_subscript_number = kDefaultEntitySubscriptNumber;

    /// @brief is_default_の取得
    /// @return 各パラメータの値がデフォルト値 (未指定; 空白のみ) であったか.
    /// @note パラメータ {3, 4, 5, 6, 7, 8, 12, 13, 15, 18}. それ以外は
    ///       値が必ず指定される
    const std::array<bool, 10>& IsDefault() const { return is_default_; }

    /// @brief is_default_の設定
    /// @param index パラメータの番号 (3-8, 12-13, 15, 18)
    /// @param value パラメータの値がデフォルト値 (未指定; 空白のみ) であったか
    /// @note パラメータ {3, 4, 5, 6, 7, 8, 12, 13, 15, 18}. それ以外は
    ///       値が必ず指定される
    /// @return true: 成功, false: 失敗 (無効なindex)
    bool SetIsDefault(const size_t, const bool);

    /// @brief デフォルト値で初期化されたインスタンスを作成する
    /// @param entity_type エンティティタイプ
    /// @param form_number フォーム番号
    /// @return そのエンティティタイプとフォーム番号に対応する
    ///         デフォルト値で初期化されたRawEntityDEのインスタンス
    /// @throw igesio::DataFormatError 0以外の、無効なフォーム番号が指定された場合;
    ///        entity_typeがform_number (!= 0) のフォーム番号を持たない場合にはこの例外を投げる.
    ///        0のフォーム番号を持たない場合、可能な最小のフォーム番号を指定する
    /// @throw igesio::DataFormatError entity_typeが無効な場合
    /// @note Node (Type 134) のDEパラメータ7 (Transformation Matrix)、
    ///       Lep Drilled Hole Property (Type 406, Form 26) のDEパラメータ5 (Level)、
    ///       Attribute Table Instance (Type 422) のDEパラメータ3 (Structure) は、
    ///       ポインター (0未満or0より大きい値) のみを指定可能である。しかし、デフォルト値で
    ///       初期化する際にそれらのポインターを知ることはできないため、仮の値として0を設定する。
    ///       そのため、これらタイプに対する戻り値を修正せずに`IsValid`に与えた場合、
    ///       `igesio::DataFormatError`例外が投げられることに注意する。
    static RawEntityDE ByDefault(const EntityType, const int = 0);

 private:
    /// @brief インスタンス作成時 (基本的に文字列からの変換時) に、
    ///        各パラメータの値がデフォルト値 (未指定; 空白のみ) であったか
    /// @note パラメータ {3, 4, 5, 6, 7, 8, 12, 13, 15, 18}. それ以外は
    ///       値が必ず指定される
    /// @note For the functional requirements for editors and analyzers (Section 1.4.7.1)
    std::array<bool, 10> is_default_ =
            {false, false, false, false, false, false, false, false, false, false};
};



/**
 * 列挙体への変換
 */

/// @brief 数値を表示状態に変換する
/// @param n 表示状態番号 (0-1)
/// @return 表示状態
/// @throw igesio::ParseError ステータス番号が無効な場合
bool ToBlankStatus(const int);

/// @brief 2桁の文字列から表示状態に変換する
/// @param status 2桁の文字列 (00または01)
/// @return 表示状態 (true: 表示, false: 非表示)
/// @throw igesio::ParseError ステータス番号が無効な場合
bool ToBlankStatus(const std::string&);

/// @brief 数値を従属エンティティスイッチに変換する
/// @param n 従属エンティティスイッチ番号 (0-3)
/// @return 従属エンティティスイッチ
/// @throw igesio::ParseError ステータス番号が無効な場合
SubordinateEntitySwitch ToSubordinateEntitySwitch(const int);

/// @brief 2桁の文字列から従属エンティティスイッチに変換する
/// @param status 2桁の文字列 (00-03)
/// @return 従属エンティティスイッチ
/// @throw igesio::ParseError ステータス番号が無効な場合
SubordinateEntitySwitch ToSubordinateEntitySwitch(const std::string&);

/// @brief 数値をエンティティ用途フラグに変換する
/// @param n エンティティ用途フラグ番号
/// @return エンティティ用途フラグ
/// @throw igesio::ParseError ステータス番号が無効な場合
EntityUseFlag ToEntityUseFlag(const int);

/// @brief 2桁の文字列からエンティティ用途フラグに変換する
/// @param status 2桁の文字列 (00-06)
/// @return エンティティ用途フラグ
/// @throw igesio::ParseError ステータス番号が無効な場合
EntityUseFlag ToEntityUseFlag(const std::string&);

/// @brief 数値を階層の種類に変換する
/// @param n 階層の種類番号 (0-2)
/// @return 階層の種類
/// @throw igesio::ParseError ステータス番号が無効な場合
HierarchyType ToHierarchyType(const int);

/// @brief 2桁の文字列から階層の種類に変換する
/// @param status 2桁の文字列 (00-02)
/// @return 階層の種類
/// @throw igesio::ParseError ステータス番号が無効な場合
HierarchyType ToHierarchyType(const std::string&);



/**
 * RawEntityDEの変換
 */

/// @brief ディレクトリエントリセクションのレコードをRawEntityDEに変換する
/// @param first ディレクトリエントリセクションの1行目 (80文字)
/// @param second ディレクトリエントリセクションの2行目 (80文字)
/// @return ディレクトリエントリセクションのパラメータ
/// @throw igesio::TypeConversionError IGES文字列の型変換エラー
/// @throw igesio::SectionFormatError 必要なパラメータが不足している場合
/// @throw igesio::DataFormatError いずれかのパラメータの値が仕様に合致しない場合
RawEntityDE ToRawEntityDE(const std::string&, const std::string&);



/**
 * 文字列への変換
 */

/// @brief EntityStatusを文字列に変換する
/// @param status EntityStatus
/// @return ステータス番号 (8桁の文字列)
std::string ToString(const EntityStatus&);

/// @brief RawEntityDEを文字列に変換する
/// @param param RawEntityDE
/// @param pd_pointer Parameter Data (パラメータ2).
///        負の値を指定した場合、パラメータデータポインタ部分をxxxで出力する
/// @param sequence_number Sequence Number (パラメータ10).
///        負の値を指定した場合、シーケンス番号部分をxxxとして出力する
/// @param line_count Parameter Line Count (パラメータ14).
///        負の値を指定した場合、パラメータ行数部分をxxxとして出力する
/// @return DEセクションの文字列表現 (2行分、改行コードを2つ含む)
std::string
ToString(const RawEntityDE&, const int = -1, const int = -1, const int = -1);

/// @brief RawEntityDEを文字列に変換する
/// @param param RawEntityDE
/// @param pd_pointer Parameter Data (パラメータ2).
///        負の値を指定した場合、パラメータデータポインタ部分をxxxで出力する
/// @param sequence_number Sequence Number (パラメータ10).
///        負の値を指定した場合、シーケンス番号部分をxxxとして出力する
/// @param line_count Parameter Line Count (パラメータ14).
///        負の値を指定した場合、パラメータ行数部分をxxxとして出力する
/// @return DEセクションの文字列表現 (2行)
std::pair<std::string, std::string>
ToStrings(const RawEntityDE&, const int = -1, const int = -1, const int = -1);



/**
 * 検証用関数
 */

/// @brief RawEntityDEの値が有効かどうかを検証する
/// @param de 検証するRawEntityDE
/// @note エンティティタイプとフォーム番号に対して、各パラメータが適切に設定されているか
///       を検証する. ただし、DEパラメータ2 (Parameter Data)、DEパラメータ10,20
///       (Sequence Number) 、およびDEパラメータ16,17 (Reserved) は検証しない.
///       前者2種類は他のセクション/レコードの情報が必要なためであり、後者は情報がないため.
/// @throw iio::DataFormatError エンティティタイプとFormが指定する、パラメータの条件に
///        paramのパラメータが合致しない場合
/// @note 仕様に厳密には従っていないIGESファイルも存在するため、実行する際は注意が必要.
void IsValid(const RawEntityDE&);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_DIRECTORY_ENTRY_PARAM_H_
