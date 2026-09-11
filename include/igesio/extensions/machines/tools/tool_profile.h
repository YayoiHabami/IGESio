/**
 * @file extensions/machines/tools/tool_profile.h
 * @brief 工具・ホルダのデータモデルと、簡易アセンブリ
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note 工具やホルダは輪郭線に基づいた回転対象形状として定義する. 各部位は1つ以上の
 *       `ToolProfileElement`で表現する. 各要素は部位の種別 (切れ刃/シャンク/ホルダ)
 *       と、回転軸上に始点と終点を置く母線1本を持ち、それぞれ独立の閉じた回転体とする.
 *       要素同士のz範囲の重なり (ホルダ内部のシャンク等) は許容する.
 * @note 工具やホルダは原点を工具先端 (切れ刃の軸上最先端)、z軸を工具軸 (ホルダ側が正)
 *       とする2次元工具座標系で表現する. 指令点は回転軸上における工具先端からの距離
 *       (`command_point_z`) で表現する.
 *       ホルダは主軸に差し込まれる部分 (テーパシャンク・プルスタッド等) を含めて
 *       モデル化してよい. 主軸側との境界はゲージライン (`gauge_line_z`) で与え、
 *       これより主軸側の部分は主軸内にあるものとして扱う.
 * @note 母線はr-z平面 (r: 半径方向 ≥ 0、z: 工具軸方向) の直線と円弧のみで構成する.
 *       rを右方向、zを上方向に設定した平面で輪郭線を定義するため、先端から上へたどる
 *       凸の円弧 (ボールやラジアス等のR) は反時計回り、ネック部やフィレット等における
 *       凹の円弧は時計回りで表現する.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_PROFILE_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_PROFILE_H_

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"

namespace igesio::extensions::machines {

/// @brief 切れ刃部のデフォルト色 (RGB 0~1. `#ffd900`)
constexpr std::array<float, 3> kDefaultCutterColor = {1.0f, 0.85098f, 0.0f};
/// @brief シャンク部のデフォルト色 (RGB 0~1. `#ffffff`)
constexpr std::array<float, 3> kDefaultShankColor = {1.0f, 1.0f, 1.0f};
/// @brief ホルダ部のデフォルト色 (RGB 0~1. `#8090a0`)
constexpr std::array<float, 3> kDefaultHolderColor = {0.50196f, 0.56471f, 0.62745f};

/// @brief 工具・ホルダの部位の種別
/// @note 実体化時の部位容器 (Assembly) の名前にも用いる (`ToolPartName`)
enum class ToolPart {
    /// @brief 切れ刃部
    /// @note 工具の切削部位のみを指す. 非切削部はkShankとする.
    kCutter,
    /// @brief シャンク部
    /// @note 工具の非切削部位を指す. 切削部はkCutterとする. ホルダ非表示時も描画する
    kShank,
    /// @brief ホルダ部
    kHolder,
};

/// @brief 工具・ホルダの部位名を取得する
/// @return `"cutter"` / `"shank"` / `"holder"`
std::string_view ToolPartName(ToolPart part);

/// @brief 工具・ホルダの部位名を`ToolPart`に変換する
/// @param text `"cutter"` / `"shank"` / `"holder"` (大文字小文字を区別する)
/// @return 対応する部位. 未知の文字列なら`std::nullopt`
std::optional<ToolPart> ParseToolPart(std::string_view text);

/// @brief 工具・ホルダの部位のデフォルトの色を取得する
/// @param part 部位
/// @return RGB (0~1)
const std::array<float, 3>& DefaultPartColor(ToolPart part);

/// @brief r-z平面における母線の1セグメント (直線または円弧)
/// @note 2次元工具座標は (r, z) [mm] であり、先端をz=0とし、主軸側が正方向とする.
///       rは半径方向 (≥ 0)、zは工具軸方向
struct ProfileSegment {
    /// @brief セグメントの種類
    enum class Kind {
        /// @brief 直線
        kLine,
        /// @brief 円弧
        kArc,
    } kind = Kind::kLine;

    /// @brief 始点 (r, z)
    igesio::Vector2d start = igesio::Vector2d::Zero();
    /// @brief 終点 (r, z)
    igesio::Vector2d end = igesio::Vector2d::Zero();
    /// @brief 円弧の中心 (r, z). `kArc`のみ
    /// @note `|start - center| == |end - center|`
    igesio::Vector2d center = igesio::Vector2d::Zero();
    /// @brief 円弧の向き (`kArc`のみ). 2次元工具座標で見て反時計回りならtrue
    bool counter_clockwise = true;

    /// @brief 直線区間を作る
    /// @param start 始点 (r, z)
    /// @param end 終点 (r, z)
    static ProfileSegment Line(
        const igesio::Vector2d& start, const igesio::Vector2d& end);

    /// @brief 円弧区間を作る
    /// @param start 始点 (r, z)
    /// @param end 終点 (r, z)
    /// @param center 中心 (r, z)
    /// @param counter_clockwise 2次元工具座標で見て反時計回りならtrue
    static ProfileSegment Arc(
        const igesio::Vector2d& start, const igesio::Vector2d& end,
        const igesio::Vector2d& center, bool counter_clockwise = true);
};

/// @brief 工具・ホルダの部位要素 (回転体を作る1つの閉じた母線)
/// @note 母線は回転軸上 (r = 0) に始点と終点を持つ. 端面は実体化時に補完しないため,
///       生成側で明示的に持たせること (`CloseElementOnAxis`).
struct ToolProfileElement {
    /// @brief 部位の種別
    ToolPart part = ToolPart::kCutter;
    /// @brief 表示名 (省略可)
    std::string name;
    /// @brief 母線のセグメント列 (連続. 前の`end`と次の`start`が一致する)
    std::vector<ProfileSegment> segments;
    /// @brief 色 (RGB 0~1). 省略時は部位のデフォルト色
    std::optional<std::array<float, 3>> color;
    /// @brief 不透明度 (0=透明〜1=不透明)
    float opacity = 1.0f;
};

/// @brief 工具・ホルダの輪郭
/// @note 工具先端を原点、軸を+z方向とする2次元工具座標系で表現する.
/// @note 簡易アセンブリ由来の寸法 (直径等) は保持しない. GUIの寸法表示は
///       `MaxRadius()`/`CuttingLength()`/`Reach()`/`PartExtent()`から作る
struct ToolProfile {
    /// @brief 工具名
    std::string name;
    /// @brief 部位要素 (順序は自由. 実体化時の番号は部位内の出現順)
    std::vector<ToolProfileElement> elements;
    /// @brief 回転軸上における指令点の位置 (工具先端からの距離) [mm]
    double command_point_z = 0.0;
    /// @brief 回転軸上におけるゲージライン (主軸への取り付け基準面) の位置
    ///        (工具先端からの距離) [mm]
    /// @note これより主軸側 (+z側) の部分は主軸内に差し込まれる. 差し込み深さは
    ///       `Reach() - *gauge_line_z`. 未指定なら`GaugeLength`が
    ///       ホルダ上端で代用して警告する (差し込み部分を持たない輪郭向け)
    std::optional<double> gauge_line_z;

    /// @brief 切れ刃長 (切れ刃要素の最大z) を返す
    /// @return 切れ刃要素が無ければ0
    /// @note 円弧セグメントがz方向に凸な場合、その最上部も考慮する.
    double CuttingLength() const;

    /// @brief 工具全長 (全要素の最大z. 主軸内の差し込み部分を含む) を返す
    /// @note 円弧セグメントがz方向に凸な場合、その最上部も考慮する.
    /// @return 要素が無ければ0
    double Reach() const;

    /// @brief 最大半径 (全要素の最大r) を返す
    /// @return 要素が無ければ0
    /// @note 工具軸に垂直な方向についての最大径. 円弧セグメントのr方向の凸性も考慮する.
    double MaxRadius() const;

    /// @brief 指定部位のz範囲`{min, max}`を返す
    /// @return 指定された部位の要素が無ければ`std::nullopt`
    std::optional<std::array<double, 2>> PartExtent(ToolPart part) const;

    /// @brief 工具先端からゲージラインまでの距離を返す
    /// @param warnings 警告の追加先 (nullptrなら追加しない)
    /// @return `gauge_line_z`があればその値. 無ければホルダ要素の上端 (最大z),
    ///         それも無ければ`Reach()`を返し、いずれも
    ///         `"gauge line not specified; ..."`の警告を追加する
    /// @note 取り付けオフセットと制御点 (`tool_assembly.h`) はこの値を用いる
    double GaugeLength(std::vector<Diagnostic>* warnings) const;
};

/// @brief 工具・ホルダの輪郭を検証する
/// @param profile 検証する輪郭
/// @throw std::invalid_argument 以下のいずれかに違反する場合 (先に検出したもの1件):
///        要素が空、切れ刃要素が無い、区間が空の要素がある、零長の直線、
///        中心から等距離でない円弧の端点、退化した円弧 (半径0または全円)、
///        区間が連続でない、負の半径 (直線の端点・円弧の張り出しを含む)、
///        要素の始点または終点が軸上でない、先端 (切れ刃要素の始点) が原点でない
///        または原点より先端側に出る要素がある、指令点が`[0, Reach()]`の外、
///        ゲージライン (指定時) が`[0, Reach()]`の外.
///        文言は`"ToolProfile: element[i] (<part>) segment[j]: ..."`の形式
/// @note 要素間の重なり・自己交差・部位のz順序・名前の一意性は検査しない.
///       許容誤差は`kDegenerateTolerance`
void ValidateToolProfile(const ToolProfile& profile);

/// @brief 工具/ホルダの輪郭を閉じる
/// @param element 対象の要素 (区間が空なら何もしない)
/// @note 母線の先頭・末尾が回転軸上になければ、それぞれ同じzの軸上点までの直線を補う.
///       既に輪郭が閉じている (始点/終点が回転軸上にある) 要素は変更しない
void CloseElementOnAxis(ToolProfileElement* element);



/**
 * 簡易アセンブリ (輪郭線ではなく寸法で工具を規定する) 関連
 */

/// @brief 簡易アセンブリの寸法定義
/// @note 輪郭線ではなく工具の種類や直径等の寸法で種類を決める.
///       円柱シャンクと円柱ホルダを持つ基本工具
/// @note 長さは換算済みの内部単位 [mm]. 値の検証 (正の値・大小関係) は
///       `MakeSimpleToolProfile`が幾何の成立に必要な範囲でのみ行う
struct SimpleToolSpec {
    /// @brief 工具の種類
    enum class Cutter {
        /// @brief ボールエンドミル (先端が半球)
        kBall,
        /// @brief スクエアエンドミル (先端が平面)
        kSquare,
        /// @brief ラジアスエンドミル (先端の角にコーナR)
        kRadius,
    } cutter = Cutter::kBall;

    /// @brief 指令点の位置
    enum class CommandPoint {
        /// @brief 工具先端
        kTip,
        /// @brief 先端の円弧切れ刃の中心
        /// @note cutterがkBallの場合のみ有効
        kCenter,
    } command_point = CommandPoint::kTip;

    /// @brief 工具直径 [mm]
    double diameter = 0.0;
    /// @brief コーナ半径 [mm] (ラジアスのみ)
    double corner_radius = 0.0;
    /// @brief 切れ刃長 (先端から切れ刃上端まで) [mm]
    double cutting_length = 0.0;
    /// @brief 工具長 (先端からシャンク上端まで) [mm]
    double tool_length = 0.0;
    /// @brief 突き出し長 (先端からホルダ下端まで) [mm]
    double overhang = 0.0;
    /// @brief ホルダ直径 [mm]
    double holder_diameter = 0.0;
    /// @brief ホルダ長 [mm]
    double holder_length = 0.0;
};

/// @brief 簡易アセンブリで対応する工具種名を`SimpleToolSpec::Cutter`に変換する
/// @param text `"ball"` / `"square"` / `"radius"` (大文字小文字を区別する)
/// @return 対応する種類. 未知の文字列なら`std::nullopt`
std::optional<SimpleToolSpec::Cutter> ParseSimpleCutter(std::string_view text);

/// @brief 簡易アセンブリで対応する工具種名 (TOMLで用いる文字列) を得る
std::string_view SimpleCutterName(SimpleToolSpec::Cutter cutter);

/// @brief 簡易アセンブリで対応する指令点位置名を
///        `SimpleToolSpec::CommandPoint`に変換する
/// @param text `"tip"` / `"center"` (大文字小文字を区別する)
/// @return 対応する位置. 未知の文字列なら`std::nullopt`
std::optional<SimpleToolSpec::CommandPoint>
ParseSimpleCommandPoint(std::string_view text);

/// @brief 簡易アセンブリで対応する指令点位置名 (TOMLで用いる文字列) を得る
std::string_view SimpleCommandPointName(SimpleToolSpec::CommandPoint point);

/// @brief 簡易アセンブリから工具輪郭を生成する
/// @param spec 寸法
/// @param warnings 警告の追加先 (nullptrなら追加しない)
/// @return 切れ刃・シャンク (`tool_length > cutting_length`のときのみ)・ホルダの
///         要素を持つ輪郭. 指令点はボールかつ`kCenter`のとき`diameter / 2`、
///         それ以外は0. ゲージライン (`gauge_line_z`) は`overhang + holder_length`
///         (簡易アセンブリではホルダ上端面をゲージラインとする)
/// @throw std::invalid_argument 幾何が成立しない場合: 長さが正でない,
///        ボールで`cutting_length < diameter / 2`、ラジアスで`corner_radius`が
///        `(0, diameter / 2)`の外または`cutting_length < corner_radius`,
///        `cutting_length > tool_length`、ボール以外で`kCenter`
/// @note `overhang < cutting_length` (ホルダが切れ刃に被る) は警告を出す.
///       シャンクは`cutting_length`から`tool_length`までの全長を持ち、
///       ホルダの内部でも打ち切らない
ToolProfile MakeSimpleToolProfile(const SimpleToolSpec& spec,
                                  std::vector<Diagnostic>* warnings);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_PROFILE_H_
