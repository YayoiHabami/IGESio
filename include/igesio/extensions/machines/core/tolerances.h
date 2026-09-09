/**
 * @file extensions/machines/core/tolerances.h
 * @brief machines拡張で共有する数値の許容誤差
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 機械定義の入出力、運動学モデル、逆運動学、動作生成等が
 *       同じ値で比較できるように、複数の翻訳単位が参照する許容誤差をここに集約する.
 *       単一の実装ファイルだけが用いる値 (出力桁数等) は各ファイル内に置くこと.
 * @note 単位は内部単位 (mm・rad・s) に従う
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_CORE_TOLERANCES_H_
#define IGESIO_EXTENSIONS_MACHINES_CORE_TOLERANCES_H_

namespace igesio::extensions::machines {

/// @brief 単位ベクトル・直交性・正規直交性の検証に用いる許容誤差
/// @note 機械定義フォーマットの規定値. ノルムが1からこの値を越えて
///       外れる方向ベクトルは仕様違反とし、範囲内なら再正規化して用いる
constexpr double kUnitVectorTolerance = 1e-3;

/// @brief 退化 (ゼロベクトル・射影の消失等) の判定に用いる許容誤差
/// @note ベクトルのノルムがこの値未満なら、方向を定められないとみなす
constexpr double kDegenerateTolerance = 1e-9;

/// @brief 特異値・ピボットの許容誤差
/// @note 直進軸の実効方向が3次元を張るかの判定に用いる.
///       読込時のチェーン検証と位置IKの連立方程式の両方で用いる
constexpr double kRankTolerance = 1e-6;

/// @brief NC指令値の可動範囲 (`limits`) 検査の許容誤差 [mm] または [rad]
/// @note 換算経路の異なる値同士 (IK解と`limits`等) はずれうるるため、
///       範囲検査はこの許容誤差つきで行う. radに対しては約5.7e-8°に相当する
constexpr double kLimitTolerance = 1e-9;

/// @brief 姿勢IKの到達可能判定の許容誤差 (無次元. 単位ベクトルの内積の差)
constexpr double kReachTolerance = 1e-9;

/// @brief 姿勢IKの特異判定 (旋回角が不定) の許容誤差 (無次元)
constexpr double kSingularTolerance = 1e-9;

/// @brief 数値的に0とみなす許容誤差
/// @note 書き出し時の微小値の丸め、単位行列・零ベクトルの判定、
///       IKの退化 (ρ≈0)、2直線の平行判定 (1-b²≈0) に用いる
constexpr double kZeroTolerance = 1e-12;

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_CORE_TOLERANCES_H_
