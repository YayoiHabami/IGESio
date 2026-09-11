/**
 * @file entities/curves/circular_arc.h
 * @brief CircularArc (Type 100): 円弧エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_CURVES_CIRCULAR_ARC_H_
#define IGESIO_ENTITIES_CURVES_CIRCULAR_ARC_H_

#include <memory>

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief 円弧を表すエンティティクラス
/// @note 円弧中心 (x_c, y_c), 始点 (x_s, y_s), 終点 (x_t, y_t) および
///       定義座標系におけるz座標 z_t から定義される.
/// @note IGES5.3のType 100は反時計回り (CCW) の弧のみ表現可能だが、本クラスは
///       ライブラリ内部の拡張表現として時計回り (CW) の弧も表現可能としている
///       (`IsClockwise()`). CWの弧はIGES出力時 (`ConvertToIntermediate`) に
///       `ExpandForExport`で「鏡映したCCW弧 + Transformation Matrix (Type 124)」
///       へ展開される. `ToRawEntityPD`を直接使う低レベル経路では展開されず、
///       PD上は逆向きのCCW弧になるため、`ConvertToIntermediate`経由で出力すること.
/// @note CW弧をType 142/144のパラメータ空間曲線として用いることは非推奨
///       (出力時に付く変換行列を規格どおりに解釈する処理系でのみ正しく扱われる)
class CircularArc : public EntityBase, public virtual ICurve2D {
 protected:
    /// @brief 円弧の中心座標 (x_c, y_c, z_t)
    Vector3d center_;
    /// @brief 始点の座標 (x_s, y_s, z_t)
    Vector3d start_point_;
    /// @brief 終点の座標 (x_t, y_t, z_t)
    Vector3d terminate_point_;
    /// @brief 時計回りの弧か
    /// @note Type 100はCCWのみ表現可能なため、本フラグはライブラリ内部の拡張表現.
    ///       PD読込時は常にfalse. IGES出力時は`ExpandForExport`でCCW弧+Type 124に
    ///       展開される
    bool is_clockwise_ = false;

    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::EntityParameterError parametersの数が7でない場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters([[maybe_unused]] const pointer2ID&) override;

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
    CircularArc(const RawEntityDE&, const IGESParameterVector&,
                const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief 中心点と始点・終点から円弧を生成するコンストラクタ
    /// @param center 円弧の中心座標 (x_c, y_c)
    /// @param start_point 始点の座標 (x_s, y_s)
    /// @param terminate_point 終点の座標 (x_t, y_t)
    /// @param z_t 定義座標系におけるz座標
    /// @param is_clockwise 始点から終点へ時計回りに進む弧とする場合はtrue
    /// @throw igesio::EntityValueError 始点と終点が等距離でない場合、
    ///        または半径が0に近い場合
    CircularArc(const Vector2d&, const Vector2d&, const Vector2d&,
                const double = 0.0, const bool = false);

    /// @brief 中心点と半径、始点角度と終点角度から円弧を生成するコンストラクタ
    /// @param center 円弧の中心座標 (x_c, y_c)
    /// @param radius 円弧の半径
    /// @param start_angle 始点の角度 [rad]
    /// @param end_angle 終点の角度 [rad]
    /// @param z_t 定義座標系におけるz座標
    /// @throw igesio::EntityValueError 半径が0に近い場合
    /// @note `start_angle < end_angle`なら反時計回り、`start_angle > end_angle`なら
    ///       時計回りの弧となる. 同じ角度なら反時計回りの閉じた円
    CircularArc(const Vector2d&, const double,
                const double, const double, const double = 0.0);

    /// @brief 中心点と半径から円（閉じた円弧）を生成するコンストラクタ
    /// @param center 円の中心座標 (x_c, y_c)
    /// @param radius 円の半径
    /// @param z_t 定義座標系におけるz座標
    /// @param is_clockwise 時計回りに進む閉じた円とする場合はtrue
    ///        (点集合は同じで、パラメータの進行方向のみ逆になる)
    /// @throw igesio::EntityValueError 半径が0に近い場合
    CircularArc(const Vector2d&, const double, const double = 0.0,
                const bool = false);



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;

    /// @brief 時計回りの弧をIGES規格の表現 (鏡映CCW弧 + Type 124) へ展開する
    /// @return 反時計回りの弧なら展開不要 (`replacement == nullptr`).
    ///         時計回りの弧なら、y成分を中心について鏡映したCCW弧 (置換) と、
    ///         中心を通るXT軸平行線まわりのπ回転を表すTransformation Matrix
    ///         (補助. Form 0、従属スイッチは00) を返す. 置換弧のDE第7欄は補助行列を指し、
    ///         元の弧が変換行列M0を参照していれば補助行列がM0を参照する
    ///         (点はまず補助行列で、次にM0で変換される. IGES 5.3 §3.2.3)
    /// @throw igesio::ReferenceError 元の弧の変換行列参照がID保持のみで
    ///        未解決の場合
    ExportExpansion ExpandForExport() const override;



    /**
     * ICurve implementation
     */

    /// @brief 曲線のパラメータ範囲を取得する
    /// @return `{t_start, t_end}`の形式のパラメータ範囲 [rad]
    /// @note 始点角θs、角度範囲Δ (> 0) に対し常に`{θs, θs + Δ}`. 反時計回りでは
    ///       パラメータtがそのまま幾何角度になり、時計回りでは幾何角度は
    ///       φ(t) = 2θs − t (tの増加とともに角度が減る) となる
    std::array<double, 2> GetParameterRange() const override;

    /// @brief 曲線が閉じているかどうか
    /// @return 始点と終点が一致する場合は`true`、そうでない場合は`false`
    bool IsClosed() const override;

    /// @brief 定義空間における曲線のn階導関数 C^n(t) を計算する
    /// @param t パラメータ値
    /// @param n 何階まで計算するか; 例えば2を指定した場合、0階 C(t) から2階 C''(t) まで計算
    /// @return 導関数 C'(t), C''(t)、計算できない場合は`std::nullopt`
    std::optional<CurveDerivatives>
    TryGetDefinedDerivatives(const double, const unsigned int) const override;

    /// @brief 定義空間における曲線のバウンディングボックスを取得する
    numerics::BoundingBox GetDefinedBoundingBox() const override;



    /**
     * 描画用
     */

    /// @brief 定義空間における円弧の中心座標を取得する
    /// @return 円弧の中心座標 (x_c, y_c, z_t)
    Vector3d Center() const { return center_; }

    /// @brief 半径を取得する
    /// @return 円弧の半径
    double Radius() const;

    /// @brief 時計回りの弧かどうか
    /// @return 時計回りの場合はtrue
    bool IsClockwise() const { return is_clockwise_; }

    /// @brief 角度範囲 (始点から終点までの中心角) を取得する
    /// @return 角度範囲Δ [rad]. 向きによらず正で、閉じた円では2π
    double SweepAngle() const;

    /// @brief 定義空間における始点の角度を取得する
    /// @return 始点の角度θs [rad] (反時計回りのX軸からの角度、[0, 2π))
    double StartAngle() const;
    /// @brief 定義空間における終点の角度を取得する
    /// @return 終点の幾何角度 [rad]. 反時計回りではθs + Δ、時計回りではθs − Δ
    /// @note 時計回りの弧では`EndAngle() < StartAngle()`となる
    double EndAngle() const;



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

/// @brief 中心点と始点・終点から円弧を作成する
/// @param center 円弧の中心座標 (x_c, y_c)
/// @param start_point 始点の座標 (x_s, y_s)
/// @param terminate_point 終点の座標 (x_t, y_t)
/// @param z_t 定義座標系におけるz座標
/// @param is_clockwise 始点から終点へ時計回りに進む弧とする場合はtrue
/// @return 作成されたCircularArcのshared_ptr
/// @throw igesio::EntityValueError 始点と終点が中心から等距離でない場合、
///        または半径が0に近い場合
std::shared_ptr<CircularArc> MakeCircularArc(
        const Vector2d& center, const Vector2d& start_point,
        const Vector2d& terminate_point, double z_t = 0.0,
        bool is_clockwise = false);

/// @brief 中心点・半径・始終角から円弧を作成する
/// @param center 円弧の中心座標 (x_c, y_c)
/// @param radius 円弧の半径
/// @param start_angle 始点の角度 [rad]
/// @param end_angle 終点の角度 [rad]
/// @param z_t 定義座標系におけるz座標
/// @return 作成されたCircularArcのshared_ptr
/// @throw igesio::EntityValueError 半径が0に近い場合
/// @note `start_angle > end_angle`なら時計回りの弧になる
std::shared_ptr<CircularArc> MakeCircularArc(
        const Vector2d& center, double radius,
        double start_angle, double end_angle, double z_t = 0.0);

/// @brief 閉じた円を作成する
/// @param center 円の中心座標 (x_c, y_c)
/// @param radius 円の半径
/// @param z_t 定義座標系におけるz座標
/// @param is_clockwise 時計回りに進む閉じた円とする場合はtrue
/// @return 作成されたCircularArcのshared_ptr
/// @throw igesio::EntityValueError 半径が0に近い場合
std::shared_ptr<CircularArc> MakeCircle(
        const Vector2d& center, double radius, double z_t = 0.0,
        bool is_clockwise = false);

/// @brief 円弧上の3点 (始点・通過点・終点) から円弧を作成する
/// @param start_point 始点の座標 (x, y)
/// @param mid_point 弧が通過する中間点の座標 (x, y)
/// @param terminate_point 終点の座標 (x, y)
/// @param z_t 定義座標系におけるz座標
/// @return 作成されたCircularArcのshared_ptr
/// @note 3点が時計回りに並ぶ場合は入力順のまま時計回りの弧を生成する
///       (`IsClockwise() == true`. パラメータの進行方向は常に始点→通過点→終点)
/// @throw igesio::EntityValueError 3点が同一直線上にある場合
///        (一致する点を含む場合も同様)
std::shared_ptr<CircularArc> MakeCircularArcThroughPoints(
        const Vector2d& start_point, const Vector2d& mid_point,
        const Vector2d& terminate_point, double z_t = 0.0);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_CIRCULAR_ARC_H_
