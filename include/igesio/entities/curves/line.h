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

    /// @brief 定義空間における曲線のn階導関数 C^n(t) を計算する
    /// @param t パラメータ値
    /// @param n 何階まで計算するか; 例えば2を指定した場合、0階 C(t) から2階 C''(t) まで計算
    /// @return 導関数 C'(t), C''(t)、計算できない場合は`std::nullopt`
    std::optional<CurveDerivatives>
    TryGetDerivatives(const double, const unsigned int) const override;

    /// @brief 曲線の全長を取得する
    /// @return 曲線の全長
    double Length() const override;

    /// @brief 曲線の [t_start, t_end] 間の長さを取得する
    /// @param start パラメータ範囲の開始値
    /// @param end パラメータ範囲の終了値
    /// @return 曲線の t ∈ [start, end] 間の長さ
    /// @throw std::invalid_argument start >= endの場合、
    ///        startまたはendがパラメータ範囲外の場合
    double Length(const double, const double) const override;



    /**
     * 描画用
     */

    /// @brief 線分/半直線/直線の始点・終点を取得する
    /// @return この線を定義する始点P1と終点P2の座標値 (x, y, z).
    ///         半直線や直線の場合は、定義空間において線が通る点
    ///         P1, P2の座標値を返す. firstがP1、secondがP2
    std::pair<const Vector3d&, const Vector3d&> GetAnchorPoints() const;



 protected:
    /// @brief エンティティ自身が参照する変換行列に従い、座標orベクトルを変換する
    /// @param input 変換前の座標orベクトル v
    /// @param is_point 座標を変換する場合は`true`、ベクトルを変換する場合は`false`
    /// @return 変換後の座標orベクトル. 回転行列 R、平行移動ベクトル T に対し、
    ///         座標値の場合は v' = Rv + T、ベクトルの場合は v' = Rv
    /// @note inputがstd::nulloptの場合はそのまま返す
    ///       としてオーバライドすること
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input, const bool is_point) const override {
        return TransformImpl(input, is_point);
    }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_LINE_H_
