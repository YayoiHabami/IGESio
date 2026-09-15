/**
 * @file extensions/machines/toolpath/nc_modal.h
 * @brief NC変換の内部構造 (ブロックのワードの分類、変換の作業状態、グループ別処理)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note `nc_interpreter.cpp` (ブロックの走査、サブプログラム、初期レコード) と
 *       `nc_modal.cpp` (Gコードのグループ別処理、Mコード、移動レコードの生成) が
 *       共有する内部ヘッダ. 公開ヘッダからはincludeしない.
 * @note 1ブロックの処理順は以下.
 *       (1) 無効化コードの検査,
 *       (2) コメント,
 *       (3) 単位、絶対/増分、平面、送りモード,
 *       (4) ワークオフセット,
 *       (5) 工具長補正,
 *       (6) 傾斜面,
 *       (7) 非モーダル (G04/G28/G30/G53/G92),
 *       (8) 動作コード,
 *       (9) F/S/T,
 *       (10) M (M98/M99/M30/M02以外),
 *       (11) 移動レコード,
 *       (12) M98/M99/M30/M02
 * @note G/Mコードの正規化した名前 (`"G1"` / `"G43.5"` / `"M6"`) は
 *       `FormatNcWord`の出力と`NormalizeCodeName`の出力が一致することを前提に,
 *       無効化コードの比較に用いる.
 */
#ifndef SRC_EXTENSIONS_MACHINES_TOOLPATH_NC_MODAL_H_
#define SRC_EXTENSIONS_MACHINES_TOOLPATH_NC_MODAL_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"
#include "igesio/extensions/machines/toolpath/nc_interpreter.h"

namespace igesio::extensions::machines::detail {

/// @brief 1ブロックのワードの分類
struct BlockWords {
    /// @brief 元ファイルの行番号
    int line = 0;
    /// @brief Gコード (正規化した名前. 出現順)
    std::vector<std::string> g_codes;
    /// @brief Mコード (正規化した名前. 出現順)
    std::vector<std::string> m_codes;
    /// @brief G/M以外のワード (同じアドレスは最後の値)
    std::map<char, double> values;
    /// @brief コメント除去後の本文
    std::string text;
    /// @brief コメント本文
    std::vector<std::string> comments;

    /// @brief アドレスのワードがあるか
    /// @param address アドレス (大文字)
    bool Has(char address) const { return values.count(address) > 0; }
    /// @brief アドレスのワードの値を取得する
    /// @param address アドレス (大文字)
    /// @return ワードの値. 無ければ`std::nullopt`
    std::optional<double> Get(char address) const;
    /// @brief Gコードを含むか
    /// @param code 正規化した名前 (`"G43.5"`等)
    bool HasG(const std::string& code) const;
};

/// @brief ブロックのワードを分類する
/// @param block 字句解析済みのブロック
/// @return G/Mコードとその他のワードに分けた結果
BlockWords ClassifyBlock(const NcBlock& block);

/// @brief 変換の作業状態 (1回の`InterpretNc`の間だけ有効)
struct InterpretContext {
    /// @brief 変換の設定
    const NcInterpretOptions& options;
    /// @brief モーダル状態
    NcState& state;
    /// @brief 出力先のプログラム
    ClProgram& program;
    /// @brief 正規化した無効化コード
    std::set<std::string> disabled;
    /// @brief 出力済みレコードを適用した状態 (状態レコードの重複を避けるため)
    ClState emitted;
    /// @brief 未知のMコードの集計 (コード → 回数)
    std::map<int, int> unknown_m_codes;

    /// @brief レコードを出力する
    /// @param record 出力するレコード
    /// @param line 元ファイルの行番号 (出所の`program_index`は`options`の値)
    void Emit(ClRecord record, int line);
    /// @brief 警告を追加する (`context`は空)
    /// @param message 内容
    /// @param line 元ファイルの行番号
    void Warn(const std::string& message, int line);
};

/// @brief ブロックの処理で決まる移動レコードの生成条件
struct MotionFlags {
    /// @brief このブロックの移動を生成しない
    /// @note G04/G28/G68.2等が座標語を読み取った場合に`true`
    bool skip_motion = false;
    /// @brief このブロックのみ機械座標 (G53)
    bool machine_frame = false;
};

/// @brief ブロック末尾で処理するMコードの結果
struct BlockControl {
    /// @brief M30/M02/主プログラムのM99
    bool end = false;
    /// @brief M99 (サブプログラムからの復帰)
    bool return_from_sub = false;
    /// @brief M98のP番号
    std::optional<int> call_program;
    /// @brief M98の繰り返し回数 (L/K. 省略時1)
    int call_count = 1;
};

/// @brief 無効化されたコードを検査する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @throw igesio::DataFormatError 無効化されたG/Mコードを含む場合
void CheckDisabledCodes(const InterpretContext& context, const BlockWords& words);

/// @brief コメントを`ClComment`として出力する (`keep_comments`のとき)
/// @param context 変換の作業状態
/// @param words ブロックのワード
void EmitComments(InterpretContext& context, const BlockWords& words);

/// @brief 単位、絶対/増分、平面、送りモード (G20/G21/G90/G91/G17〜G19/G94/G95) を
///        処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
void ProcessModes(InterpretContext& context, const BlockWords& words);

/// @brief ワークオフセット (G54〜G59/G54.1 P) を処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
void ProcessWorkOffset(InterpretContext& context, const BlockWords& words);

/// @brief 工具長補正 (G43/G44/G49/G43.4/G43.5、H/D単独) を処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
void ProcessLengthOffset(InterpretContext& context, const BlockWords& words);

/// @brief 傾斜面 (G68.2/G53.1/G69) を処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @return G68.2が座標語を読み取った場合は`skip_motion`を`true`にした条件
MotionFlags ProcessTiltedPlane(InterpretContext& context, const BlockWords& words);

/// @brief 非モーダル (G04/G28/G30/G53/G92) を処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @param flags これまでの処理で決まった条件
/// @return `flags`を更新した条件
/// @throw igesio::DataFormatError G28/G30の中間点への移動に不備がある場合
///        (`ProcessMotion`から伝播)
MotionFlags ProcessNonModal(InterpretContext& context, const BlockWords& words,
                            MotionFlags flags);

/// @brief 動作コード (G00〜G03) と未対応のGを処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
void ProcessMotionCode(InterpretContext& context, const BlockWords& words);

/// @brief F/S/Tコードを処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
void ProcessFeedSpindleTool(InterpretContext& context, const BlockWords& words);

/// @brief Mコードのうちブロック内で処理するもの (M03〜M09/M06/M00/M01/未知) を
///        処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @note M98/M99/M30/M02は`ProcessBlockEnd`で処理する
void ProcessMCodes(InterpretContext& context, const BlockWords& words);

/// @brief 状態レコード (工具、ワークオフセット、長さ補正、送り) の変化を出力する
/// @param context 変換の作業状態
/// @param line 元ファイルの行番号
void SyncStateRecords(InterpretContext& context, int line);

/// @brief 移動レコード (`ClGoto`/`ClArc`) を生成する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @param flags 移動レコードの生成条件
/// @throw igesio::DataFormatError G43.5のIJKがゼロベクトル、または円弧の中心
///        指定に不備がある場合
void ProcessMotion(InterpretContext& context, const BlockWords& words,
                   const MotionFlags& flags);

/// @brief ブロック末尾のMコード (M98/M99/M30/M02) を処理する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @return 終了/復帰/サブプログラム呼び出しの指示
BlockControl ProcessBlockEnd(InterpretContext& context, const BlockWords& words);

/// @brief 未知のMコードだけを持つブロックか
/// @param context 変換の作業状態 (制御装置定義の軸アドレス表を参照する)
/// @param words ブロックのワード
/// @note `true`の場合は本文全体を`ClPassThrough`にする
bool IsUnknownMOnlyBlock(const InterpretContext& context, const BlockWords& words);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_TOOLPATH_NC_MODAL_H_
