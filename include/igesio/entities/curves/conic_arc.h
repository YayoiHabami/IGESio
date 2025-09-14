/**
 * @file entities/curves/conic_arc.h
 * @brief Conic Arc (Type 104): 円錐曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-08
 * @copyright 2025 Yayoi Habami
 * @note このファイルは、円錐曲線 (楕円、放物線、双曲線) を表すエンティティの定義を含む。
 */
#ifndef IGESIO_ENTITIES_CURVES_CONIC_ARC_H_
#define IGESIO_ENTITIES_CURVES_CONIC_ARC_H_

#include <string>
#include <utility>

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief 2次曲線の種類
enum class ConicType {
    /// @brief 楕円
    /// @note 定義空間において原点を中心とする、
    ///       半径 (rx, ry) の楕円として定義される。
    kEllipse = 1,
    /// @brief 双曲線
    kHyperbola = 2,
    /// @brief 放物線
    kParabola = 3
};

/// @brief ConicTypeを文字列に変換する
std::string ToString(const ConicType);

/// @brief 2次曲線弧を表すエンティティクラス (Entity Type 104)
/// @note 2次方程式 Ax^2 + Bxy + Cy^2 + Dx + Ey + F = 0 と、
///       始点、終点から定義される。
class ConicArc : public EntityBase, public virtual ICurve2D {
 protected:
    /// @brief 2次方程式の係数 {A, B, C, D, E, F}
    std::array<double, 6> coeffs_;
    /// @brief 始点の座標 (x, y, z_t)
    Vector3d start_point_;
    /// @brief 終点の座標 (x, y, z_t)
    Vector3d terminate_point_;

    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersの数が11でない場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters(const pointer2ID& de2id) override;

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
    /// @throw std::invalid_argument iges_idがkUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    ConicArc(const RawEntityDE&, const IGESParameterVector& parameters,
             const pointer2ID& = {}, const uint64_t = kUnsetID);

    /// @brief パラメータから生成する場合のコンストラクタ
    /// @param coeffs 2次方程式の係数 {A, B, C, D, E, F}
    /// @param start_point 始点の座標 (x, y)
    /// @param terminate_point 終点の座標 (x, y)
    /// @param z_t z座標 (デフォルトは0.0)
    ConicArc(const std::array<double, 6>&, const Vector2d&,
             const Vector2d&, const double = 0.0);

    /// @brief 楕円弧を生成するコンストラクタ
    /// @param radius X軸/Y軸方向半径 (rx, ry)
    /// @param start_angle 始点の角度 (ラジアン)
    /// @param end_angle 終点の角度 (ラジアン)
    /// @param z_t z座標 (デフォルトは0.0)
    /// @note 楕円の中心は原点 (x=0, y=0) とする。
    ConicArc(const std::pair<double, double>&,
             const double, const double, const double = 0.0);

    /// @brief 円錐曲線の種類
    /// @return 円錐曲線の種類 (楕円、双曲線、放物線)
    ConicType GetConicType() const;



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;



    /**
     * ICurve implementation
     */

    /// @brief 曲線のパラメータ範囲を取得する
    /// @return `{t_start, t_end}`の形式のパラメータ範囲
    /// @note パラメータに問題がある場合は`{0, 0}`を返す
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
    std::optional<Vector3d> TryGetDefinedNormalAt(const double) const override;

    /// @brief 曲線上の点 C(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetPointAt(const double t) const override;

    /// @brief 曲線上の接線ベクトル T(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された接線ベクトル (tx, ty, tz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetTangentAt(const double t) const override;

    /// @brief 曲線上の法線ベクトル N(t) を取得する
    /// @param t パラメータ値
    /// @return 曲線上の正規化された法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d> TryGetNormalAt(const double t) const override;



    /**
     * 描画用
     */
    /// @brief 楕円中心を取得する (kEllipse用; それ以外での動作は未定義)
    /// @return 楕円の中心座標 (x, y, z_t)
    Vector3d EllipseCenter() const;
    /// @brief 楕円の半径を取得する (kEllipse用; それ以外での動作は未定義)
    /// @return X軸/Y軸方向の半径 (rx, ry)
    std::pair<double, double> EllipseRadii() const;
    /// @brief 楕円の始点の角度を取得する (kEllipse用; それ以外での動作は未定義)
    /// @return 始点の角度 [rad]
    double EllipseStartAngle() const;
    /// @brief 楕円の終点の角度を取得する (kEllipse用; それ以外での動作は未定義)
    /// @return 終点の角度 [rad]
    double EllipseEndAngle() const;



 private:
    /// @brief 2次曲線の種類を係数から計算する
    std::optional<ConicType> CalculateConicType() const;

    /// @brief 指定された点が2次曲線上に存在するか判定する
    /// @param x 判定する点のX座標
    /// @param y 判定する点のY座標
    bool IsOnConic(const double, const double) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_CONIC_ARC_H_
