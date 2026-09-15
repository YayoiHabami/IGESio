/**
 * @file tests/extensions/machines/toolpath/dialects_for_testing.h
 * @brief machines拡張のテストで共有する制御装置の方言のフィクスチャ
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 機種ごとの方言はライブラリには置かず (将来はTOMLの制御装置定義にする),
 *       テストの期待出力 (`nc/expected_d200z.nc`) の基準としてここに置く.
 */
#ifndef TESTS_EXTENSIONS_MACHINES_TOOLPATH_DIALECTS_FOR_TESTING_H_
#define TESTS_EXTENSIONS_MACHINES_TOOLPATH_DIALECTS_FOR_TESTING_H_

#include <string>
#include <vector>

#include "igesio/extensions/machines/toolpath/nc_block.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"

namespace toolpath_test {

/// @brief Makino D200Z向けの方言 (C-SpaceCAM現行の出力に相当)
/// @note Fanuc系のデフォルトをもとに、以下を設定する.
///       (1) ヘッダ: `G91G28Z0` (Z軸を機械原点に退避)、`G28X0Y0` (XYを機械原点に),
///           `G49` (工具長補正の解除)、`G90` (絶対指令)
///       (2) 工具交換: `T{tool}M06` (工具番号を展開)
///       (3) フッタ: `G91G28Z0` (Z軸退避)、`M05` (主軸停止)
///       (4) 構文: ワードの間に区切りを入れない、座標4桁/ベクトル6桁/角度3桁,
///           Fコードは整数
inline igesio::extensions::machines::NcDialect MakinoD200zDialect() {
    namespace mc = igesio::extensions::machines;
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.name = "makino-d200z";
    dialect.header = {"G91G28Z0", "G28X0Y0", "G49", "G90"};
    dialect.tool_change = {"T{tool}M06"};
    dialect.footer = {"G91G28Z0", "M05"};
    dialect.syntax.word_separator = "";
    dialect.syntax.coordinate = mc::NcNumberFormat{4};
    dialect.syntax.vector = mc::NcNumberFormat{6};
    dialect.syntax.angle = mc::NcNumberFormat{3};
    dialect.syntax.feed = mc::NcNumberFormat{0, false, false};
    return dialect;
}

}  // namespace toolpath_test

#endif  // TESTS_EXTENSIONS_MACHINES_TOOLPATH_DIALECTS_FOR_TESTING_H_
