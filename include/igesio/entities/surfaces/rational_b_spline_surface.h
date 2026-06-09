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

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
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

    /// @brief 幾何からu/v方向の閉フラグ (PROP1/PROP2) を再計算する
    /// @note 制御点・重み・ノット・パラメータ範囲を変更した際に呼び出す.
    ///       クランプかつパラメータ範囲がノット定義域全体の場合は境界制御点の
    ///       一致と重みの比例で判定し、それ以外は両端での複数サンプル評価で
    ///       判定する (情報的フラグのため後者は厳密判定ではない)
    void UpdateClosedness();



 protected:
    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::EntityParameterError parametersの数が不正な場合
    /// @throw igesio::EntityValueError K1, K2, M1, M2の値が不正な場合
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
    RationalBSplineSurface(const RawEntityDE&, const IGESParameterVector&,
                           const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::EntityDataError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit RationalBSplineSurface(const IGESParameterVector&);

    /// @brief NURBS曲面パラメータから直接構築するコンストラクタ
    /// @param k1 u方向の制御点の最大インデックス
    /// @param k2 v方向の制御点の最大インデックス (制御点数 = (k1+1)(k2+1))
    /// @param m1 曲面のu方向の次数
    /// @param m2 曲面のv方向の次数
    /// @param u_knots ノットベクトルS (サイズk1+m1+2)
    /// @param v_knots ノットベクトルT (サイズk2+m2+2)
    /// @param weights 重みW ((k1+1)×(k2+1))
    /// @param control_points 制御点P (3 × (k1+1)(k2+1); col = i*(k2+1)+j)
    /// @param parameter_range パラメータ範囲 { U(0), U(1), V(0), V(1) }
    /// @param is_u_periodic PROP4: u方向に周期的か (default: false)
    /// @param is_v_periodic PROP5: v方向に周期的か (default: false)
    /// @note PROP1/PROP2 (閉フラグ) は幾何から、PROP3 (polynomial) は
    ///       重みから自動計算する
    /// @throw igesio::EntityValueError 各データの寸法が整合しない場合
    RationalBSplineSurface(
        unsigned int k1, unsigned int k2,
        unsigned int m1, unsigned int m2,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const MatrixXd& weights,
        const Matrix3Xd& control_points,
        const std::array<double, 4>& parameter_range,
        bool is_u_periodic = false,
        bool is_v_periodic = false);

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
    TryGetDefinedDerivatives(const double, const double, const unsigned int) const override;

    /// @brief 定義空間における曲面のバウンディングボックスを取得する
    numerics::BoundingBox GetDefinedBoundingBox() const override;



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

    /// @brief 多項式形式か (PROP3)
    /// @note trueの場合、制御点の重みはすべて等しい値であることが期待される.
    ///       falseの場合、rational (すなわちNURBS) 形式で定義される
    bool IsPolynomial() const noexcept { return is_polynomial_; }

    /// @brief 曲面がu方向に周期的か (PROP4)
    /// @note IGESにおいて定義される、確認用のパラメータであり、
    ///       trueであっても特別な処理は行わない
    bool IsUPeriodic() const noexcept { return is_u_periodic_; }

    /// @brief 曲面がv方向に周期的か (PROP5)
    /// @note IGESにおいて定義される、確認用のパラメータであり、
    ///       trueであっても特別な処理は行わない
    bool IsVPeriodic() const noexcept { return is_v_periodic_; }



    /**
     * データ変更
     */

    /// @brief 制御点P(i, j)を変更する
    /// @param i u方向のインデックス (0 <= i <= K1)
    /// @param j v方向のインデックス (0 <= j <= K2)
    /// @param point 新しい座標値 (x, y, z)
    /// @throw std::out_of_range i, jが範囲外の場合
    /// @note 閉フラグ (PROP1/PROP2) は幾何から自動的に再計算される
    void SetControlPointAt(unsigned int, unsigned int, const Vector3d&);

    /// @brief 制御点全体を差し替える
    /// @param control_points 新しい制御点 (3 × (K1+1)(K2+1); col = i*(K2+1)+j)
    /// @throw igesio::EntityValueError 列数 (制御点数) が既存と異なる場合.
    ///        制御点数を変更する場合はSetDataを使用すること
    /// @note 閉フラグ (PROP1/PROP2) は幾何から自動的に再計算される
    void SetControlPoints(const Matrix3Xd&);

    /// @brief 重みW(i, j)を変更する
    /// @param i u方向のインデックス (0 <= i <= K1)
    /// @param j v方向のインデックス (0 <= j <= K2)
    /// @param weight 新しい重み (正の実数)
    /// @throw std::out_of_range i, jが範囲外の場合
    /// @throw igesio::EntityValueError weightが正の実数でない場合
    /// @note 多項式フラグ (PROP3) と閉フラグ (PROP1/PROP2) は
    ///       自動的に再計算される
    void SetWeightAt(unsigned int, unsigned int, double);

    /// @brief 重み全体を差し替える
    /// @param weights 新しい重み ((K1+1)×(K2+1))
    /// @throw igesio::EntityValueError 形状が既存と異なる場合、
    ///        または正でない重みが含まれる場合. 形状を変更する場合は
    ///        SetDataを使用すること
    /// @note 多項式フラグ (PROP3) と閉フラグ (PROP1/PROP2) は
    ///       自動的に再計算される
    void SetWeights(const MatrixXd&);

    /// @brief u方向のノットベクトルSを差し替える
    /// @param knots 新しいノットベクトル (サイズK1+M1+2)
    /// @throw igesio::EntityValueError サイズが既存と異なる場合、
    ///        または非減少でない場合. サイズ (制御点数や次数) を
    ///        変更する場合はSetDataを使用すること
    /// @note 閉フラグ (PROP1/PROP2) は幾何から自動的に再計算される
    void SetUKnots(const std::vector<double>&);

    /// @brief v方向のノットベクトルTを差し替える
    /// @param knots 新しいノットベクトル (サイズK2+M2+2)
    /// @throw igesio::EntityValueError サイズが既存と異なる場合、
    ///        または非減少でない場合. サイズ (制御点数や次数) を
    ///        変更する場合はSetDataを使用すること
    /// @note 閉フラグ (PROP1/PROP2) は幾何から自動的に再計算される
    void SetVKnots(const std::vector<double>&);

    /// @brief 曲面のパラメータ範囲 U(0), U(1), V(0), V(1) を設定する
    /// @param range パラメータ範囲 { U(0), U(1), V(0), V(1) }
    /// @throw igesio::EntityValueError U(0) >= U(1) または V(0) >= V(1) の場合
    /// @note ノット定義域との関係はValidatePDが警告として検証する.
    ///       閉フラグ (PROP1/PROP2) は両端での一致で定義されるため再計算される
    void SetParameterRange(const std::array<double, 4>&);

    /// @brief u方向の周期フラグ (PROP4) を設定する
    /// @note IGESにおいて定義される、確認用のパラメータであり、
    ///       trueであっても特別な処理は行わない
    void SetUPeriodic(const bool is_periodic) noexcept {
        is_u_periodic_ = is_periodic;
        MarkGeometryModified();
    }

    /// @brief v方向の周期フラグ (PROP5) を設定する
    /// @note IGESにおいて定義される、確認用のパラメータであり、
    ///       trueであっても特別な処理は行わない
    void SetVPeriodic(const bool is_periodic) noexcept {
        is_v_periodic_ = is_periodic;
        MarkGeometryModified();
    }

    /// @brief 曲面の種類 (フォーム番号) を設定する
    /// @param type 曲面の種類 (RationalBSplineSurfaceType)
    /// @return 設定に成功した場合はtrue、無効な種類の場合はfalse
    /// @note 規格上、フォーム番号は優先曲面タイプを伝えるための
    ///       情報的なパラメータであり、幾何形状との一致は検証しない
    bool SetSurfaceType(RationalBSplineSurfaceType);

    /// @brief NURBS構造全体を一括で差し替える
    /// @param degrees 曲面の次数 { M1, M2 }
    /// @param u_knots ノットベクトルS (サイズK1+M1+2)
    /// @param v_knots ノットベクトルT (サイズK2+M2+2)
    /// @param weights 重みグリッド ([i][j] = W(i,j)); 空の場合は
    ///        全て1.0 (polynomial形式)
    /// @param control_points 制御点グリッド ([i][j] = P(i,j));
    ///        K1+1, K2+1はグリッドの寸法から決定される
    /// @param parameter_range パラメータ範囲 { U(0), U(1), V(0), V(1) };
    ///        省略時はノット定義域 [S(0), S(N1)]×[T(0), T(N2)] 全体
    /// @param is_u_periodic PROP4: u方向に周期的か
    /// @param is_v_periodic PROP5: v方向に周期的か
    /// @throw igesio::EntityValueError 各データの整合性が取れない場合
    /// @note ObjectIDやDEレコードを保持したまま編集できる. 検証はすべて
    ///       メンバ変数の変更前に行われ、失敗時に状態は変化しない.
    ///       PROP1/PROP2は幾何から、PROP3は重みから再計算される
    void SetData(std::pair<unsigned int, unsigned int> degrees,
                 const std::vector<double>& u_knots,
                 const std::vector<double>& v_knots,
                 const std::vector<std::vector<double>>& weights,
                 const std::vector<std::vector<Vector3d>>& control_points,
                 const std::optional<std::array<double, 4>>& parameter_range
                     = std::nullopt,
                 bool is_u_periodic = false,
                 bool is_v_periodic = false);



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

/// @brief NURBSパラメータからRationalBSplineSurfaceを作成する
/// @param degrees 曲面の次数 { M1, M2 }
/// @param control_points 制御点グリッド ([i][j] = P(i,j));
///        K1+1, K2+1はグリッドの寸法から決定される
/// @param u_knots ノットベクトル S(-M1), ..., S(N1+M1) (サイズK1+M1+2)
/// @param v_knots ノットベクトル T(-M2), ..., T(N2+M2) (サイズK2+M2+2)
/// @param weights 重みグリッド ([i][j] = W(i,j));
///        空の場合は全て1.0 (polynomial形式)
/// @param parameter_range パラメータ範囲 { U(0), U(1), V(0), V(1) };
///        省略時はノット定義域 [S(0), S(N1)]×[T(0), T(N2)] 全体
/// @param is_u_periodic PROP4: u方向に周期的か
/// @param is_v_periodic PROP5: v方向に周期的か
/// @return 作成されたRationalBSplineSurfaceのshared_ptr
/// @throw igesio::EntityValueError 次数が0の場合、グリッドが空または矩形で
///        ない場合、制御点数が次数+1未満の場合、ノット数が不正または
///        非減少でない場合、重みの寸法が不正または正でない重みが含まれる
///        場合、U(0) >= U(1) または V(0) >= V(1) の場合
std::shared_ptr<RationalBSplineSurface> MakeRationalBSplineSurface(
        std::pair<unsigned int, unsigned int> degrees,
        const std::vector<std::vector<Vector3d>>& control_points,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const std::vector<std::vector<double>>& weights = {},
        std::optional<std::array<double, 4>> parameter_range = std::nullopt,
        bool is_u_periodic = false,
        bool is_v_periodic = false);

/// @brief クランプ一様ノットのB-Spline曲面を作成する
/// @param degrees 曲面の次数 { M1, M2 }
/// @param control_points 制御点グリッド ([i][j] = P(i,j))
/// @param weights 重みグリッド ([i][j] = W(i,j));
///        空の場合は全て1.0 (polynomial形式)
/// @return 作成されたRationalBSplineSurfaceのshared_ptr
/// @throw igesio::EntityValueError 次数が0の場合、グリッドが空または矩形で
///        ない場合、制御点数が次数+1未満の場合、重みの寸法が不正または
///        正でない重みが含まれる場合
/// @note ノットベクトルは両端を重複度M+1でクランプし、内部ノットを
///       等間隔に配置した [0, 1] 上のものを両方向に自動生成する
std::shared_ptr<RationalBSplineSurface> MakeClampedBSplineSurface(
        std::pair<unsigned int, unsigned int> degrees,
        const std::vector<std::vector<Vector3d>>& control_points,
        const std::vector<std::vector<double>>& weights = {});

/// @brief BezierパッチをRationalBSplineSurfaceとして作成する
/// @param control_points 制御点グリッド ([i][j] = P(i,j));
///        次数は {行数-1, 列数-1} となる
/// @param weights 重みグリッド ([i][j] = W(i,j));
///        空の場合は全て1.0 (polynomial形式)
/// @return 作成されたRationalBSplineSurfaceのshared_ptr
/// @throw igesio::EntityValueError 各方向の制御点数が2未満の場合、
///        グリッドが矩形でない場合、重みの寸法が不正または正でない重みが
///        含まれる場合
/// @note パラメータ範囲は [0, 1]² となる
std::shared_ptr<RationalBSplineSurface> MakeBezierSurface(
        const std::vector<std::vector<Vector3d>>& control_points,
        const std::vector<std::vector<double>>& weights = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_RATIONAL_B_SPLINE_SURFACE_H_
