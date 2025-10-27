/**
 * @file entities/interfaces/i_geometry.h
 * @brief 曲線・曲面等のクラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-10-21
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_INTERFACES_I_GEOMETRY_H_
#define IGESIO_ENTITIES_INTERFACES_I_GEOMETRY_H_

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"



namespace igesio::entities {

/// @brief 全ての幾何形状クラスの基底クラス
/// @note 派生クラスにおいては、以下のメンバ関数をオーバーライドすること
///       - `Transform` (protected): 常にオーバライドが必要
///       - `NDim` (public): 定義空間が2次元のエンティティの場合のみ
class IGeometry : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~IGeometry() = default;

    /// @brief 幾何形状の次元数を取得する
    /// @note 基本的には3（3次元形状）を返すが、定義空間が2次元のエンティティ
    ///       については2を返す. IGES 5.3で規定された定義空間が平面の場合に
    ///       のみ2を返すため、結果的に形状が平面上に存在する形状、例えば
    ///       制御点のz座標がすべて0であるRationalBSplineSurfaceなどは、
    ///       3を返すことに注意 (定義空間は3次元であるため).
    virtual unsigned int NDim() const noexcept { return 3; }



    /**
     * 描画に関する情報
     */

    /** TODO: 以下の関数を実装する
    /// @brief サーフェスのバウンディングボックス（軸平行な最小の直方体）を取得する
    /// @return サーフェスのバウンディングボックスを表す`BoundingBox`オブジェクト
    virtual BoundingBox GetBoundingBox() const = 0;
     */



 protected:
    /// @brief エンティティ自身が参照する変換行列に従い、座標orベクトルを変換する
    /// @param input 変換前の座標orベクトル v
    /// @param is_point 座標を変換する場合は`true`、ベクトルを変換する場合は`false`
    /// @return 変換後の座標orベクトル. 回転行列 R、平行移動ベクトル T に対し、
    ///         座標値の場合は v' = Rv + T、ベクトルの場合は v' = Rv
    /// @note inputがstd::nulloptの場合はそのまま返す
    /// @note 各派生クラスで`... Transform(...) const { return TransformImpl(..); }`
    ///       としてオーバライドすること
    virtual std::optional<Vector3d> Transform(
            const std::optional<Vector3d>&, const bool) const = 0;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_GEOMETRY_H_
