/**
 * @file entities/interfaces/i_surface.h
 * @brief 曲面クラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_INTERFACES_I_SURFACE_H_
#define IGESIO_ENTITIES_INTERFACES_I_SURFACE_H_

#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/interfaces/i_geometry.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

/// @brief 曲面の偏導関数
class SurfaceDerivatives {
    /// @brief 何階までの偏導関数を格納するか
    /// @note order = nの場合、n+1個の偏導関数を格納する.
    ///       例えばorder = 2の場合、S, Su, Sv, Suu, Suv, Svv を格納する.
    unsigned int order_ = 0;
    /// @brief n階までの偏導関数 S, Su, Sv, Suu, Suv, Svv, ...
    /// @note derivatives[i][j] は i階u偏導関数かつ j階v偏導関数 S^(i,j) に対応.
    ///       未定義等の場合、ゼロベクトルが格納される. 格納されない場所 (i + j > order)
    ///       には要素が存在しない.
    std::vector<std::vector<Vector3d>> derivatives_;

 public:
    /// @brief コンストラクタ
    /// @param order 何階までの偏導関数を格納するか.
    ///              例えばorder = 2の場合、S, Su, Sv, Suu, Suv, Svv を格納する.
    explicit SurfaceDerivatives(const unsigned int = 0);

    /// @brief 何階までの偏導関数を格納しているか取得する
    /// @note order = nの場合、n+1個の偏導関数を格納する.
    ///       例えばorder = 2の場合、S, Su, Sv, Suu, Suv, Svv を格納する.
    unsigned int Order() const noexcept { return order_; }

    /// @brief 偏導関数 S^(i,j) (const版)
    /// @param i u偏導関数の階数
    /// @param j v偏導関数の階数
    /// @return 対応する偏導関数の参照
    /// @throw std::out_of_range i, jが範囲外の場合. i + j > orderの場合も含む
    const Vector3d& operator()(const unsigned int, const unsigned int) const;
    /// @brief 偏導関数 S^(i,j) (非const版)
    /// @param i u偏導関数の階数
    /// @param j v偏導関数の階数
    /// @return 対応する偏導関数の参照
    /// @throw std::out_of_range i, jが範囲外の場合. i + j > orderの場合も含む
    Vector3d& operator()(const unsigned int, const unsigned int);

    /// @brief サイズを変更する
    /// @param order 新しいサイズ.
    /// @note order = nの場合、n+1個の偏導関数を格納する.
    ///       例えばorder = 2の場合、S, Su, Sv, Suu, Suv, Svv を格納する
    /// @note 既存のデータは保持され、新しい要素はゼロベクトルで初期化される
    void Resize(const unsigned int);
};



/// @brief 全てのサーフェスクラスの基底クラス
/// @note 全てのサーフェスは3次元空間に定義されるため、
///       `ISurface2D`や`ISurface3D`のようなサブクラスは定義しない.
/// @note 個別エンティティクラスにおいては、以下のメンバ関数を
///       オーバーライドすること (すべてpublic):
///       - 常にオーバーライドするべきもの
///         - `IsUClosed`
///         - `IsVClosed`
///         - `GetParameterRange`
///         - `TryGetDerivatives`
class ISurface : public virtual IEntityIdentifier,
                 public virtual IGeometry {
 public:
    /// @brief デストラクタ
    virtual ~ISurface() = default;



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
     * 曲面の偏導関数（ベクトル）を計算する
     */

    /// @brief 定義空間におけるサーフェスの偏導関数 S^(i,j)(u, v) を計算する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @param order 何階まで計算するか; 例えば2を指定した場合、0階 S(u, v) から
    ///              2階 S^(2,0)(u, v), S^(1,1)(u, v), S^(0,2)(u, v) まで計算
    /// @return 偏導関数 S, Su, Sv, ...、計算できない場合は`std::nullopt`
    virtual std::optional<SurfaceDerivatives>
    TryGetDerivatives(const double, const double, const unsigned int) const = 0;



    /**
     * 曲面の幾何学的情報 (スカラー) を取得する
     */

    /// @brief 第一基本形式 E, F, G を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return 第一基本形式 E, F, G のタプル.
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<std::tuple<double, double, double>>
    TryGetFirstFundamentalForm(const double, const double) const;

    /// @brief 第二基本形式 L, M, N を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return 第二基本形式 L, M, N のタプル.
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<std::tuple<double, double, double>>
    TryGetSecondFundamentalForm(const double, const double) const;

    /// @brief ガウス曲率 K を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return ガウス曲率 K.
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<double> TryGetGaussianCurvature(const double, const double) const;
    /// @brief 平均曲率 H を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return 平均曲率 H.
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<double> TryGetMeanCurvature(const double, const double) const;
    /// @brief 主曲率 κ1, κ2 を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return 主曲率 κ1, κ2 のペア (κ1 >= κ2).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<std::pair<double, double>>
    TryGetPrincipalCurvatures(const double, const double) const;



    /**
     * 曲面の幾何学的情報 (ベクトル) を取得する
     */

    /// @brief 定義空間におけるサーフェス上の点 P(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d>
    TryGetDefinedPointAt(const double, const double) const;

    /// @brief 定義空間における単位接線ベクトル Tu(u, v) と Tv(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の単位接線ベクトル Tu(u, v), Tv(u, v).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<std::pair<Vector3d, Vector3d>>
    TryGetDefinedTangentAt(const double, const double) const;

    /// @brief 定義空間におけるサーフェス上の法線ベクトル N(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d>
    TryGetDefinedNormalAt(const double, const double) const;

    /// @brief サーフェス上の点 P(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d>
    TryGetPointAt(const double, const double) const;

    /// @brief サーフェス上の接線ベクトル (Tu, Tv) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の接線ベクトル Tu(u, v), Tv(u, v).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<std::pair<Vector3d, Vector3d>>
    TryGetTangentAt(const double, const double) const;

    /// @brief サーフェス上の法線ベクトル N(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d>
    TryGetNormalAt(const double, const double) const;

    /// @brief サーフェス上の点 P(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の点の座標値 (x, y, z)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    Vector3d GetPointAt(const double, const double) const;

    /// @brief サーフェス上の接線ベクトル (Tu, Tv) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の接線ベクトル Tu(u, v), Tv(u, v)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    std::pair<Vector3d, Vector3d> GetTangentAt(const double, const double) const;

    /// @brief サーフェス上の法線ベクトル N(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の法線ベクトル (nx, ny, nz)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    Vector3d GetNormalAt(const double, const double) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_SURFACE_H_
