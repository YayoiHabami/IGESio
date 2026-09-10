/**
 * @file extensions/machines/core/rotation.h
 * @brief machines拡張の回転3形式の行列化関数と剛体変換関連の関数関数
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 機械定義フォーマットにおける回転3形式 (軸ベクトル指定・軸角・オイラー角
 *       [I, J, K]) を回転行列へ写す関数と、順運動学で用いる剛体変換 (4x4同次行列)
 *       の合成・逆・適用を提供する. TOMLには依存しない (機械定義の読込側が行う).
 * @note 角度は全てradで受け取る (内部単位. `core/units.h`参照). ファイル上の
 *       deg値は読込側が換算してから渡す.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_CORE_ROTATION_H_
#define IGESIO_EXTENSIONS_MACHINES_CORE_ROTATION_H_

#include <variant>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/tolerances.h"

namespace igesio::extensions::machines {

/// @brief 軸角形式 `{ axis = [x, y, z], angle = a }`
/// @note 単位ベクトルaxisまわりの右ねじ回転
struct AxisAngleSpec {
    /// @brief 回転軸 (単位ベクトル)
    igesio::Vector3d axis;
    /// @brief 回転角 [rad]
    double angle_rad = 0.0;
};

/// @brief オイラー角形式 `[I, J, K]`
/// @note 固定軸まわりにX→Y→Zの順で適用する (R = Rz(K) Ry(J) Rx(I))
struct EulerIjkSpec {
    /// @brief 各軸の回転角 [rad]
    igesio::Vector3d ijk_rad;
};

/// @brief 軸ベクトル指定形式 `{ x_axis = [...], y_axis = [...], z_axis = [...] }`
/// @note 元の座標系のx, y, z軸それぞれの回転後の方向 (回転行列の列ベクトル)
struct ColumnsSpec {
    /// @brief x軸の回転後の方向
    igesio::Vector3d x_axis;
    /// @brief y軸の回転後の方向
    igesio::Vector3d y_axis;
    /// @brief z軸の回転後の方向
    igesio::Vector3d z_axis;
};

/// @brief 回転の記述 (3形式のいずれか)
using RotationSpec = std::variant<AxisAngleSpec, EulerIjkSpec, ColumnsSpec>;

/// @brief 軸まわりの回転行列を作る
/// @param axis 回転軸. 内部で正規化する
/// @param angle_rad 回転角 [rad] (右ねじ)
/// @return 3x3回転行列
/// @throw std::invalid_argument axisがゼロベクトルに近い場合
igesio::Matrix3d RotationAboutAxis(const igesio::Vector3d& axis,
                                   double angle_rad);

/// @brief オイラー角 [I, J, K] から回転行列を作る
/// @param ijk_rad 各軸の回転角 [rad]
/// @return R = Rz(K) Ry(J) Rx(I)
igesio::Matrix3d RotationFromEulerIjk(const igesio::Vector3d& ijk_rad);

/// @brief 軸ベクトル指定から回転行列を作る
/// @param columns 3列の方向ベクトル
/// @return 各列を再正規化した回転行列
/// @throw std::invalid_argument 各列が単位長でない (許容1e-3)、
///        正規直交でない (max|RᵀR - I| > 1e-3)、または行列式が負 (鏡映) の場合.
/// @note エラーメッセージは理由のみとし、文脈は呼び出し側が与える
igesio::Matrix3d RotationFromColumns(const ColumnsSpec& columns);

/// @brief 回転の記述を回転行列へ写す
/// @param spec 3形式のいずれか
/// @return 3x3回転行列
/// @throw std::invalid_argument `RotationAboutAxis`, `RotationFromColumns`の例外
igesio::Matrix3d ResolveRotation(const RotationSpec& spec);

/// @brief 回転と並進から剛体変換 (4x4同次行列) を作る
igesio::Matrix4d MakeRigid(const igesio::Matrix3d& rotation,
                           const igesio::Vector3d& translation);

/// @brief 並進のみの剛体変換を作る
igesio::Matrix4d Translation(const igesio::Vector3d& translation);

/// @brief 点を通る軸まわりの回転を表す剛体変換を作る
/// @param direction 軸方向. 内部で正規化する
/// @param point 軸上の1点
/// @param angle_rad 回転角 [rad] (右ねじ)
/// @return 線形部R、並進部point - R·pointの剛体変換 (順運動学の回転因子)
/// @throw std::invalid_argument directionがゼロベクトルに近い場合
igesio::Matrix4d RotationAboutLine(const igesio::Vector3d& direction,
                                   const igesio::Vector3d& point,
                                   double angle_rad);

/// @brief 剛体変換の逆変換を返す
/// @param transform 剛体変換 (左上3x3が回転行列であること)
/// @return [Rᵀ, -Rᵀt]
igesio::Matrix4d RigidInverse(const igesio::Matrix4d& transform);

/// @brief 剛体変換の回転部 (左上3x3) を返す
igesio::Matrix3d RotationPart(const igesio::Matrix4d& transform);

/// @brief 剛体変換の並進部 (第4列の上3成分) を返す
igesio::Vector3d TranslationPart(const igesio::Matrix4d& transform);

/// @brief 剛体変換を点に適用する (回転+並進)
igesio::Vector3d ApplyPoint(const igesio::Matrix4d& transform,
                            const igesio::Vector3d& point);

/// @brief 剛体変換を方向ベクトルに適用する (回転のみ)
igesio::Vector3d ApplyDirection(const igesio::Matrix4d& transform,
                                const igesio::Vector3d& direction);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_CORE_ROTATION_H_
