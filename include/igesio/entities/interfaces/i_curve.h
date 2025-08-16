/**
 * @file entities/interfaces/i_curve.h
 * @brief 曲線クラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_INTERFACES_I_CURVE_H_
#define IGESIO_ENTITIES_INTERFACES_I_CURVE_H_

#include "igesio/common/matrix.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"



namespace igesio::entities {

/// @brief 全ての曲線クラスの基底クラス
/// @note 全ての曲線は2次元または3次元空間に定義されるため、
///       `ICurve2D`や`ICurve3D`のようなサブクラスを定義する.
///       すべての曲線は定義空間と、実際の空間の両方の表現を持つ.
///       定義空間は2次元または3次元の理想的な空間であり、例えば
////      Circular Arcはz = z_Tの平面上に定義される. 一方、親から見た
///       時の座標値は、それに回転・平行移動を適用したものとなる.
///       前者を`TryGetDefinedPointAt`などの関数で取得し、
///       後者を`TryGetPointAt`などの関数で取得する
/// @note 個別エンティティクラスにおいては、以下のメンバ関数を
///       オーバーライドすること (すべてpublic):
///       - 常にオーバーライドするべきもの
///         - `IsClosed`
///         - `GetParameterRange`
///         - `TryGetDefinedPointAt`
///         - `TryGetDefinedTangentAt`
///         - `TryGetDefinedNormalAt`
///         - `TryGetPointAt`
///         - `TryGetTangentAt`
///         - `TryGetNormalAt`
class ICurve : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~ICurve() = default;

    /// @brief 曲線の次元数 (定義空間の次元数) を取得する
    /// @return 曲線の次元数 (2次元曲線は2、3次元曲線は3を返す)
    virtual unsigned int NDim() const noexcept = 0;



    /// @brief 曲線が閉じているかどうか
    /// @return 始点と終点が一致する場合は`true`、そうでない場合は`false`
    virtual bool IsClosed() const = 0;

    /// @brief 曲線のパラメータ範囲を取得する
    /// @return `{t_start, t_end}`の形式でパラメータ範囲を返す
    ///         半直線の場合は`{0, std::numeric_limits<double>::infinity()}`のように返す
    virtual std::array<double, 2> GetParameterRange() const = 0;

    /// @brief 曲線 C(t) の始点のパラメータ t_start が有限かどうか
    /// @return 曲線の始点のパラメータ
    bool HasFiniteStart() const;
    /// @brief 曲線 C(t) の終点のパラメータ t_end が有限かどうか
    /// @return 曲線の終点のパラメータ
    bool HasFiniteEnd() const;

    /// @brief 曲線が有限な長さを持つかどうか
    /// @return 線分や円弧などの有限な曲線は`true`を返す
    bool IsFinite() const;



    /**
     * 曲線の幾何学的情報 (ベクトル) を取得する
     */

    /// @brief 定義空間における曲線の始点を取得する
    /// @return 曲線の始点の座標値 (x, y, z).
    ///         半直線など、始点が存在しない場合は`std::nullopt`
    /// @note 曲線が閉じている場合は始点と終点が一致する
    std::optional<Vector3d> TryGetDefinedStartPoint() const;

    /// @brief 定義空間における曲線の終点を取得する
    /// @return 曲線の終点の座標値 (x, y, z).
    ///         半直線など、終点が存在しない場合は`std::nullopt`
    /// @note 曲線が閉じている場合は始点と終点が一致する
    std::optional<Vector3d> TryGetDefinedEndPoint() const;

    /// @brief 定義空間における曲線上の点 C(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d> TryGetDefinedPointAt(const double) const = 0;

    /// @brief 定義空間における曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty, tz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d> TryGetDefinedTangentAt(const double) const = 0;

    /// @brief 定義空間における曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d> TryGetDefinedNormalAt(const double) const = 0;

    /// @brief 曲線の始点を取得する
    /// @return 曲線の始点の座標値 (x, y, z).
    ///         半直線など、始点が存在しない場合は`std::nullopt`
    /// @note 曲線が閉じている場合は始点と終点が一致する
    std::optional<Vector3d> TryGetStartPoint() const;

    /// @brief 曲線の終点を取得する
    /// @return 曲線の終点の座標値 (x, y, z).
    ///         半直線など、終点が存在しない場合は`std::nullopt`
    /// @note 曲線が閉じている場合は始点と終点が一致する
    std::optional<Vector3d> TryGetEndPoint() const;

    /// @brief 曲線上の点 C(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d> TryGetPointAt(const double) const = 0;

    /// @brief 曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty, tz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d> TryGetTangentAt(const double) const = 0;

    /// @brief 曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d> TryGetNormalAt(const double) const = 0;

    /// @brief 曲線の始点を取得する
    /// @return 曲線の始点の座標値 (x, y, z)
    /// @throw std::out_of_range 曲線の始点が存在しない場合
    Vector3d GetStartPoint() const;

    /// @brief 曲線の終点を取得する
    /// @return 曲線の終点の座標値 (x, y, z)
    /// @throw std::out_of_range 曲線の終点が存在しない場合
    Vector3d GetEndPoint() const;

    /// @brief 曲線上の点 C(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の点の座標値 (x, y, z)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    Vector3d GetPointAt(const double) const;

    /// @brief 曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty, tz)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    Vector3d GetTangentAt(const double) const;

    /// @brief 曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, nz)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    Vector3d GetNormalAt(const double) const;



    /**
     * 描画に関する情報
     */

    /** TODO: 以下の関数を実装する
    /// @brief サーフェスのバウンディングボックス（軸平行な最小の直方体）を取得する
    /// @return サーフェスのバウンディングボックスを表す`BoundingBox`オブジェクト
    virtual BoundingBox GetBoundingBox() const = 0;
     */
};



/// @brief 2次元曲線の基底クラス
class ICurve2D : public ICurve {
 public:
    /// @brief デストラクタ
    ~ICurve2D() override = default;

    /// @brief 曲線の次元数 (定義空間の次元数) を取得する
    /// @return 曲線の次元数 (2)
    unsigned int NDim() const noexcept override { return 2; }



    /**
     * 曲線の幾何学的情報 (ベクトル; 2D) を取得する
     */

    /// @brief 定義空間における曲線の始点を取得する (ICurve2D専用)
    /// @return 曲線の始点の座標値 (x, y).
    ///         半直線など、始点が存在しない場合は`std::nullopt`
    /// @note 曲線が閉じている場合は始点と終点が一致する
    std::optional<Vector2d> TryGetDefinedStartPoint2D() const;

    /// @brief 定義空間における曲線の終点を取得する (ICurve2D専用)
    /// @return 曲線の終点の座標値 (x, y).
    ///         半直線など、終点が存在しない場合は`std::nullopt`
    /// @note 曲線が閉じている場合は始点と終点が一致する
    std::optional<Vector2d> TryGetDefinedEndPoint2D() const;

    /// @brief 定義空間における曲線上の点 C(t) を取得する (ICurve2D専用)
    /// @param t パラメータ値
    /// @return 曲線上の点の座標値 (x, y).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector2d> TryGetDefinedPointAt2D(const double) const;

    /// @brief 定義空間における曲線上の接線ベクトル T(t) を取得する (ICurve2D専用)
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector2d> TryGetDefinedTangentAt2D(const double) const;

    /// @brief 定義空間における曲線上の法線ベクトル N(t) を取得する (ICurve2D専用)
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector2d> TryGetDefinedNormalAt2D(const double) const;

    /// @brief 定義空間における曲線の始点を取得する (ICurve2D専用)
    /// @return 曲線の始点の座標値 (x, y)
    /// @throw std::out_of_range 曲線の始点が存在しない場合
    Vector2d GetStartPoint2D() const;

    /// @brief 定義空間における曲線の終点を取得する (ICurve2D専用)
    /// @return 曲線の終点の座標値 (x, y)
    /// @throw std::out_of_range 曲線の終点が存在しない場合
    Vector2d GetEndPoint2D() const;
};



/// @brief 3次元曲線の基底クラス
class ICurve3D : public ICurve {
 public:
    /// @brief デストラクタ
    ~ICurve3D() override = default;

    /// @brief 曲線の次元数 (定義空間の次元数) を取得する
    /// @return 曲線の次元数 (3)
    unsigned int NDim() const noexcept override { return 3; }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_CURVE_H_
