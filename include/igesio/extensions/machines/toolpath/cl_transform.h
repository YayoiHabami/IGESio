/**
 * @file extensions/machines/toolpath/cl_transform.h
 * @brief CLプログラムの変換
 *        (円弧の分割、単位換算、区間の列挙、アプローチ/リトラクト)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note いずれも`ClProgram`から`ClProgram` (または補助情報) への変換である.
 *       動作生成/経路線/NC出力で共用する. 運動学は扱わない.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_TRANSFORM_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_TRANSFORM_H_

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines {

/// @brief 円弧を弦誤差で等角分割した通過点列を計算する
/// @param start 始点 (ワーク座標)
/// @param arc 円弧
/// @param chord_tolerance 弦誤差 [mm] (正の値)
/// @return 通過点列 (終点を含み、始点を含まない). 半径が0の場合は終点のみ
/// @note 回転角θ、半径r、弦誤差εに対し、分割数は
///       `N = max(1, ⌈θ / (2 arccos(1 - ε/r))⌉)`、ε ≥ r なら ⌈2θ/π⌉ とする.
///       全円 (`full_turns = 1`) は θ = 2π
/// @throw std::invalid_argument `chord_tolerance`が正でない,
///        または法線がゼロベクトルの場合
std::vector<igesio::Vector3d> DiscretizeArc(
        const igesio::Vector3d& start,
        const ClArc& arc, double chord_tolerance);

/// @brief 長さの単位を換算する (座標、円弧の中心、送り)
/// @param program 換算するプログラム
/// @param scale 乗じる係数 (inch→mmなら`kInchToMillimeter`)
/// @note `tool_axis`と軸の指令値は換算しない (NC由来の軸の指令はG20/G21で
///       換算済みのため、軸名だけでは直進軸か回転軸か判別できない).
///       基本的には3行1組/APTの`unit_scale`を読込後に適用するために用いる
void ScaleLengths(ClProgram& program, double scale);

/// @brief 円弧を`ClGoto`列に置き換える
/// @param program 変換するプログラム
/// @param chord_tolerance 弦誤差 [mm]
/// @note 主平面外の円弧を出力できないソース形式や3行1組CLの出力に用いる. 各通過点の
///       工具軸は始点値 (直前の`ClState::tool_axis`) と終点値の球面線形補間で,
///       始点値が無ければ最終点のみに終点値を設定する. 始点が分からない円弧
///       (直前の位置が無い) はそのまま残す. `sources`は元の円弧のソース位置は
///       各通過点にコピーする
/// @throw std::invalid_argument `chord_tolerance`が正でない、または法線が
///        ゼロベクトルの円弧がある場合 (`DiscretizeArc`から伝播)
void LinearizeArcs(ClProgram& program, double chord_tolerance);

/// @brief 経路区間 (`records`の半開区間 [begin, end))
struct ClPathRange {
    /// @brief 区間の先頭のレコードインデックス
    std::size_t begin = 0;
    /// @brief 区間の末尾の次のレコードインデックス
    std::size_t end = 0;
    /// @brief 区間名 (`ClMarker`の名前)
    /// @note マーカー無しの分割では空
    std::string name;
    /// @brief 早送りの連続区間か
    bool rapid = false;
};

/// @brief 経路区間を列挙する
/// @param program 対象のプログラム
/// @return 区間の一覧
///         (区間列が`records`のインデックス範囲の分割となる. レコードが無ければ空)
/// @note 分け方は以下.
///       (1) `kPathBegin`の`ClMarker`があれば、`kPathBegin`から`kPathEnd`までを
///           1区間とし、外側のレコードは名前が空の区間にまとめる,
///       (2) 無ければ、早送りの連続区間と切削の連続区間の切り替わりで分ける.
///       いずれも状態レコードとドウェルは直前の区間に含める
std::vector<ClPathRange> EnumeratePaths(const ClProgram& program);

/// @brief アプローチ/リトラクトの指定
/// @note 退避とクリアランス移動のみを扱う
struct ApproachRetractSpec {
    /// @brief 退避方向
    enum class Direction {
        /// @brief 工具軸方向 (`distance`を用いる)
        kToolAxis,
        /// @brief ワーク座標の固定ベクトル (`offset`を用いる)
        kWork,
    };
    /// @brief 退避方向
    Direction direction = Direction::kToolAxis;
    /// @brief 工具軸方向の退避量 [mm] (`kToolAxis`で用いる)
    double distance = 0.0;
    /// @brief ワーク座標の退避ベクトル [mm] (`kWork`で用いる)
    igesio::Vector3d offset = igesio::Vector3d::Zero();
    /// @brief クリアランス点 (退避点からのワーク座標の差分)
    /// @note さらに離れた位置への早送りを追加する. 無い場合は退避点まで
    std::optional<igesio::Vector3d> clearance;
    /// @brief 退避区間の送りの倍率
    /// @note 直前の`feed`に乗じる. 早送りには適用しない
    double feed_ratio = 1.0;
};

/// @brief `EnumeratePaths`の各切削区間の先頭/末尾にアプローチ/リトラクトを挿入する
/// @param program 変換するプログラム
/// @param spec アプローチ・リトラクトの指定
/// @param[out] warnings 挿入を省いた区間の警告 (`nullptr`なら報告しない)
/// @note 挿入するレコードは以下.
///       (1) 先頭: [クリアランス点への早送り] → 退避点への早送り → 開始点への切削
///           (role=`kApproach`)
///       (2) 末尾: 退避点への切削 (role=`kRetract`) → [クリアランス点への早送り]
///       退避区間の切削は`feed × feed_ratio`の送りで行い、送りが分かり倍率が1で
///       ない場合は前後に`ClFeed`を挿入して元の送りに戻す. 早送りのみの区間は対象外.
///       先頭または末尾の点が定まらない (軸の指令のみの移動等) 区間、および
///       `kToolAxis`で工具軸が定まらない区間は警告して省く.
///       挿入したレコードの`sources`は`line = 0` (不明) にする
void InsertApproachRetract(ClProgram& program, const ApproachRetractSpec& spec,
                           std::vector<Diagnostic>* warnings = nullptr);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_TRANSFORM_H_
