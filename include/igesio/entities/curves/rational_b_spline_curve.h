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

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief Rational B-Spline Curveの種類
/// @note フォーム番号に対応する
enum class RationalBSplineCurveType {
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
    // NOTE: PROP2 (閉じているか) はIsClosed()で取得可能
    // NOTE: PROP3 (多項式形式か) はIsPolynomial()で取得可能

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

    /// @brief 制御点から平面性 (PROP1) と平面法線を再計算する
    /// @note 制御点を変更した際に呼び出し、不変条件
    ///       is_planar_ ⟺ normal_vector_.has_value() を維持する
    void UpdatePlanarity();

 protected:
    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::EntityParameterError parametersの数が不正な場合
    /// @throw igesio::EntityValueError KまたはMの値が不正な場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters(const pointer2ID& de2id) override;

 public:
    /// @brief コンストラクタ
    /// @param de_record DEレコードのパラメータ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @throw igesio::EntityDataError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    RationalBSplineCurve(const RawEntityDE&, const IGESParameterVector&,
                         const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::EntityDataError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit RationalBSplineCurve(const IGESParameterVector&);

    /// @brief NURBS曲線パラメータから直接構築するコンストラクタ
    /// @param k               制御点の最大インデックス（制御点数 = k+1）
    /// @param m               曲線の次数
    /// @param knots           ノットベクトル T（サイズ k+m+2）
    /// @param weights         重み W（サイズ k+1）
    /// @param control_points  制御点 P（3 × (k+1)）
    /// @param parameter_range パラメータ範囲 { V(0), V(1) }
    /// @param is_periodic     PROP4: 周期的か（default: false）
    /// @note PROP1（is_planar）および normal_vector は制御点から自動計算する.
    ///       制御点が平面的でない場合、または法線が一意に定まらない場合は
    ///       non-planar として扱う.
    /// @throw igesio::EntityDataError parametersのいずれかが正しくない場合
    RationalBSplineCurve(
        unsigned int k,
        unsigned int m,
        const std::vector<double>& knots,
        const std::vector<double>& weights,
        const Matrix3Xd& control_points,
        const std::array<double, 2>& parameter_range,
        bool is_periodic = false);

    /// @brief 曲線の種類
    /// @return 曲線の種類 (RationalBSplineCurveType)
    RationalBSplineCurveType GetCurveType() const;



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;



    /**
     * 直線部・角点サポート (ICurve override)
     */

    /// @brief 直線部のパラメータ区間リストを返す
    /// @return degree_ == 1 の場合は長さ (> 0) の全ノットスパン.
    ///         それ以外は空リスト
    std::vector<std::array<double, 2>> GetLinearSegments() const override;

    /// @brief 角点のパラメータ値リストを返す
    /// @return 内部ノットのうち重複度 >= degree_ のもの.
    ///         重複度m, 次数MのノットはC^(M-m)連続のため,
    ///         m >= M で C^0 (角点) となる
    std::vector<double> GetCornerParams() const override;



    /**
     * ICurve implementation
     */

    /// @brief 曲線のパラメータ範囲を取得する
    /// @return `{t_start, t_end}`の形式のパラメータ範囲
    /// @note パラメータに問題がある場合は`{0, 0}`を返す
    std::array<double, 2> GetParameterRange() const override;

    /// @brief 曲線が閉じているかどうか (PROP2)
    /// @return 始点と終点が一致する場合は`true`、そうでない場合は`false`
    bool IsClosed() const override;

    /// @brief 定義空間における曲線のn階導関数 C^n(t) を計算する
    /// @param t パラメータ値
    /// @param n 何階まで計算するか; 例えば2を指定した場合、0階 C(t) から2階 C''(t) まで計算
    /// @return 導関数 C'(t), C''(t)、計算できない場合は`std::nullopt`
    std::optional<CurveDerivatives>
    TryGetDefinedDerivatives(const double, const unsigned int) const override;

    /// @brief 定義空間における曲線のバウンディングボックスを取得する
    /// @return すべての制御点を含む最小の軸平行バウンディングボックス
    numerics::BoundingBox GetDefinedBoundingBox() const override;



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

    /// @brief 多項式形式か (PROP3)
    /// @note 制御点の重みがすべて等しい値の場合、trueが返される.
    ///       falseの場合、rational (すなわちNURBS) 形式で定義される.
    bool IsPolynomial() const;

    /// @brief 曲線が周期的か (PROP4)
    /// @note IGESにおいて定義される、確認用のパラメータであり、
    ///       trueであっても特別な処理は行わない
    bool IsPeriodic() const noexcept { return is_periodic_; }



    /**
     * データ取得
     */

    /// @brief 制御点の数 (K+1)
    int NumControlPoints() const noexcept {
        return static_cast<int>(control_points_.cols());
    }

    /// @brief 制御点P(i)を取得する
    /// @param i 制御点のインデックス (0 <= i <= K)
    /// @return 制御点P(i)の座標値 (x, y, z)
    /// @throw std::out_of_range iが範囲外の場合
    Vector3d GetControlPointAt(const size_t) const;

    /// @brief 重みW(i)を取得する
    /// @param i 重みのインデックス (0 <= i <= K)
    /// @return 重みW(i)
    /// @throw std::out_of_range iが範囲外の場合
    double GetWeightAt(const size_t) const;

    /// @brief 曲線が平面的か (PROP1)
    bool IsPlanar() const noexcept { return is_planar_; }

    /// @brief 定義空間における、曲線が乗る平面の単位法線ベクトル
    /// @return 平面の法線ベクトル; 曲線が平面的でない場合はstd::nullopt
    /// @note TryGetDefinedNormalAt(t)が返す曲線の主法線とは異なり、
    ///       曲線が配置される平面の法線を表す
    std::optional<Vector3d> TryGetDefinedPlaneNormal() const {
        return normal_vector_;
    }

    /// @brief 曲線が乗る平面の単位法線ベクトル (親の空間)
    /// @return 変換行列適用後 (v' = Rv) の法線ベクトル;
    ///         曲線が平面的でない場合はstd::nullopt
    std::optional<Vector3d> TryGetPlaneNormal() const;



    /**
     * データ変更
     */

    /// @brief 制御点P(i)を変更する
    /// @param i 制御点のインデックス (0 <= i <= K)
    /// @param point 新しい座標値 (x, y, z)
    /// @throw std::out_of_range iが範囲外の場合
    /// @note 平面性 (PROP1) と平面法線は制御点から自動的に再計算される
    void SetControlPointAt(const size_t, const Vector3d&);

    /// @brief 制御点全体を差し替える
    /// @param control_points 新しい制御点 (3 × (K+1))
    /// @throw igesio::EntityValueError 列数 (制御点数) が既存と異なる場合.
    ///        制御点数を変更する場合はSetDataを使用すること
    /// @note 平面性 (PROP1) と平面法線は制御点から自動的に再計算される
    void SetControlPoints(const Matrix3Xd&);

    /// @brief 重みW(i)を変更する
    /// @param i 重みのインデックス (0 <= i <= K)
    /// @param weight 新しい重み (正の実数)
    /// @throw std::out_of_range iが範囲外の場合
    /// @throw igesio::EntityValueError weightが正の実数でない場合
    void SetWeightAt(const size_t, const double);

    /// @brief 重み全体を差し替える
    /// @param weights 新しい重み (サイズK+1)
    /// @throw igesio::EntityValueError サイズが既存と異なる場合、
    ///        または正でない重みが含まれる場合
    void SetWeights(const std::vector<double>&);

    /// @brief ノットベクトルを差し替える
    /// @param knots 新しいノットベクトル (サイズK+M+2)
    /// @throw igesio::EntityValueError サイズが既存と異なる場合、
    ///        または非減少でない場合. サイズ (制御点数や次数) を
    ///        変更する場合はSetDataを使用すること
    void SetKnots(const std::vector<double>&);

    /// @brief 曲線のパラメータ範囲 V(0), V(1) を設定する
    /// @param range パラメータ範囲 { V(0), V(1) }
    /// @throw igesio::EntityValueError V(0) >= V(1) の場合
    /// @note ノット定義域 [T(0), T(N)] との関係はValidatePDが警告として検証する
    void SetParameterRange(const std::array<double, 2>&);

    /// @brief 周期フラグ (PROP4) を設定する
    /// @note IGESにおいて定義される、確認用のパラメータであり、
    ///       trueであっても特別な処理は行わない
    void SetPeriodic(const bool is_periodic) noexcept {
        is_periodic_ = is_periodic;
    }

    /// @brief 曲線の種類 (フォーム番号) を設定する
    /// @param type 曲線の種類 (RationalBSplineCurveType)
    /// @return 設定に成功した場合はtrue、無効な種類の場合はfalse
    /// @note 規格上、フォーム番号は優先曲線タイプを伝えるための
    ///       情報的なパラメータであり、幾何形状との一致は検証しない
    bool SetCurveType(const RationalBSplineCurveType);

    /// @brief NURBS構造全体を一括で差し替える
    /// @param degree 曲線の次数M
    /// @param knots ノットベクトル (サイズK+M+2)
    /// @param weights 重み (サイズK+1); 空の場合は全て1.0 (polynomial形式)
    /// @param control_points 制御点 (3 × (K+1)); Kは列数から決定される
    /// @param parameter_range パラメータ範囲 { V(0), V(1) };
    ///        省略時はノット定義域 [T(0), T(N)] 全体
    /// @param is_periodic PROP4: 周期的か
    /// @throw igesio::EntityValueError 各データの整合性が取れない場合
    /// @note ObjectIDやDEレコードを保持したまま編集できる. 検証はすべて
    ///       メンバ変数の変更前に行われ、失敗時に状態は変化しない
    void SetData(const unsigned int degree,
                 const std::vector<double>& knots,
                 const std::vector<double>& weights,
                 const Matrix3Xd& control_points,
                 const std::optional<std::array<double, 2>>& parameter_range
                     = std::nullopt,
                 const bool is_periodic = false);



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



/**
 * ファクトリ関数
 */

/// @brief NURBSパラメータからRationalBSplineCurveを作成する
/// @param degree 曲線の次数M
/// @param control_points 制御点 P(0), ..., P(K) (3 × (K+1));
///        Kは列数から決定される
/// @param knots ノットベクトル T(-M), ..., T(N+M) (サイズK+M+2)
/// @param weights 重み W(0), ..., W(K) (サイズK+1);
///        空の場合は全て1.0 (polynomial形式)
/// @param parameter_range パラメータ範囲 { V(0), V(1) };
///        省略時はノット定義域 [T(0), T(N)] 全体
/// @param is_periodic PROP4: 周期的か
/// @return 作成されたRationalBSplineCurveのshared_ptr
/// @throw igesio::EntityValueError 次数が0の場合、制御点数がM+1未満の場合、
///        ノット数が不正または非減少でない場合、重み数が不正または
///        正でない重みが含まれる場合、V(0) >= V(1)の場合
std::shared_ptr<RationalBSplineCurve> MakeRationalBSplineCurve(
        unsigned int degree,
        const Matrix3Xd& control_points,
        const std::vector<double>& knots,
        const std::vector<double>& weights = {},
        std::optional<std::array<double, 2>> parameter_range = std::nullopt,
        bool is_periodic = false);

/// @brief クランプ一様ノットのB-Spline曲線を作成する
/// @param degree 曲線の次数M
/// @param control_points 制御点 (3 × (K+1)); Kは列数から決定される
/// @param weights 重み (サイズK+1); 空の場合は全て1.0 (polynomial形式)
/// @return 作成されたRationalBSplineCurveのshared_ptr
/// @throw igesio::EntityValueError 次数が0の場合、制御点数がM+1未満の場合、
///        重み数が不正または正でない重みが含まれる場合
/// @note ノットベクトルは両端を重複度M+1でクランプし、内部ノットを
///       等間隔に配置した [0, 1] 上のものを自動生成する
std::shared_ptr<RationalBSplineCurve> MakeClampedBSplineCurve(
        unsigned int degree,
        const Matrix3Xd& control_points,
        const std::vector<double>& weights = {});

/// @brief Bezier曲線をRationalBSplineCurveとして作成する
/// @param control_points 制御点 (3 × (K+1)); 次数は列数-1となる
/// @param weights 重み (サイズK+1); 空の場合は全て1.0 (polynomial形式)
/// @return 作成されたRationalBSplineCurveのshared_ptr
/// @throw igesio::EntityValueError 制御点数が2未満の場合、
///        重み数が不正または正でない重みが含まれる場合
/// @note パラメータ範囲は [0, 1] となる
std::shared_ptr<RationalBSplineCurve> MakeBezierCurve(
        const Matrix3Xd& control_points,
        const std::vector<double>& weights = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_RATIONAL_B_SPLINE_CURVE_H_
