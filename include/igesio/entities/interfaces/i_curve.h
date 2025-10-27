/**
 * @file entities/interfaces/i_curve.h
 * @brief 曲線クラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_INTERFACES_I_CURVE_H_
#define IGESIO_ENTITIES_INTERFACES_I_CURVE_H_

#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/interfaces/i_geometry.h"



namespace igesio::entities {

/// @brief 曲線の導関数
struct CurveDerivatives {
    /// @brief n階までの導関数 C(t), C'(t), ..., C^(n)(t)
    /// @note 未定義等の場合、ゼロベクトルが格納される
    std::vector<Vector3d> derivatives;

    /// @brief コンストラクタ
    /// @param n 何階までの導関数を格納するか
    explicit CurveDerivatives(const unsigned int = 0);

    /// @brief 添字演算子
    /// @param n 添字 (0: 0階 (元の曲線), 1: 1階導関数, 2: 2階導関数, ...)
    /// @return 対応する導関数の参照
    /// @throw std::out_of_range 添字が範囲外の場合
    const Vector3d& operator[](const unsigned int) const;
    /// @brief 添字演算子 (非const版)
    /// @param n 添字 (0: 0階 (元の曲線), 1: 1階導関数, 2: 2階導関数, ...)
    /// @return 対応する導関数の参照
    /// @throw std::out_of_range 添字が範囲外の場合
    Vector3d& operator[](const unsigned int);

    /// @brief サイズを変更する
    /// @param n 新しいサイズ. m階までの導関数を格納する場合、n = m + 1とする
    /// @note 既存のデータは保持され、新しい要素はゼロベクトルで初期化される
    void Resize(const unsigned int);
};

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
///       オーバーライドすること (指定のない限りpublic):
///       - 常にオーバーライドするべきもの
///         - `IsClosed`
///         - `GetParameterRange`
///         - `TryGetDerivatives`
///         - `Transform` (protected): IGeometry由来
class ICurve : public virtual IEntityIdentifier,
               public virtual IGeometry {
 public:
    /// @brief デストラクタ
    virtual ~ICurve() = default;



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
     * 曲線の導関数（ベクトル）を計算する
     */

    /// @brief 定義空間における曲線のn階導関数 C^n(t) を計算する
    /// @param t パラメータ値
    /// @param n 何階まで計算するか; 例えば2を指定した場合、0階 C(t) から2階 C''(t) まで計算
    /// @return 導関数 C'(t), C''(t)、計算できない場合は`std::nullopt`
    virtual std::optional<CurveDerivatives>
    TryGetDerivatives(const double, const unsigned int) const = 0;



    /**
     * 曲線の幾何学的情報 (スカラー) を取得する
     */

    /// @brief 曲線の曲率 κ(t) を計算する
    /// @param t パラメータ値
    /// @return 曲率 κ(t) ∈ [0, ∞), 計算できない場合は-1
    double GetCurvature(const double) const;

    /// @brief 曲線の全長を取得する
    /// @return 曲線の全長
    virtual double Length() const;

    /// @brief 曲線の [t_start, t_end] 間の長さを取得する
    /// @param start パラメータ範囲の開始値
    /// @param end パラメータ範囲の終了値
    /// @return 曲線の t ∈ [start, end] 間の長さ
    /// @throw std::invalid_argument start >= endの場合、
    ///        startまたはendがパラメータ範囲外の場合
    virtual double Length(const double, const double) const;



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
    std::optional<Vector3d> TryGetDefinedPointAt(const double) const;

    /// @brief 定義空間における曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty, tz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetDefinedTangentAt(const double) const;

    /// @brief 定義空間における曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetDefinedNormalAt(const double) const;

    /// @brief 定義空間における曲線上の従法線ベクトル B(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された従法線ベクトル (bx, by, bz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetDefinedBinormalAt(const double) const;

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
    std::optional<Vector3d> TryGetPointAt(const double) const;

    /// @brief 曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty, tz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetTangentAt(const double) const;

    /// @brief 曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetNormalAt(const double) const;

    /// @brief 曲線上の従法線ベクトル B(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された従法線ベクトル (bx, by, bz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetBinormalAt(const double) const;

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

    /// @brief 曲線上の従法線ベクトル B(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された従法線ベクトル (bx, by, bz)
    /// @throw std::out_of_range 指定されたパラメータ値がパラメータ範囲外の場合
    Vector3d GetBinormalAt(const double) const;
};



/// @brief 2次元曲線の基底クラス
class ICurve2D : public ICurve {
 public:
    /// @brief デストラクタ
    ~ICurve2D() override = default;

    /// @brief 幾何形状の次元数を取得する
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
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_CURVE_H_
