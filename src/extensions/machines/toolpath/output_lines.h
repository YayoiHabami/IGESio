/**
 * @file extensions/machines/toolpath/output_lines.h
 * @brief 出力行の連結とレコードの出力行の対応付け（'内部ヘッダ）
 * @author Yayoi Habami
 * @date 2026-10-26
 * @copyright 2026 Yayoi Habami
 * @note `cl_io.cpp`/`nc_writer.cpp`の共通処理. 両ライタークラスが記録する
 *       行要素（各要素の末尾に改行を付けて連結する文字列）を物理行に対応付ける.
 *       コメント本文、テンプレート展開結果など、要素内に改行を含む場合も,
 *       連結した文字列上の行番号と一致させる.
 */
#ifndef SRC_EXTENSIONS_MACHINES_TOOLPATH_OUTPUT_LINES_H_
#define SRC_EXTENSIONS_MACHINES_TOOLPATH_OUTPUT_LINES_H_

#include <cstddef>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines::detail {

/// @brief 行要素のインデックスの半開区間 [begin, end)
struct ElementRange {
    /// @brief 先頭の要素のインデックス
    std::size_t begin = 0;
    /// @brief 末尾の次の要素のインデックス（`begin`と等しければ空）
    std::size_t end = 0;
};

/// @brief 行要素を改行で連結し、要素の区間を物理行の範囲に変換する
/// @param elements 行要素（各要素の末尾に改行を付けて連結する）
/// @param ranges レコードごとの要素の区間（`end <= elements.size()`であること）
/// @param newline 改行の種類
/// @return 連結した文字列と、`ranges`と同じ長さの物理行の範囲
/// @note 物理行は`'\n'`の数で数える. `"\r\n"`は1行、単独の`'\r'`は改行としない.
///       空の区間は、直後の要素の先頭行を`first`とする
/// @throw std::out_of_range `begin > end`または`end > elements.size()`の区間が
///        ある場合
ProgramText JoinWithLineRanges(const std::vector<std::string>& elements,
                               const std::vector<ElementRange>& ranges,
                               NewlineStyle newline);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_TOOLPATH_OUTPUT_LINES_H_
