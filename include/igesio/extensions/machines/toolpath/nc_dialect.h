/**
 * @file extensions/machines/toolpath/nc_dialect.h
 * @brief 制御装置の定義 (コード名、構文、アドレス表、テンプレート行)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note プロジェクトフォーマット仕様の、制御装置定義に対応する構造 (仕様未定義).
 *       機種名によるコードの分岐は置かず、機種差 (「方言」のような、機種ごとで
 *       使用可能なワード) は本型のデータで表す.
 *       仕様決定時に`nc_dialect_io.h`の追加だけで済むよう、公開メンバは全て値型にする.
 * @note 読込 (`nc_interpreter.h`) のコード表は固定で、本型からはアドレス表,
 *       無効化コード、デフォルトのモーダル状態、ワーク座標系の選択コードの対応のみを
 *       参照する. コード名、構文、テンプレート行は出力 (`nc_writer.h`) で用いる.
 * @note ワーク座標系の選択コードの「標準の形式」は`"G54"`〜`"G59"`および
 *       `"G54.1P<n>"` (nは先頭が0でない整数) を指す.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_DIALECT_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_DIALECT_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"

namespace igesio::extensions::machines {

/// @brief 円弧の平面 (G17 / G18 / G19)
enum class ArcPlane {
    /// @brief XY平面 (G17. 中心はI, J、法線は+Z)
    kXY,
    /// @brief ZX平面 (G18. 中心はK, I、法線は+Y)
    kZX,
    /// @brief YZ平面 (G19. 中心はJ, K、法線は+X)
    kYZ,
};

/// @brief 出力で使うコード名
/// @note 読込では`nc_interpreter.h`の固定表を用い、本構造体を参照しない
struct NcVocabulary {
    /// @brief 早送り
    std::string rapid = "G00";
    /// @brief 直線切削
    std::string linear = "G01";
    /// @brief 時計回りの円弧
    std::string arc_cw = "G02";
    /// @brief 反時計回りの円弧
    std::string arc_ccw = "G03";
    /// @brief XY平面
    std::string plane_xy = "G17";
    /// @brief ZX平面
    std::string plane_zx = "G18";
    /// @brief YZ平面
    std::string plane_yz = "G19";
    /// @brief 絶対指令
    std::string absolute = "G90";
    /// @brief 増分指令
    std::string incremental = "G91";
    /// @brief mm単位
    std::string metric = "G21";
    /// @brief inch単位
    std::string inch = "G20";
    /// @brief 毎分送り
    std::string feed_per_minute = "G94";
    /// @brief 工具長補正 (TCPなし)
    std::string length_comp = "G43";
    /// @brief 工具先端点制御 (回転軸の指令)
    std::string tcp_rotary = "G43.4";
    /// @brief 工具先端点制御 (工具軸方向)
    std::string tcp_vector = "G43.5";
    /// @brief 工具長補正の解除
    std::string cancel_comp = "G49";
    /// @brief 機械座標の移動 (非モーダル)
    std::string machine_coordinates = "G53";
    /// @brief 主軸正転
    std::string spindle_cw = "M03";
    /// @brief 主軸逆転
    std::string spindle_ccw = "M04";
    /// @brief 主軸停止
    std::string spindle_off = "M05";
    /// @brief クーラント入
    std::string coolant_on = "M08";
    /// @brief クーラント切
    std::string coolant_off = "M09";
    /// @brief 工具交換
    std::string tool_change = "M06";
    /// @brief プログラム終了
    std::string program_end = "M30";
    /// @brief オプショナルストップ
    std::string optional_stop = "M01";
    /// @brief ドウェル
    std::string dwell = "G04";
    /// @brief ドウェル時間のアドレス
    char dwell_address = 'P';
    /// @brief 秒からドウェル時間の値への係数 (Pはms)
    double dwell_scale = 1000.0;
};

/// @brief 字句の規約
/// @note 出力のデフォルト. `NcWriteOptions::syntax`で置き換えられる
struct NcSyntax {
    /// @brief 先頭と末尾の`%`
    bool percent = true;
    /// @brief プログラム番号の接頭辞 (`"O"`)
    std::string program_prefix = "O";
    /// @brief プログラム番号の桁数 (`"O0001"`)
    int program_digits = 4;
    /// @brief コメントの開始 (行末コメント形式なら`";"`)
    std::string comment_open = "(";
    /// @brief コメントの終了 (行末コメント形式なら空)
    std::string comment_close = ")";
    /// @brief N番号の刻み (`std::nullopt` = 付けない)
    std::optional<int> line_number_step;
    /// @brief N番号の桁数
    int line_number_digits = 1;
    /// @brief ワードの間の区切り (`""`または`" "`)
    std::string word_separator;
    /// @brief 座標語の書式
    NcNumberFormat coordinate{4};
    /// @brief IJK指令 (工具軸方向、円弧中心) の書式
    NcNumberFormat vector{6};
    /// @brief Fコードの書式 (整数、小数点なし)
    NcNumberFormat feed{0, false, false};
    /// @brief 回転軸の角度指令の書式 (deg)
    NcNumberFormat angle{3};
};

/// @brief 制御装置の定義
struct NcDialect {
    /// @brief 表示名 (`"fanuc"` / `"makino-d200z"`等)
    std::string name;
    /// @brief 出力で使うコード名
    NcVocabulary vocab;
    /// @brief 字句の規約
    NcSyntax syntax;
    /// @brief 回転軸の軸アドレス → 機械定義の軸名
    /// @note 表に無いアドレスのワードは読込時に警告して無視する
    std::map<char, std::string> rotary_address_to_register = {
            {'A', "A"}, {'B', "B"}, {'C', "C"}};
    /// @brief 直進軸の軸アドレス → 機械定義の軸名
    std::map<char, std::string> linear_address_to_register = {
            {'X', "X"}, {'Y', "Y"}, {'Z', "Z"}};
    /// @brief 無効化するコード (`"G68.2"`等)
    /// @note `NormalizeCodeName`で正規化して比較する
    std::set<std::string> disabled_codes;
    /// @brief 正規化したワーク座標系の選択コード (`"G54"` / `"G54.1P2"`) → `[[work_offset]].id`
    /// @note 表に無いコードは、標準の形式の文字列をそのままidにする
    std::map<std::string, std::string> work_offset_ids;
    /// @brief モーダル状態のデフォルト (電源投入時/プログラム開始時)
    struct Defaults {
        /// @brief 絶対指令 (G90)
        bool absolute = true;
        /// @brief 円弧の平面
        ArcPlane plane = ArcPlane::kXY;
        /// @brief mm単位 (G21)
        bool metric = true;
        /// @brief 毎分送り (G94)
        bool feed_per_minute = true;
    };
    /// @brief モーダル状態のデフォルト
    Defaults defaults;
    /// @brief プログラム開始時にモーダル状態をデフォルトに戻す
    /// @note 工具番号とワークオフセットidは戻さない
    bool reset_modal_at_program_start = false;
    /// @brief ヘッダのテンプレート行
    /// @note `{変数名}`を展開する. 変数は`program` (O番号)、`name` (表示名),
    ///       `date` (出力日 `YYYY-MM-DD`)、`tool` (工具番号)、`h` (補正番号),
    ///       `spindle` (回転数)、`feed` (毎分送り)、`work_offset` (id).
    ///       ヘッダでは、まだ現れていない値をプログラム中の最初の出現で補う
    std::vector<std::string> header;
    /// @brief フッタのテンプレート行
    /// @note 変数は`header`と同じで、プログラム末尾の値を用いる
    std::vector<std::string> footer;
    /// @brief 工具交換のテンプレート行
    /// @note 空なら`T<n> M06`を出力する. 変数は`header`と同じで、`tool`は
    ///       選択する工具番号になる
    std::vector<std::string> tool_change;
};

/// @brief Fanuc系のデフォルトの制御装置定義を作成する
/// @return コード名と構文は`NcVocabulary`/`NcSyntax`のデフォルト値、テンプレートは空
NcDialect DefaultFanucDialect();

/// @brief コード名を正規化した形式に変換する
/// @param text コード名 (`"G68.2"` / `"g68.20"` / `"G068.2"` / `"M06"`)
/// @return 大文字のアドレス + 先頭の0を除いた整数部 + (小数部があれば) `.` + 末尾の0を
///         除いた小数部 (`"G68.2"` / `"M6"`). 数値でなければ大文字化のみ
/// @note 前後の空白は除去する
std::string NormalizeCodeName(std::string_view text);

/// @brief テンプレート行の変数 (`{変数名}`) を展開する
/// @param line テンプレート行
/// @param variables 変数名 → 値
/// @param[out] warnings 未知の変数 (空文字にする) の警告 (`nullptr`なら報告しない)
/// @return 展開した行
/// @note 閉じられていない`{`はそのまま残す
std::string ExpandTemplate(std::string_view line,
                           const std::map<std::string, std::string>& variables,
                           std::vector<Diagnostic>* warnings);

/// @brief ワーク座標系の選択コードをidに変換する
/// @param dialect 制御装置の定義
/// @param code 正規化したGコード (`"G54"`〜`"G59"`、`"G54.1"`)
/// @param p `G54.1`のP番号 (無ければ`std::nullopt`)
/// @return `work_offset_ids`にあればそのid、無ければ標準の形式 (`"G54"` /
///         `"G54.1P2"`). `G54.1`でPが無ければ`"G54.1"`
std::string WorkOffsetIdFromWord(const NcDialect& dialect, std::string_view code,
                                 std::optional<int> p);

/// @brief ワークオフセットidをワーク座標系の選択コードに変換する
/// @param dialect 制御装置の定義
/// @param id ワークオフセットid
/// @return `work_offset_ids`で`id`に対応するコード. 無ければ、`id`が標準の形式なら
///         そのまま. どちらでもなければ`std::nullopt` (出力側でコメント化する)
std::optional<std::string> WorkOffsetWordFromId(const NcDialect& dialect,
                                                std::string_view id);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_DIALECT_H_
