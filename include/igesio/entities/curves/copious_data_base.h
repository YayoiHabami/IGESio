/**
 * @file entities/curves/copious_data_base.h
 * @brief Copious Data (Type 106) の基底クラス
 * @author Yayoi Habami
 * @date 2025-08-19
 * @copyright 2025 Yayoi Habami
 * @note Copious Data (Forms 1-3), Linear Path (Forms 11-13),
 *       Centerline (Forms 20-21), Section (Forms 31-38),
 *       Witness Line (Form 40), Simple Closed Planar Curve (Form 63)
 *       の基底クラス. Copious DataとLinear Path、Simple Closed Planar Curve
 *       を除き、CurveではなくAnnotationエンティティではあるが、
 *       メインは曲線の方なので`entities/curves`に定義する
 */
#ifndef IGESIO_ENTITIES_CURVES_COPIOUS_DATA_BASE_H_
#define IGESIO_ENTITIES_CURVES_COPIOUS_DATA_BASE_H_

#include <utility>

#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief Copious Data (Type 106) のフォーム番号に対応する
///        列挙体
/// @note IP=1 の場合、2つの値を一組とする形式の、
///       IP=2 の場合、3つの値を一組とする形式の、
///       IP=3 の場合、6つの値を一組とする形式の座標値を持つ
/// @note Curve and Surfaceエンティティは以下の7種類:
///       - PlanarPoints, Points3D, Sextuples (1-3)
///       - PlanarPolyline, Polyline3D, PolylineAndVectors (11-13)
///       - PlanarLoop (63)
/// @note Annotationエンティティは以下の11種類:
///       - CenterlineByPoints (20)
///       - CenterlineByCircles (21)
///       - GeneralHatch (31) ~ AluminumHatch (38)
///       - WitnessLine (40)
enum class CopiousDataType {
    /// @brief 平面 Z_T = const 上に存在する座標ペア (IP=1)
    kPlanarPoints = 1,
    /// @brief 3次元空間の座標3つ組 (IP=2)
    kPoints3D = 2,
    /// @brief 座標6つ組 (IP=3)
    kSextuples = 3,
    /// @brief 平面 Z_T = const 上に存在する、平面上の折れ線の頂点 (IP=1)
    kPlanarPolyline = 11,
    /// @brief 3次元空間の折れ線 (IP=2)
    kPolyline3D = 12,
    /// @brief 折れ線の頂点とそれに関連するベクトル (3次元+3次元) (IP=3)
    kPolylineAndVectors = 13,
    /// @brief 点群を通る中心線 (IP=1)
    kCenterlineByPoints = 20,
    /// @brief 円の中心点群を通る中心線 (IP=1)
    kCenterlineByCircles = 21,
    /// @brief 断面ハッチング (フォーム31, IP=1):
    ///        鋳鉄または可鍛鋳鉄およびすべての材料の一般用途
    kGeneralHatch = 31,
    /// @brief 断面ハッチング (フォーム32, IP=1): 鋼
    kSteelHatch = 32,
    /// @brief 断面ハッチング (フォーム33, IP=1):
    ///        青銅、真鍮、銅、および組成物
    kBronzeHatch = 33,
    /// @brief 断面ハッチング (フォーム34, IP=1):
    ///        ゴム・プラスチックおよび電気絶縁
    kRubberHatch = 34,
    /// @brief 断面ハッチング (フォーム35, IP=1):
    ///        チタンおよび耐火物
    kTitaniumHatch = 35,
    /// @brief 断面ハッチング (フォーム36, IP=1):
    ///        大理石、スレート、ガラス、磁器
    kMarbleHatch = 36,
    /// @brief 断面ハッチング (フォーム37, IP=1):
    ///        ホワイトメタル、亜鉛、鉛、バビット、および合金
    kZincHatch = 37,
    /// @brief 断面ハッチング (フォーム38, IP=1):
    ///        マグネシウム、アルミニウム、およびアルミニウム合金
    kAluminumHatch = 38,
    /// @brief 証拠線 (IP=1)
    kWitnessLine = 40,
    /// @brief 単純な閉じた平面曲線 (IP=1)
    kPlanarLoop = 63
};

/// @brief CopiousDataTypeに対応するIPの値を取得する
/// @param type CopiousDataType
/// @return IPの値 (1, 2, 3のいずれか)
/// @note IP=1 の場合、2つの値を一組とする形式の、
///       IP=2 の場合、3つの値を一組とする形式の、
///       IP=3 の場合、6つの値を一組とする形式の座標値を持つ
/// @throw igesio::DataFormatError typeが無効な場合
int GetIP(const CopiousDataType);



/// @brief Copious Data (Type 106) の基底クラス
class CopiousDataBase : public EntityBase {
 protected:
    /// @brief 座標値データ (IP=3の場合の、後半の3つ組を除く)
    Matrix3Xd coordinates_;
    /// @brief 座標値データ (IP=3の場合の、後半の3つ組)
    Matrix3Xd addition_;

    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersの数が7でない場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters([[maybe_unused]] const pointer2ID&) override;


 public:
    /// @brief コンストラクタ
    /// @param de_record DEレコードのパラメータ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    CopiousDataBase(const RawEntityDE&, const IGESParameterVector&,
                    const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param type CopiousDataTypeの値
    /// @param coordinates 座標値データ (IP=3の場合の、後半の3つ組を除く)
    /// @param addition 座標値データ (IP=3の場合のみ指定する)
    /// @throw igesio::DataFormatError typeが無効な場合、
    ///        与えられた座標値が2組未満の場合、
    ///        IPが1または2のときに、additionが指定された場合、
    ///        IPが3のときに、coordinatesとadditionの列数が異なる場合
    CopiousDataBase(const CopiousDataType, const Matrix3Xd&,
                    const Matrix3Xd& = Matrix3Xd::Zero(3, 1));

    /// @brief デストラクタ
    ~CopiousDataBase() override = default;

    /// @brief データの種類を取得する
    /// @return CopiousDataTypeの値
    CopiousDataType GetDataType() const;

    /// @brief IP (データの種類: 1=座標2つ組, 2=座標3つ組, 3=座標6つ組)
    /// @return IPの値
    /// @throw igesio::DataFormatError 無効なタイプが指定されている場合
    int GetIP() const;



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    /// @return 全パラメータが適合しているか否か
    ValidationResult ValidatePD() const override;



    /**
     * 描画用
     */

    /// @brief i番目の座標値を取得する
    /// @param i 座標値のインデックス
    /// @return 座標値のベクトル
    /// @throw std::out_of_range インデックスが範囲外の場合
    Vector3d GetCoordinate(const size_t) const;

    /// @brief i番目の追加座標値を取得する (IP=3の場合のみ)
    /// @param i 追加座標値のインデックス
    /// @return 追加座標値のベクトル
    /// @throw std::out_of_range インデックスが範囲外の場合
    Vector3d GetAddition(const size_t) const;

    /// @brief 座標値の数を取得する
    /// @return 座標値の数
    size_t GetCount() const;

    /// @brief 全座標値を取得する (const参照)
    /// @return 座標値の行列
    const Matrix3Xd& Coordinates() const {
        return coordinates_;
    }
    /// @brief 全追加座標値を取得する (const参照, IP=3の場合のみ)
    /// @return 追加座標値の行列
    const Matrix3Xd& Addition() const {
        return addition_;
    }

    /// @brief 全長を計算する
    /// @note 1本の折れ線として計算する (IP=3の場合、座標6つ組の前半3つ組のみ使用)
    double TotalLength() const;

    /// @brief 指定した長さに対応する座標値を取得する
    /// @param length 全長に対する位置 (0.0～TotalLength()の範囲)
    /// @return 指定した長さに対応する座標値、範囲外の場合はstd::nullopt
    /// @note 2つの点の間に位置する場合、線形補間で計算する
    std::optional<Vector3d> GetCoordinateAtLength(const double) const;

    /// @brief 指定した長さに最も近い頂点のインデックスを取得する
    /// @param length 全長に対する位置 (0.0～TotalLength()の範囲)
    /// @return 指定した長さに最も近い頂点のインデックスと、その点への距離.
    ///         範囲外の場合は距離をinfinityとする
    std::pair<size_t, double> GetNearestVertexAt(const double) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_COPIOUS_DATA_BASE_H_
