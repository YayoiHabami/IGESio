/**
 * @file extensions/machines/toolpath/nc_interpreter.h
 * @brief NCプログラムから`ClProgram`への変換
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note モーダル状態 (`NcState`) を保ちながらブロックを順に処理し、動作レコードと
 *       状態レコードの列 (`ClProgram`) を生成する. 対応するコードは以下.
 *       (1) 動作: G00/G01/G02/G03、平面: G17/G18/G19、絶対/増分: G90/G91,
 *           送り: G94/G95 (G95は警告)、単位: G20/G21
 *       (2) 工具長補正: G43 H / G44 (警告) / G49 / G43.4 H / G43.5 H
 *       (3) ワークオフセット: G54〜G59、G54.1 P
 *       (4) 傾斜面: G68.2 X Y Z I J K、G53.1、G69
 *       (5) 非モーダル: G04 P/X、G28 (G30は警告)、G53、G92 (警告)
 *       (6) T / M06、S / M03 / M04 / M05、M08 / M09、M98 P [L/K] / M99,
 *           M30 / M02、M00 / M01 (`ClPassThrough`)、H/D単独
 *       未対応のG (G41/G42/G65/固定サイクル等) と未知のMは警告して
 *       `ClPassThrough`にする.
 * @note G02/G03を`ClArc`にするのはTCP有効 (G43.5/G68.2) の場合のみ
 *       (`ClArc`はワーク座標系における円弧を表現する構造体であるため).
 *       TCP無効 (G43/G49) での円弧は登録値相対の座標語なので、弦誤差0.05 mmで
 *       座標語の`ClGoto`列に変換する (動作生成が直線ブロックと同様に扱う).
 *       また、`ClArc`は回転軸指令値を保持できないため、G43.4中の円弧も
 *       警告して制御点の`ClGoto`列に変換する.
 * @note 用語は以下のとおり.
 *       (1) TCP: 工具先端点制御. 形式は`NcState::Tcp`,
 *       (2) 特徴座標系: G68.2で定義する傾斜面の座標系.
 *           特徴座標→ワーク座標の同次変換を`NcState::feature_frame`に保持する
 * @note 回転軸の角度指令とG68.2のIJKはdegとして読み`kDegreeToRadian`で換算する.
 *       G43.5のIJKは方向ベクトルなので換算しない. F指令は毎分なので
 *       `kSecondsPerMinute`で割る. G20では座標、F、IJK、Rに`kInchToMillimeter`を
 *       乗じる.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_INTERPRETER_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_INTERPRETER_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"

namespace igesio::extensions::machines {

/// @brief NC変換のモーダル状態
/// @note `[[program]]`の配列順に引き継ぐ. GUIのモーダル表示にも用いる
struct NcState {
    /// @brief 工具先端点制御の形式
    enum class Tcp {
        /// @brief 無効 (G49)
        /// @note XYZは登録値相対のNC指令値
        kOff,
        /// @brief 工具軸方向 (G43.5)
        /// @note XYZは制御点、IJKは工具軸方向
        kVector,
        /// @brief 回転軸の指令 (G43.4)
        /// @note XYZは制御点、回転軸はNC指令値
        kRotaryWords,
        /// @brief 傾斜面 (G68.2 + G53.1)
        /// @note XYZは特徴座標、工具軸は特徴座標系のz軸
        kTiltedPlane,
    };

    /// @brief 動作モード (G00〜G03)
    MotionKind motion = MotionKind::kRapid;
    /// @brief 動作モードが指定済みか
    /// @note 未指定の場合は座標語があっても移動レコードを生成しない
    bool motion_set = false;
    /// @brief 円弧の平面
    ArcPlane plane = ArcPlane::kXY;
    /// @brief 絶対指令 (G90) か
    bool absolute = true;
    /// @brief mm単位 (G21) か
    /// @note G20の場合は長さに`kInchToMillimeter`を乗じる
    bool metric = true;
    /// @brief 毎分送り (G94) か
    bool feed_per_minute = true;
    /// @brief 送り速度 [mm/s]
    std::optional<double> feed;
    /// @brief 主軸の状態
    /// @note S単独のブロックは回転数のみ更新する
    ClSpindle spindle;
    /// @brief 工具先端点制御の形式
    Tcp tcp = Tcp::kOff;
    /// @brief 工具長補正が有効か (G43/G43.4/G43.5で有効、G49で無効)
    /// @note 有効かつ`h_number`があるときの補正番号が`ClLengthOffset`の値になる
    bool length_comp = false;
    /// @brief 工具長補正番号 (G43系のH、またはH単独)
    std::optional<int> h_number;
    /// @brief 工具径補正番号 (D単独. 保持のみ)
    std::optional<int> d_number;
    /// @brief 直前の座標語の値 (G91の基準)
    /// @note 座標系はTCPの形式による. `kVector`/`kRotaryWords`ではワーク座標の
    ///       制御点、`kTiltedPlane`では特徴座標の制御点、`kOff`では登録値相対の
    ///       直進軸の指令. G53ブロックでは更新しない
    igesio::Vector3d position_work = igesio::Vector3d::Zero();
    /// @brief 直前のブロックでTCPの形式が変わったか
    /// @note 次のG91増分の基準が曖昧になるので警告する
    bool tcp_changed = false;
    /// @brief 直前の工具軸方向 (ワーク座標)
    /// @note `kVector`ではIJK、`kTiltedPlane`では特徴座標系のz軸
    std::optional<igesio::Vector3d> tool_axis_work;
    /// @brief 回転軸のモーダル値 [rad]
    /// @note 軸名は`rotary_address_to_register`で変換済み
    NcValues rotary_values;
    /// @brief G68.2の特徴座標系 (特徴座標→ワーク座標の同次変換)
    std::optional<igesio::Matrix4d> feature_frame;
    /// @brief 選択中の工具番号 (`kNoTool`の場合は工具なし)
    int tool_number = kNoTool;
    /// @brief Tコードで指定され、M06を待っている工具番号
    std::optional<int> pending_tool;
    /// @brief 選択中のワークオフセットid
    std::string work_offset_id = "G54";
    /// @brief M30/M02を読んだか
    bool ended = false;
};

/// @brief NC変換の設定
struct NcInterpretOptions {
    /// @brief 制御装置の定義
    /// @note アドレス表、無効化コード、デフォルト、ワーク座標系の選択コードを参照する
    NcDialect dialect;
    /// @brief 同一ファイルに無いO番号のサブプログラムを取得する関数
    /// @note 引数はO番号. 未設定または`std::nullopt`を返した場合は
    ///       `DataFormatError`を送出する
    std::function<std::optional<std::string>(int)> subprogram_loader;
    /// @brief サブプログラムの最大深さ
    int max_subprogram_depth = 8;
    /// @brief `SourceLocation::program_index`に設定する値
    int program_index = 0;
    /// @brief `( … )`と`;`のコメントを`ClComment`にする
    bool keep_comments = true;
    /// @brief 未知のG/Mを`ClPassThrough`にする
    /// @note `false`の場合は警告と集計のみ
    bool keep_unknown_words = true;
};

/// @brief NCプログラムの文字列を`ClProgram`に変換する
/// @param text NCプログラム
/// @param lex_options 字句解析の設定 (行範囲、ブロックスキップ)
/// @param options 変換の設定
/// @param[in,out] state モーダル状態
///                (`nullptr`なら`options.dialect`のデフォルトで開始し破棄する)
/// @return CLプログラム. 警告は`ClProgram::warnings`
///         (`context`は空、`line`は行番号)
/// @note `state`の扱いは以下.
///       (1) `dialect.reset_modal_at_program_start`なら冒頭でデフォルトに戻す
///           (`tool_number`と`work_offset_id`は保つ)
///       (2) `ended`と`pending_tool`は常にリセットする
///       (3) 先頭に`state`の工具番号、ワークオフセット、長さ補正 (有効時のみ),
///           送り (有効時のみ) を`ClLoadTool`/`ClSelectWorkOffset`/
///           `ClLengthOffset`/`ClFeed`として`line = 0`で生成し、単独でも
///           `ClState`が定まるようにする
///       未知のMコードは情報診断1件に集計する
/// @throw igesio::DataFormatError 無効化されたコードがある, `kVector`でI/J/Kの
///        中心指定がある, 円弧にI/J/KもRも無い, Rと始点==終点, Rが弦の半分より
///        小さい, G43.5のIJKがゼロベクトル, サブプログラムが未定義,
///        深さ超過, または数値に不備がある場合
ClProgram InterpretNc(const std::string& text, const NcLexOptions& lex_options,
                      const NcInterpretOptions& options, NcState* state);

/// @brief NCファイルを`ClProgram`に変換する
/// @param path ファイルのパス
/// @param lex_options 字句解析の設定
/// @param options 変換の設定
/// @param[in,out] state モーダル状態 (`InterpretNc`と同じ)
/// @return CLプログラム (`InterpretNc`と同じ)
/// @throw igesio::FileOpenError ファイルを開けない場合
/// @throw igesio::DataFormatError `InterpretNc`と同じ
ClProgram InterpretNcFile(const std::filesystem::path& path,
                          const NcLexOptions& lex_options,
                          const NcInterpretOptions& options, NcState* state);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_INTERPRETER_H_
