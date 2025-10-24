/**
 * @file entities/curves/parametric_spline_curve.h
 * @brief Parametric Spline Curve (Type 112): パラメトリックスプライン曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-12
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_CURVES_PARAMETRIC_SPLINE_CURVE_H_
#define IGESIO_ENTITIES_CURVES_PARAMETRIC_SPLINE_CURVE_H_

#include <vector>

#include "igesio/common/matrix.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief Parametric Spline Curveの種類
/// @note CTYPE (Parameter Dataセクションの1番目のパラメータ) に対応する
enum class ParametricSplineCurveType {
    /// @brief 直線
    kLinear = 1,
    /// @brief 二次曲線
    kQuadratic = 2,
    /// @brief 三次曲線
    kCubic = 3,
    /// @brief Wilson-Fowler曲線
    kWilsonFowler = 4,
    /// @brief 修正Wilson-Fowler曲線
    kModifiedWilsonFowler = 5,
    /// @brief Bスプライン曲線
    kBSpline = 6
};

/// @brief パラメトリックスプライン曲線エンティティ (Entity Type 112)
/// @note 曲線の次数を H、セグメント数を N とする. パラメトリックスプライン曲線は
///       それぞれ多項式で定義される N 個のセグメントから構成される. 各セグメント i
///       (0 <= i <= N-1) は、パラメータ範囲 T(i) <= u < T(i+1) で定義され、s=u-T(i)
///       を用いて、曲線上の点 C(u) = (x(u), y(u), z(u)) は以下のように表される.
///       $p(u) = A_p(i) + s * B_p(i) + s^2 * C_p(i) + s^3 * D_p(i)$ (p = x, y, z)
///       ここで、A_p(i), B_p(i), C_p(i), D_p(i) はそれぞれセグメント i における
///       p成分の多項式係数である. H=1の場合、C_p(i) = D_p(i) = 0、
///       H=2の場合、D_p(i) = 0 となる.
class ParametricSplineCurve : public EntityBase, public virtual ICurve3D {
 private:
    /// @brief 曲線の種類 (CTYPE)
    ParametricSplineCurveType curve_type_ = ParametricSplineCurveType::kBSpline;

    /// @brief 曲線の次数 H (0 <= H <= 3)
    /// @note Hは曲線の滑らかさの指標として使用される.
    ///       H=0の場合は、曲線は（すべてのブレークポイントで）連続的である.
    ///       H=1の場合は、曲線は連続的かつ勾配の連続性を持つ.
    ///       H=2の場合は、曲線は連続的かつ勾配と曲率の連続性を持つ.
    unsigned int degree_ = 0;

    /// @brief 曲面の次元
    /// @note 平面上で定義されている場合は2、3次元空間で定義されている場合は3
    unsigned int n_dim_ = 3;

    /// @brief ブレークポイント T(1), ..., T(N+1) (N+1個)
    std::vector<double> breakpoints_;

    /// @brief 各セグメントにおける多項式係数
    /// @note coefficients_.block<3, 4>(0, 4*i) はセグメント i (0 <= i <= N-1)
    ///       における多項式係数 A_p(i), B_p(i), C_p(i), D_p(i) (p = x, y, z)
    ///       を含む 3x4 行列を表す. 1セグメントあたり3x4個の係数が格納されるため、
    ///       coefficients_のサイズは 3 x (4 * N) となる.
    Matrix3Xd coefficients_;

    /// @brief 末端 u=T(N+1) における関数値 ~ 3次導関数値
    /// @note i列目 (i=0,1,2,3) はそれぞれ関数値、1次導関数値、
    ///       2次導関数値 / 2!、3次導関数値 / 3! を表す
    /// @note 末端値をuを計算せずに取得するために定義されているが、
    ///       本プログラムでは使用されない
    Matrix<double, 3, 4> end_derivatives_;

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
    ParametricSplineCurve(const RawEntityDE&, const IGESParameterVector&,
                          const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit ParametricSplineCurve(const IGESParameterVector&);

    /// @brief 曲線の種類
    /// @return 曲線の種類 (ParametricSplineCurveType)
    ParametricSplineCurveType GetCurveType() const noexcept;



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

    /// @brief 定義空間における曲線のn階導関数 C^n(t) を計算する
    /// @param t パラメータ値
    /// @param n 何階まで計算するか; 例えば2を指定した場合、0階 C(t) から2階 C''(t) まで計算
    /// @return 導関数 C'(t), C''(t)、計算できない場合は`std::nullopt`
    std::optional<CurveDerivatives>
    TryGetDerivatives(const double, const unsigned int) const override;



    /**
     * 描画
     */

    /// @brief 曲線の次数 H
    /// @return 曲線の次数 H (0 <= H <= 3)
    unsigned int Degree() const noexcept { return degree_; }

    /// @brief セグメント数 N
    /// @return セグメント数 N (N >= 1)
    /// @note ブレークポイントの数-1
    unsigned int NumberOfSegments() const noexcept {
        return static_cast<unsigned int>(breakpoints_.size() - 1);
    }

    /// @brief ブレークポイント T(0), ..., T(N)
    /// @return ブレークポイントのベクトル (サイズ N+1)
    const std::vector<double>& Breakpoints() const noexcept { return breakpoints_; }

    /// @brief 各セグメントにおける多項式係数
    /// @return 各セグメントにおける多項式係数を含む 3x(4*N) 行列
    const Matrix3Xd& Coefficients() const noexcept { return coefficients_; }

    /// @brief i番目のセグメントにおける多項式係数
    /// @param i セグメントのインデックス (0 <= i < N)
    /// @return セグメント i における多項式係数を含む 3x4 行列
    /// @throw std::out_of_range iが0未満またはN以上の場合
    /// @note IGES規格書と異なり、0始まりのインデックスを使用する
    Matrix34d Coefficients(const unsigned int i) const {
        if (i < 0 || i >= NumberOfSegments()) {
            throw std::out_of_range("Segment index out of range");
        }
        return coefficients_.block<3, 4>(0, 4 * i);
    }



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



 private:
    /// @brief パラメータ t に対応するセグメントのインデックス i
    ///        と、そのセグメント内の局所パラメータ s を計算する
    /// @param t パラメータ値
    /// @return `{i, s}`
    std::optional<std::pair<unsigned int, double>>
    FindSegmentIndex(const double t) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_PARAMETRIC_SPLINE_CURVE_H_
