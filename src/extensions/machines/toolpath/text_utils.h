/**
 * @file extensions/machines/toolpath/text_utils.h
 * @brief NC/CLの読み書きで共用する文字列と診断関連の関数群 (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note `cl_io.cpp`/`nc_block.cpp`/`nc_dialect.cpp`等が共用する. 文字の判定は
 *       ASCIIのみを対象とし、入力はバイト列のまま扱う. 公開ヘッダには含めない
 */
#ifndef SRC_EXTENSIONS_MACHINES_TOOLPATH_TEXT_UTILS_H_
#define SRC_EXTENSIONS_MACHINES_TOOLPATH_TEXT_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"

namespace igesio::extensions::machines::detail {

/// @brief 空白か
/// @param c 判定する文字
bool IsSpace(char c);

/// @brief 数字か
/// @param c 判定する文字
bool IsDigit(char c);

/// @brief 英字か
/// @param c 判定する文字
bool IsAlpha(char c);

/// @brief 前後の空白を除去する
/// @param text 対象の文字列
/// @return 空白を除いた部分 (元の文字列を参照する)
std::string_view Trim(std::string_view text);

/// @brief 大文字に変換する
/// @param text 対象の文字列
/// @return 英字を大文字にした文字列
std::string ToUpper(std::string_view text);

/// @brief 警告/情報を追加する
/// @param[out] diagnostics 追記先 (`nullptr`なら何もしない)
/// @param severity 重大度
/// @param message 内容
/// @param line 行番号 (不明なら0)
/// @note `context`は空にする (NC/CLでは行番号を`line`に格納する)
void PushDiagnostic(std::vector<Diagnostic>* diagnostics, Severity severity,
                    std::string message, int line = 0);

/// @brief 警告を追加する
/// @param[out] diagnostics 追記先 (`nullptr`なら何もしない)
/// @param message 内容
/// @param line 行番号 (不明なら0)
/// @note `PushDiagnostic`の`Severity::kWarning`固定
void PushWarning(std::vector<Diagnostic>* diagnostics, std::string message,
                 int line = 0);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_TOOLPATH_TEXT_UTILS_H_
