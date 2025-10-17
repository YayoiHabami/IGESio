/**
 * @file entities/interfaces/i_surface.h
 * @brief 曲面クラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_INTERFACES_I_SURFACE_H_
#define IGESIO_ENTITIES_INTERFACES_I_SURFACE_H_

#include "igesio/common/matrix.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

/// @brief 全てのサーフェスクラスの基底クラス
/// @note 全てのサーフェスは3次元空間に定義されるため、
///       `ISurface2D`や`ISurface3D`のようなサブクラスは定義しない.
/// @note 個別エンティティクラスにおいては、以下のメンバ関数を
///       オーバーライドすること (すべてpublic):
///       - 常にオーバーライドするべきもの
///         - `IsUClosed`
///         - `IsVClosed`
///         - `GetParameterRange`
///         - `TryGetDefinedPointAt`
///         - `TryGetDefinedNormalAt`
///         - `TryGetPointAt`
///         - `TryGetNormalAt`
class ISurface : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~ISurface() = default;

    /// @brief サーフェスの次元数を取得する
    /// @return サーフェスの次元数 (3)
    virtual unsigned int NDim() const { return 3; }



    /// @brief サーフェスがU方向に閉じているかどうか
    /// @return u_minの座標値とu_maxの座標値が一致する場合は`true`、そうでない場合は`false`
    virtual bool IsUClosed() const = 0;
    /// @brief サーフェスがV方向に閉じているかどうか
    /// @return v_minの座標値とv_maxの座標値が一致する場合は`true`、そうでない場合は`false`
    virtual bool IsVClosed() const = 0;

    /// @brief サーフェスのパラメータ範囲を取得する
    /// @return `{u_start, u_end, v_start, v_end}`の形式でパラメータ範囲を返す
    /// @note 平面の一つの辺が無限に伸びている場合は、対応するパラメータが
    ///       `std::numeric_limits<double>::infinity()`となる
    virtual std::array<double, 4> GetParameterRange() const = 0;
    /// @brief サーフェスのパラメータ範囲を取得する (u方向のみ)
    /// @return `{u_start, u_end}`の形式でu方向のパラメータ範囲を返す
    /// @note u方向に無限に伸びている場合は、対応するパラメータが
    ///       `std::numeric_limits<double>::infinity()`となる
    std::array<double, 2> GetURange() const;
    /// @brief サーフェスのパラメータ範囲を取得する (v方向のみ)
    /// @return `{v_start, v_end}`の形式でv方向のパラメータ範囲を返す
    /// @note v方向に無限に伸びている場合は、対応するパラメータが
    ///       `std::numeric_limits<double>::infinity()`となる
    std::array<double, 2> GetVRange() const;

    /// @brief サーフェス S(u, v) の始点uパラメータが有限かどうか
    /// @return u_startが有限かどうか
    bool HasFiniteUStart() const;
    /// @brief サーフェス S(u, v) の終点uパラメータが有限かどうか
    /// @return u_endが有限かどうか
    bool HasFiniteUEnd() const;
    /// @brief サーフェス S(u, v) の始点vパラメータが有限かどうか
    /// @return v_startが有限かどうか
    bool HasFiniteVStart() const;
    /// @brief サーフェス S(u, v) の終点vパラメータが有限かどうか
    /// @return v_endが有限かどうか
    bool HasFiniteVEnd() const;

    /// @brief サーフェスが有限の面積を持つかどうか
    /// @return 有限の面積を持つ場合は`true`、無限の面積を持つ場合は`false`
    virtual bool IsFinite() const;



    /**
     * 曲面の幾何学的情報 (ベクトル) を取得する
     */

    /// @brief 定義空間におけるサーフェス上の点 P(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d>
    TryGetDefinedPointAt(const double, const double) const = 0;

    /// @brief 定義空間におけるサーフェス上の法線ベクトル N(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d>
    TryGetDefinedNormalAt(const double, const double) const = 0;

    /// @brief サーフェス上の点 P(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d>
    TryGetPointAt(const double, const double) const = 0;

    /// @brief サーフェス上の法線ベクトル N(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    virtual std::optional<Vector3d>
    TryGetNormalAt(const double, const double) const = 0;

    /// @brief サーフェス上の点 P(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の点の座標値 (x, y, z)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    Vector3d GetPointAt(const double, const double) const;

    /// @brief サーフェス上の法線ベクトル N(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の法線ベクトル (nx, ny, nz)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    Vector3d GetNormalAt(const double, const double) const;



    /**
     * 描画に関する情報
     */

    /** TODO: 以下の関数を実装する
    /// @brief サーフェスのバウンディングボックス（軸平行な最小の立方体）を取得する
    /// @return サーフェスのバウンディングボックスを表す`BoundingBox`オブジェクト
    virtual BoundingBox GetBoundingBox() const = 0;
     */
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_SURFACE_H_
