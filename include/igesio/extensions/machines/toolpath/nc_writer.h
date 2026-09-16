/**
 * @file extensions/machines/toolpath/nc_writer.h
 * @brief ポストプロセッサ (`ClProgram` → NC)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 運動学の計算は行わない. 工具軸を回転軸の指令で出力する
 *       (`TcpStyle::kRotaryWords`) には、先に`simulation/axis_resolution.h`の
 *       `ResolveAxisWords`で`ClGoto::axis_words`に回転軸の指令を書き込んでおくこと.
 * @note 出力の並びは`%` → `O`番号 → ヘッダテンプレート → 冒頭のモーダルコード →
 *       レコード列 → フッタテンプレート → `%`. `ClEnd`でフッタと`program_end`を
 *       出力し、以降のレコードは出力しない (警告). `ClEnd`が無ければ末尾に
 *       フッタと`program_end`を補う.
 * @note 読み書きの規約: `InterpretNc(WriteNcToString(p))`が`p`の動作レコードと
 *       状態レコードを再現する (ヘッダ、フッタ、冒頭のモーダルコード無しの場合).
 *       ただし`TcpStyle::kNone`で出力したG02/G03は、読込側がTCP無効の円弧を
 *       座標語の`ClGoto`列に変換するため`ClArc`としては再現しない.
 *       (`ClArc`はワーク座標系における円弧を表現する構造体であるため,
 *       機械構成に依存してワーク座標系の円弧でなくなる可能性のあるNCでは線形補間する)
 * @note 主平面はXY/ZX/YZ平面 (`ArcPlane`) を指す. 法線がいずれかの軸に一致する
 *       円弧のみG02/G03で出力できる.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_WRITER_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_WRITER_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"

namespace igesio::extensions::machines {

/// @brief 工具軸の出力形式
enum class TcpStyle {
    /// @brief G43.5 (IJKの工具軸方向)
    kVector,
    /// @brief G43.4 (回転軸の指令)
    /// @note `axis_words`に回転軸が必要
    kRotaryWords,
    /// @brief 工具軸を出力しない (3軸)
    kNone,
};

/// @brief 円弧の出力形式
enum class ArcOutput {
    /// @brief G02/G03 (主平面上の円弧のみ)
    /// @note 主平面外の円弧は折れ線化して警告する
    kNative,
    /// @brief 常に折れ線化
    kLinearize,
};

/// @brief Fコードの出力形式
enum class FeedOutput {
    /// @brief 変化時のみ
    kOnChange,
    /// @brief 切削ブロックごとに毎回
    kEveryCutBlock,
};

/// @brief 出力の設定
/// @note 未指定の構文は`dialect.syntax`に従う
struct NcWriteOptions {
    /// @brief プログラム番号 (`O`番号)
    int program_number = 1;
    /// @brief 工具軸の出力形式
    TcpStyle tcp = TcpStyle::kVector;
    /// @brief モーダル省略 (直前と同じワードを省く)
    struct ModalSuppression {
        /// @brief G00/G01 (G02/G03/G53は常に出力する)
        bool motion_code = true;
        /// @brief 直前と同じX/Y/Z
        bool coordinates = true;
        /// @brief 直前と同じI/J/K
        /// @note TCP中は省略しないことを推奨する
        bool tool_axis = false;
        /// @brief G17/G18/G19
        bool plane = true;
    };
    /// @brief モーダル省略
    ModalSuppression suppress;
    /// @brief Fコードの出力形式
    FeedOutput feed = FeedOutput::kOnChange;
    /// @brief 円弧の出力形式
    ArcOutput arc = ArcOutput::kNative;
    /// @brief 折れ線化の弦誤差 [mm] (`kLinearize`と主平面外の円弧に使う)
    double arc_chord_tolerance = 0.05;
    /// @brief `ClComment`/`ClMarker`をコメントで出力する
    bool comments = true;
    /// @brief `PathRole`の変化をコメントで出力する (`"(Approach)"`等)
    bool role_comments = false;
    /// @brief `dialect.header`/`footer`を出力する
    bool write_header_footer = true;
    /// @brief ヘッダテンプレート (指定時は`dialect`のものを置き換える)
    std::optional<std::vector<std::string>> header;
    /// @brief フッタテンプレート (指定時は`dialect`のものを置き換える)
    std::optional<std::vector<std::string>> footer;
    /// @brief ヘッダ直後にG90/G21/G94/G17を出力する
    /// @note 制御装置定義のデフォルトと異なるものだけ出力する
    bool preamble_modal_codes = true;
    /// @brief 同じ制御装置定義 (`"nc"`) の`ClPassThrough`をそのまま出力する
    /// @note `false`ならコメント化する
    bool passthrough = true;
    /// @brief 構文 (指定時は`dialect.syntax`を置き換える)
    std::optional<NcSyntax> syntax;
    /// @brief 改行の種類
    NewlineStyle newline = NewlineStyle::kLf;

    /// @brief 値の整合を検査する
    /// @return 不備の説明 (負のプログラム番号、`arc_chord_tolerance <= 0`等).
    ///         問題なければ`std::nullopt`
    /// @note 出力関数ではこの文言で`std::invalid_argument`を送出する
    std::optional<std::string> Validate() const;
};

/// @brief CLプログラムをNCプログラムに文字列化する
/// @param program 出力するプログラム
/// @param dialect 制御装置の定義
/// @param options 出力の設定
/// @param[out] warnings 警告 (`ValidateClProgram`の結果、折れ線化した円弧の件数,
///        コメント化した指令、工具軸/長さ補正の欠落、省いた円弧等. `nullptr`なら
///        報告しない)
/// @return NCプログラム
/// @note 始点が分からない、または法線がゼロベクトルの円弧は警告して省く
/// @throw std::invalid_argument `options.Validate()`が不備を返す場合
/// @throw igesio::DataFormatError `tcp = kRotaryWords`で回転軸の指令を持たない移動が
///        ある場合
std::string WriteNcToString(const ClProgram& program, const NcDialect& dialect,
                            const NcWriteOptions& options = {},
                            std::vector<Diagnostic>* warnings = nullptr);

/// @brief CLプログラムをNCファイルに出力する
/// @param path 出力先
/// @param program 出力するプログラム
/// @param dialect 制御装置の定義
/// @param options 出力の設定
/// @param[out] warnings `WriteNcToString`と同じ
/// @throw std::invalid_argument `WriteNcToString`と同じ
/// @throw igesio::DataFormatError `WriteNcToString`と同じ
/// @throw igesio::FileOpenError ファイルを開けない場合
void WriteNc(const std::filesystem::path& path, const ClProgram& program,
             const NcDialect& dialect, const NcWriteOptions& options = {},
             std::vector<Diagnostic>* warnings = nullptr);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_WRITER_H_
