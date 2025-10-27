/**
 * @file entities/curves/copious_data.h
 * @brief Copious Data (Type 106, forms 1-3) クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 * @note Copious Data (Forms 1-3) のエンティティクラス.
 */
#ifndef IGESIO_ENTITIES_CURVES_COPIOUS_DATA_H_
#define IGESIO_ENTITIES_CURVES_COPIOUS_DATA_H_

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/curves/copious_data_base.h"



namespace igesio::entities {

/// @brief Copious Data (Type 106, forms 1-3) クラス
/// @note 頂点の数は`GetCount()`で取得できる. 各頂点の座標値は
///       `Coordinates().col(i)`で取得できる (iは0から始まるインデックス).
class CopiousData : public CopiousDataBase,
                    public virtual ICurve3D {
 public:
    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    /// @return 全パラメータが適合しているか否か
    ValidationResult ValidatePD() const override;

    using CopiousDataBase::CopiousDataBase;

    /**
     * ICurve implementation
     */


    /// @brief 曲線が閉じているかどうか
    /// @return 常にfalse
    bool IsClosed() const override;

    /// @brief 曲線のパラメータ範囲を取得する
    /// @return `{t_start, t_end}`の形式でパラメータ範囲を返す
    ///         半直線の場合は`{0, std::numeric_limits<double>::infinity()}`のように返す
    std::array<double, 2> GetParameterRange() const override;

    /// @brief 定義空間における曲線のn階導関数 C^n(t) を計算する
    /// @param t パラメータ値
    /// @param n 何階まで計算するか; 例えば2を指定した場合、0階 C(t) から2階 C''(t) まで計算
    /// @return 1階以上については常にゼロベクトルを返す
    std::optional<CurveDerivatives>
    TryGetDerivatives(const double, const unsigned int) const override;

    /// @brief 曲線の全長を取得する
    /// @return 曲線の全長
    double Length() const override { return 0; };

    /// @brief 曲線の [t_start, t_end] 間の長さを取得する
    /// @param start パラメータ範囲の開始値
    /// @param end パラメータ範囲の終了値
    /// @return 曲線の t ∈ [start, end] 間の長さ
    /// @throw std::invalid_argument start >= endの場合、
    ///        startまたはendがパラメータ範囲外の場合
    /// @note 常にゼロ（連続な曲線ではなく、離散な点列であるため）
    double Length(const double start, const double end) const override { return 0; }



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

#endif  // IGESIO_ENTITIES_CURVES_COPIOUS_DATA_H_
