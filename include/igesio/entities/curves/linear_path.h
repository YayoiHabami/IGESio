/**
 * @file entities/curves/linear_path.h
 * @brief Linear Path (Type 106, forms 11-13) クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 * @note Linear Path (Forms 11-13) のエンティティクラス.
 */
#ifndef IGESIO_ENTITIES_CURVES_LINEAR_PATH_H_
#define IGESIO_ENTITIES_CURVES_LINEAR_PATH_H_

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/curves/copious_data_base.h"



namespace igesio::entities {

/// @brief Linear Path (Type 106, forms 11-13) クラス
/// @note 頂点の数は`GetCount()`で取得できる. 各頂点の座標値は
///       `Coordinates().col(i)`で取得できる (iは0から始まるインデックス).
class LinearPath : public CopiousDataBase,
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

    /// @brief 定義空間における曲線上の点 C(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetDefinedPointAt(const double t) const override;

    /// @brief 定義空間における曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 常に`std::nullopt`
    std::optional<Vector3d> TryGetDefinedTangentAt(const double) const override;

    /// @brief 定義空間における曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 常に`std::nullopt`
    std::optional<Vector3d> TryGetDefinedNormalAt(const double) const override;

    /// @brief 曲線上の点 C(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetPointAt(const double) const override;

    /// @brief 曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 常に`std::nullopt`
    std::optional<Vector3d> TryGetTangentAt(const double) const override;

    /// @brief 曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetNormalAt(const double) const override;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_LINEAR_PATH_H_
