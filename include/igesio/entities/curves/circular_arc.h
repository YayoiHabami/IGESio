/**
 * @file entities/curves/circular_arc.h
 * @brief CircularArc (Type 100): 円弧エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_CURVES_CIRCULAR_ARC_H_
#define IGESIO_ENTITIES_CURVES_CIRCULAR_ARC_H_

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief 円弧を表すエンティティクラス
/// @note 円弧中心 (x_c, y_c), 始点 (x_s, y_s), 終点 (x_t, y_t) および
///       定義座標系におけるz座標 z_t から定義される.
class CircularArc : public EntityBase, public virtual ICurve2D {
 protected:
    /// @brief 円弧の中心座標 (x_c, y_c, z_t)
    Vector3d center_;
    /// @brief 始点の座標 (x_s, y_s, z_t)
    Vector3d start_point_;
    /// @brief 終点の座標 (x_t, y_t, z_t)
    Vector3d terminate_point_;

    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersの数が7でない場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters([[maybe_unused]] const pointer2ID&) override;

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
    CircularArc(const RawEntityDE&, const IGESParameterVector&,
                const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief 中心点と始点・終点から円弧を生成するコンストラクタ
    /// @param center 円弧の中心座標 (x_c, y_c)
    /// @param start_point 始点の座標 (x_s, y_s)
    /// @param terminate_point 終点の座標 (x_t, y_t)
    /// @param z_t 定義座標系におけるz座標
    /// @throw igesio::DataFormatError 始点と終点が等距離でない場合、
    ///        または半径が0に近い場合
    CircularArc(const Vector2d&, const Vector2d&, const Vector2d&,
                const double = 0.0);

    /// @brief 中心点と半径、始点角度と終点角度から円弧を生成するコンストラクタ
    /// @param center 円弧の中心座標 (x_c, y_c)
    /// @param radius 円弧の半径
    /// @param start_angle 始点の角度 [rad]
    /// @param end_angle 終点の角度 [rad]
    /// @param z_t 定義座標系におけるz座標
    /// @throw igesio::DataFormatError 半径が0に近い場合、
    ///        または始点と終点の角度が不正な場合
    CircularArc(const Vector2d&, const double,
                const double, const double, const double = 0.0);

    /// @brief 中心点と半径から円（閉じた円弧）を生成するコンストラクタ
    /// @param center 円の中心座標 (x_c, y_c)
    /// @param radius 円の半径
    /// @param z_t 定義座標系におけるz座標
    CircularArc(const Vector2d&, const double, const double = 0.0);



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;



    /**
     * ICurve implementation
     */

    /// @brief 曲線のパラメータ範囲を取得する
    /// @return `{t_start, t_end}`の形式のパラメータ範囲 [rad]
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
     * 描画用
     */

    /// @brief 定義空間における円弧の中心座標を取得する
    /// @return 円弧の中心座標 (x_c, y_c, z_t)
    Vector3d Center() const { return center_; }

    /// @brief 半径を取得する
    /// @return 円弧の半径
    double Radius() const;

    /// @brief 定義空間における始点の角度を取得する
    /// @return 始点の角度 [rad]
    /// @note 反時計周りのX軸からの角度、start_angle < end_angle
    double StartAngle() const;
    /// @brief 定義空間における終点の角度を取得する
    /// @return 終点の角度 [rad]
    /// @note 反時計周りのX軸からの角度、start_angle < end_angle
    double EndAngle() const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_CIRCULAR_ARC_H_
