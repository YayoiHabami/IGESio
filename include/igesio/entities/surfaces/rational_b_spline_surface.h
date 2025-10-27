/**
 * @file entities/surfaces/rational_b_spline_surface.h
 * @brief Rational B-Spline Surface (Type 126): 有理Bスプライン曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-09-25
 * @copyright 2025 Yayoi Habami
 * @note IGESにおけるRational B-Spline Surfaceは、NURBS
 *       (Non-Uniform Rational B-Spline) 曲面を含む。
 */
#ifndef IGESIO_ENTITIES_SURFACES_RATIONAL_B_SPLINE_SURFACE_H_
#define IGESIO_ENTITIES_SURFACES_RATIONAL_B_SPLINE_SURFACE_H_

#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief Rational B-Spline Surfaceの種類
/// @note IGES Rational B-Spline Surfaceのフォーム番号に対応
enum class RationalBSplineSurfaceType {
    /// @brief 曲面の形状はBスプラインパラメータから決定される
    kUndetermined = 0,
    /// @brief 平面
    kPlane = 1,
    /// @brief 直円柱
    kRightCircularCylinder = 2,
    /// @brief 円錐
    kCone = 3,
    /// @brief 球
    kSphere = 4,
    /// @brief トーラス
    kTorus = 5,
    /// @brief 回転曲面
    kSurfaceOfRevolution = 6,
    /// @brief 柱状曲面
    kTabulatedCylinder = 7,
    /// @brief ルールドサーフェス
    kRuledSurface = 8,
    /// @brief 一般的な二次曲面
    kGeneralQuadricSurface = 9
};

/// @brief 有理Bスプライン曲面エンティティ (Entity Type 128)
/// @note 曲面の次数を M1, M2、制御点の数を (K1+1)(K2+1) とする.
///       N1 = 1 + K1 - M1, N2 = 1 + K2 - M2 として、
///       1つめのノットベクトルはそれぞれ S(-M1), ..., S(N1 + M1) の N1 + 2M1 + 1 個、
///       2つめのノットベクトルはそれぞれ T(-M2), ..., T(N2 + M2) の N2 + 2M2 + 1 個、
///       重みは W(0,0), ..., W(K1,K2) の (K1 + 1)(K2 + 1) 個、
///       制御点は P(0,0), ..., P(K1,K2) (P(i,j) = (x_ij, y_ij, z_ij)) の
///       (K1 + 1)(K2 + 1) 個で定義される. また、曲面は U(0) <= u <= U(1),
///       V(0) <= v <= V(1) の範囲で定義される.
class RationalBSplineSurface : public EntityBase, public virtual ISurface {
 private:
    /// @brief 曲面の次数 M1, M2 (u方向, v方向)
    std::pair<unsigned int, unsigned int> degrees_ = {0, 0};
    /// @brief 曲面がu方向に閉じているか (PROP1)
    bool is_u_closed_ = false;
    /// @brief 曲面がv方向に閉じているか (PROP2)
    bool is_v_closed_ = false;
    /// @brief 多項式形式か (PROP3)
    /// @note falseの場合、rational (すなわちNURBS) 形式で定義される.
    ///       trueの場合、制御点の重みはすべて等しい値であることが期待される
    bool is_polynomial_ = false;
    /// @brief 曲面がu方向に周期的か (PROP4)
    bool is_u_periodic_ = false;
    /// @brief 曲面がv方向に周期的か (PROP5)
    bool is_v_periodic_ = false;

    /// @brief ノットベクトル S (u方向; S(-M1), ..., S(1 + K1))
    std::vector<double> u_knots_;
    /// @brief ノットベクトル T (v方向; T(-M2), ..., T(1 + K2))
    std::vector<double> v_knots_;
    /// @brief 重み W (W(0,0), ..., W(K1,K2))
    /// @note weights_(i, j) は W(i, j) に対応
    MatrixXd weights_;
    /// @brief 制御点 P (P(0,0), ..., P(K1,K2)) (P(i,j) = (x_ij, y_ij, z_ij))
    /// @note control_points_.col(i * (K2 + 1) + j) は P(i, j) に対応
    Matrix3Xd control_points_;
    /// @brief 曲面のパラメータ範囲 (U(0), U(1), V(0), V(1))
    std::array<double, 4> parameter_range_;



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
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    RationalBSplineSurface(const RawEntityDE&, const IGESParameterVector&,
                           const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit RationalBSplineSurface(const IGESParameterVector&);

    /// @brief 曲面の種類
    /// @return 曲面の種類 (RationalBSplineSurfaceType)
    RationalBSplineSurfaceType GetSurfaceType() const;



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;



    /**
     * ISurface implementation
     */

    /// @brief サーフェスがU方向に閉じているかどうか
    /// @return u_minの座標値とu_maxの座標値が一致する場合は`true`、そうでない場合は`false`
    bool IsUClosed() const override { return is_u_closed_; }
    /// @brief サーフェスがV方向に閉じているかどうか
    /// @return v_minの座標値とv_maxの座標値が一致する場合は`true`、そうでない場合は`false`
    bool IsVClosed() const override { return is_v_closed_; }

    /// @brief サーフェスのパラメータ範囲を取得する
    /// @return `{u_start, u_end, v_start, v_end}`の形式でパラメータ範囲を返す
    /// @note 平面の一つの辺が無限に伸びている場合は、対応するパラメータが
    ///       `std::numeric_limits<double>::infinity()`となる
    std::array<double, 4> GetParameterRange() const override {
        return parameter_range_;
    }

    /// @brief 定義空間におけるサーフェスの偏導関数 S^(i,j)(u, v) を計算する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @param order 何階まで計算するか; 例えば2を指定した場合、0階 S(u, v) から
    ///              2階 S^(2,0)(u, v), S^(1,1)(u, v), S^(0,2)(u, v) まで計算
    /// @return 偏導関数 S, Su, Sv, ...、計算できない場合は`std::nullopt`
    std::optional<SurfaceDerivatives>
    TryGetDerivatives(const double, const double, const unsigned int) const override;



    /**
     * 描画用
     */
    /// @brief 曲面の次数 M1, M2 (u方向, v方向)
    std::pair<unsigned int, unsigned int> Degrees() const { return degrees_; }

    /// @brief ノットベクトル S (u方向; S(-M1), ..., S(1 + K1))
    const std::vector<double>& UKnots() const { return u_knots_; }
    /// @brief ノットベクトル T (v方向; T(-M2), ..., T(1 + K2))
    const std::vector<double>& VKnots() const { return v_knots_; }

    /// @brief 重み W (W(0,0), ..., W(K1,K2))
    /// @note weights_(i, j) は W(i, j) に対応.
    ///       各要素はcolumn-majorで格納されているため、
    ///       Weights().data()[i + j * (K1 + 1)] は W(i, j) に対応する
    const MatrixXd& Weights() const { return weights_; }

    /// @brief 重み W(i, j)
    /// @param i u方向のインデックス (0 <= i <= K1)
    /// @param j v方向のインデックス (0 <= j <= K2)
    /// @return 重み W(i, j)
    /// @throw std::out_of_range i, jが範囲外の場合
    double WeightAt(const unsigned int, const unsigned int) const;

    /// @brief 制御点の数 K1, K2 (u方向, v方向)
    /// @note 実際の制御点の数は (K1 + 1)(K2 + 1)
    std::pair<unsigned int, unsigned int> NumControlPoints() const;

    /// @brief 制御点 P (P(0,0), ..., P(K1,K2)) (P(i,j) = (x_ij, y_ij, z_ij))
    /// @note control_points_.col(i * (K2 + 1) + j) は P(i, j) に対応.
    ///       制御点の数 K1, K2 は NumControlPoints() で取得可能.
    const Matrix3Xd& ControlPoints() const { return control_points_; }
    /// @brief 制御点 P(i, j)
    /// @param i u方向のインデックス (0 <= i <= K1)
    /// @param j v方向のインデックス (0 <= j <= K2)
    /// @return 制御点 P(i, j) の座標値 (x_ij, y_ij, z_ij)
    /// @throw std::out_of_range i, jが範囲外の場合
    Vector3d ControlPointAt(const unsigned int, const unsigned int) const;



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

#endif  // IGESIO_ENTITIES_SURFACES_RATIONAL_B_SPLINE_SURFACE_H_
