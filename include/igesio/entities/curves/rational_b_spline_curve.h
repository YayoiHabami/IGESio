/**
 * @file entities/curves/rational_b_spline_curve.h
 * @brief Rational B-Spline Curve (Type 126): 有理Bスプライン曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-15
 * @copyright 2025 Yayoi Habami
 * @note IGESにおけるRational B-Spline Curveは、NURBS
 *       (Non-Uniform Rational B-Spline) 曲線を含む。
 */
#ifndef IGESIO_ENTITIES_CURVES_RATIONAL_B_SPLINE_CURVE_H_
#define IGESIO_ENTITIES_CURVES_RATIONAL_B_SPLINE_CURVE_H_

#include <vector>

#include "igesio/common/matrix.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief Rational B-Spline Curveの種類
/// @note フォーム番号に対応する
enum class RationalBSplineType {
    /// @brief 曲線の形状はパラメータにより決定される
    kUndetermined = 0,
    /// @brief 直線
    kLine = 1,
    /// @brief 円弧 (or 円)
    kCircularArc = 2,
    /// @brief 楕円弧 (or 楕円)
    kEllipticArc = 3,
    /// @brief 放物線
    kParabolicArc = 4,
    /// @brief 双曲線
    kHyperbolicArc = 5
};

/// @brief 有理Bスプライン曲線エンティティ (Entity Type 126)
/// @note 曲線の次数を M、制御点の数を K+1とする. N = 1 + K - M として、
///       ノットベクトルは T(-M), ..., T(N + M) の N + 2M + 1 個、
///       重みは W(0), ..., W(K) の K + 1 個、
///       制御点は P(0), ..., P(K) (P(i) = (x_i, y_i, z_i)) の K + 1 個で
///       定義される. また、曲線は V(0) <= t <= V(1) の範囲で定義される.
class RationalBSplineCurve : public EntityBase, public virtual ICurve3D {
 private:
    /// @brief 曲線の次数 M
    unsigned int degree_ = 0;
    /// @brief 曲線が平面的か (PROP1)
    bool is_planar_ = false;
    /// @brief 曲線が閉じているか (PROP2)
    bool is_closed_ = false;
    /// @brief 多項式形式か (PROP3)
    /// @note falseの場合、rational (すなわちNURBS) 形式で定義される.
    ///       trueの場合、制御点の重みはすべて等しい値であることが期待される
    bool is_polynomial_ = false;
    /// @brief 周期的か (PROP4)
    bool is_periodic_ = false;

    /// @brief ノットベクトル T　(T(-M), ..., T(1 + K))
    std::vector<double> knots_;
    /// @brief 重み W (W(0), ..., W(K))
    std::vector<double> weights_;
    /// @brief 制御点 P (P(0), ..., P(K)) (P(i) = (x_i, y_i, z_i))
    Matrix3Xd control_points_;
    /// @brief 曲線のパラメータ範囲 V (V(0), V(1))
    std::array<double, 2> parameter_range_;

    /// @brief 面法線ベクトル
    /// @note 曲線が平面的でない場合は定義されない
    /// @note TryGetDefinedNormalAt(t)などは、ある点C(t)における、
    ///       曲線の主法線ベクトルを返す一方で、このメンバ変数は
    ///       曲線が配置される平面の法線ベクトルを表す
    std::optional<Vector3d> normal_vector_;

 protected:
    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersの数が不正な場合
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
    RationalBSplineCurve(const RawEntityDE&, const IGESParameterVector&,
                         const pointer2ID& = {}, const uint64_t = kUnsetID);

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがkUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit RationalBSplineCurve(const IGESParameterVector&);

    /// @brief 曲線の種類
    /// @return 曲線の種類 (RationalBSplineType)
    RationalBSplineType GetCurveType() const;



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
    std::optional<Vector3d> TryGetNormalAt(const double) const override;



    /**
     * 描画
     */

    /// @brief 曲線の次数 M
    int Degree() const noexcept { return degree_; }

    /// @brief ノットベクトル T (T(-M), ..., T(1 + K))
    const std::vector<double>& Knots() const noexcept { return knots_; }

    /// @brief 重み W (W(0), ..., W(K))
    const std::vector<double>& Weights() const noexcept { return weights_; }

    /// @brief 制御点 P (P(0), ..., P(K)) (P(i) = (x_i, y_i, z_i))
    const Matrix3Xd& ControlPoints() const noexcept { return control_points_; }

 private:
    /// @brief 基底関数とその導関数の計算結果を格納する構造体
    struct BasisFunctionResult {
        /// @brief パラメータtが含まれるノットスパン
        /// @note [T(j), T(j+1)] なる j
        int knot_span;
        /// @note 基底関数の値
        std::vector<double> values;
        /// @note 基底関数の導関数の値
        /// @note derivatives[i]は(i+1)次導関数の値
        std::vector<std::vector<double>> derivatives;
    };

    /// @brief Bスプライン基底関数とその導関数を計算する
    /// @param t パラメータ値
    /// @param num_derivatives 計算する導関数の次数
    ///        (0なら基底関数のみ、1なら1次導関数まで)
    /// @return BasisFunctionResult構造体、計算に失敗した場合はstd::nullopt
    std::optional<BasisFunctionResult>
    TryComputeBasisFunctions(const double, const int) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_RATIONAL_B_SPLINE_CURVE_H_
