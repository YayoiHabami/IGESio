/**
 * @file entities/curves/line.h
 * @brief Line (Type 110): 線分/半直線/直線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-10
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_CURVES_LINE_H_
#define IGESIO_ENTITIES_CURVES_LINE_H_

#include <utility>

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief 線分/半直線/直線のタイプ
enum class LineType {
    /// @brief 線分 (デフォルト; form number 0)
    /// @note 始点 (P1) と終点 (P2) の2つの端点を持つ
    ///       (t = 0 で始点、t = 1 で終点の位置)
    kSegment = 0,
    /// @brief 半直線 (semi-bounded line; form number 1)
    /// @note 始点 (P1) を端点とし、終点 (P2) を通過点とする
    ///       (t = 0 で始点、t = 1 で P2 の位置、t = ∞ で P2 の延長線上)
    kRay = 1,
    /// @brief 直線 (unbounded line; form number 2)
    /// @note 始点 (P1) と終点 (P2) を通る直線
    ///       (t = 0 で始点、t = 1 で P2 の位置、t = ±∞ で P1 と P2 の延長線上)
    kLine = 2
};

/// @brief 線分/半直線/直線を表すエンティティクラス
class Line : public EntityBase, public virtual ICurve3D {
 protected:
    /// @brief 始点 (P1)
    Vector3d start_point_;
    /// @brief 終点または通過点 (P2)
    Vector3d terminate_point_;

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
    Line(const RawEntityDE&, const IGESParameterVector&,
         const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param start_point 始点 P1 の座標値 (x, y, z)
    /// @param terminate_point 終点または通過点 P2 の座標値 (x, y, z)
    /// @param line_type 線のタイプ (Segment, Ray, Line)
    Line(const Vector3d&, const Vector3d&, const LineType = LineType::kSegment);

    /// @brief 線の種類を取得する
    /// @return 線の種類 (Segment, Ray, Line)
    LineType GetLineType() const;



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;



    /**
     * ICurve implementation
     */

    /// @brief 曲線のパラメータ範囲を取得する
    /// @return `{t_start, t_end}`の形式のパラメータ範囲 [rad]
    std::array<double, 2> GetParameterRange() const override;

    /// @brief 曲線が閉じているかどうか
    /// @return 始点と終点が一致する場合は`true`、そうでない場合は`false`
    bool IsClosed() const override;

    /// @brief 定義空間における曲線上の点 C(t) を取得する
    /// @param t パラメータ値 (角度)
    /// @return 曲線上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetDefinedPointAt(const double) const override;

    /// @brief 定義空間における曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty, 0).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetDefinedTangentAt(const double) const override;

    /// @brief 定義空間における曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, 0).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    /// @note 直線においては、本来法線ベクトルは一意に定義されないが、
    ///       ここでは接線を90度回転させた、定義空間においてz=0のベクトルを法線として返す
    std::optional<Vector3d> TryGetDefinedNormalAt(const double) const override;

    /// @brief 曲線上の点 C(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetPointAt(const double) const override;

    /// @brief 曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty, tz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetTangentAt(const double) const override;

    /// @brief 曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    /// @note 直線においては、本来法線ベクトルは一意に定義されないが、
    ///       ここでは接線を90度回転させた、定義空間においてz=0のベクトルを法線として返す
    std::optional<Vector3d> TryGetNormalAt(const double) const override;



    /**
     * 描画用
     */

    /// @brief 線分/半直線/直線の始点・終点を取得する
    /// @return この線を定義する始点P1と終点P2の座標値 (x, y, z).
    ///         半直線や直線の場合は、定義空間において線が通る点
    ///         P1, P2の座標値を返す. firstがP1、secondがP2
    std::pair<const Vector3d&, const Vector3d&> GetAnchorPoints() const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_LINE_H_
