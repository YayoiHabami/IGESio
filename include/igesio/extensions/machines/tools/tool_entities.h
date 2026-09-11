/**
 * @file extensions/machines/tools/tool_entities.h
 * @brief 工具・ホルダのエンティティ化 (Assemblyによるツリー構造の構築)
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note 工具輪郭の各要素を回転曲面 (Type 120) として作成し、以下の構造のAssemblyを
 *       構築する. `cutter`, `shank`, `holder`はそれぞれ1つ以上の回転曲面を
 *       まとめるアセンブリであり、親に対する変換を持たない. `cutter0`や`shank0`等は
 *       いずれも一つの部位要素 (閉じた輪郭線) に対応する回転曲面エンティティであり,
 *       親に対する変換としてR_x(+90°)を持つ.
 *       ```text
 *       tool:<number>          (最上位アセンブリ. 取り付け側が変換行列を与える)
 *       ├─ cutter              (切れ刃部に対応するアセンブリ)
 *       │   ├─ cutter0
 *       │   └─ cutter1 …
 *       ├─ shank               (シャンク部に対応するアセンブリ)
 *       │   └─ shank0 …
 *       └─ holder              (ホルダ部に対応するアセンブリ)
 *           ├─ holder0
 *           └─ holder1 …
 *       ```
 *       部位要素が存在しない部位のアセンブリは作らない. 各部位のノードの番号は
 *       部位内における出現順.
 * @note IGES5.3の円弧はXY平面上でしか定義できないため、母線はXY平面 (x = r, y = z)
 *       で定義し、Y軸を回転軸とする回転体を作成した後、要素ノードの変換R_x(+90°)で
 *       Yを工具軸 (+z) へ写す. 直線のみの要素はType 106 (Form 11)、円弧を含む要素は
 *       Type 100/110をType 102で1本にした母線を持つ.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_ENTITIES_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_ENTITIES_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "igesio/models/assembly.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"

namespace igesio::extensions::machines {

/// @brief 工具・ホルダアセンブリのルートの名前の接頭辞 (`"tool:<number>"`)
constexpr std::string_view kToolAssemblyPrefix = "tool:";

/// @brief 工具・ホルダアセンブリのルートの名前を作成する
/// @param number 工具番号 (NCのTで指定する番号に対応)
std::string ToolAssemblyName(int number);

/// @brief 部位要素のエンティティの名前を作成する
/// @param part 工具・ホルダの部位種別
/// @param index 部位内での要素の番号 (0始まり)
/// @return ノード名 (`"cutter0"` / `"holder1"` 等)
std::string ToolElementName(ToolPart part, std::size_t index);

/// @brief 工具輪郭を実体化する
/// @param spec 工具アセンブリの定義
/// @return 工具・ホルダアセンブリ.
/// @throw std::invalid_argument `ValidateToolProfile`の違反
/// @note 戻り値の最上位アセンブリの変換は単位行列であるため、取り付けオフセット
///       (`ToolMountOffset`) や位置/姿勢の行列は呼び出し側が設定すること
/// @note 各要素の回転曲面は要素の色 (無ければ部位のデフォルト色) を参照し、
///       不透明度が1未満なら要素ノードに不透明度オーバーライドを設定する
std::shared_ptr<models::Assembly> MakeToolAssembly(const ToolAssemblySpec& spec);

/// @brief 切れ刃部/シャンク部/ホルダ部のアセンブリを取得する
/// @param tool 工具・ホルダアセンブリ
/// @param part 部位
/// @return 指定した部位のアセンブリ. 無ければ`nullptr`
std::shared_ptr<models::Assembly>
FindToolPart(const models::Assembly& tool, ToolPart part);

/// @brief 指定部位の表示状態 (`SetVisible`) を切り替える
/// @param tool 工具・ホルダアセンブリ
/// @param part 部位
/// @param visible 表示するならtrue
/// @note 指定した部位のアセンブリが無ければ何もしない
void SetToolPartVisible(models::Assembly& tool, ToolPart part, bool visible);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_ENTITIES_H_
