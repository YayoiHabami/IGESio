/**
 * @file extensions/machines/toolpath/nc_emit.h
 * @brief NC出力のワードの組み立て (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note `nc_writer.h`の実装で用いる. レコードを順に出力する間に保つ状態
 *       (モーダル省略のための直前のワード、送りの保留、`ClState`、N番号) を
 *       `NcEmitter`にまとめ、レコード種別ごとの出力を`Emit`のオーバーロードに分ける.
 * @note 数値の整形は`nc_block.h`の`FormatNcWord`に、テンプレート行の展開は
 *       `nc_dialect.h`の`ExpandTemplate`に委ねる. 運動学は扱わない.
 */
#ifndef SRC_EXTENSIONS_MACHINES_TOOLPATH_NC_EMIT_H_
#define SRC_EXTENSIONS_MACHINES_TOOLPATH_NC_EMIT_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_writer.h"

namespace igesio::extensions::machines::detail {

/// @brief レコード列をNCの行に変換する
/// @note `BeginProgram` → 各レコードの`Emit` → `EndProgram` → `Join`の順に呼ぶ.
///       警告は構築時に受け取った配列に追記する (`nullptr`なら報告しない)
class NcEmitter {
 public:
    /// @brief 出力設定を解決して初期化する
    /// @param dialect 制御装置の定義 (出力が終わるまで有効であること)
    /// @param options 出力の設定 (同上)
    /// @param[out] warnings 警告の追記先 (`nullptr`なら報告しない)
    NcEmitter(const NcDialect& dialect, const NcWriteOptions& options,
              std::vector<Diagnostic>* warnings);

    /// @brief プログラムの冒頭 (`%`、O番号、ヘッダ、冒頭のモーダルコード) を
    ///        出力する
    /// @param program 出力するプログラム (テンプレート変数と事前検査に用いる)
    void BeginProgram(const ClProgram& program);
    /// @brief レコードを1つ出力し、モーダル状態を更新する
    /// @param record 出力するレコード
    /// @param source 出所 (無ければ`nullptr`. 警告の行番号に用いる)
    /// @throw igesio::DataFormatError `kRotaryWords`で回転軸の指令が定まらない移動が
    ///        ある場合 (`AppendTcpWords`から伝播)
    void Emit(const ClRecord& record, const SourceLocation* source);
    /// @brief 直線移動を出力する (`kMachine`なら`EmitMachineGoto`)
    /// @param record 出力するレコード
    /// @throw igesio::DataFormatError `EmitGoto`と同じ
    void Emit(const ClGoto& record);
    /// @brief 円弧を出力する (主平面上ならG02/G03、それ以外は折れ線化)
    /// @param record 出力するレコード
    /// @note 始点が分からない、または法線がゼロベクトルの場合は警告して省く
    void Emit(const ClArc& record);
    /// @brief ドウェルを出力する
    /// @param record 出力するレコード
    void Emit(const ClDwell& record);
    /// @brief 送りを出力する
    /// @param record 出力するレコード (参照しない)
    /// @note 何もしない. 送りは`state_`の更新を経て次の切削ブロックにFコードとして
    ///       付ける
    void Emit(const ClFeed& record);
    /// @brief 主軸の回転を出力する
    /// @param record 出力するレコード
    void Emit(const ClSpindle& record);
    /// @brief クーラントの入切を出力する
    /// @param record 出力するレコード
    void Emit(const ClCoolant& record);
    /// @brief 工具の選択を出力する
    /// @param record 出力するレコード
    /// @note テンプレートがあればそれ、無ければ`T<n> M06`
    void Emit(const ClLoadTool& record);
    /// @brief ワーク座標系の選択を出力する (変換できないidはコメント化)
    /// @param record 出力するレコード
    void Emit(const ClSelectWorkOffset& record);
    /// @brief 工具長補正を出力する (`tcp`に応じたコード、解除はG49)
    /// @param record 出力するレコード
    void Emit(const ClLengthOffset& record);
    /// @brief コメントを出力する
    /// @param record 出力するレコード
    void Emit(const ClComment& record);
    /// @brief 区切りをコメントで出力する
    /// @param record 出力するレコード
    void Emit(const ClMarker& record);
    /// @brief 変換せずに保持する文字列を出力する (制御装置定義が違えばコメント化)
    /// @param record 出力するレコード
    void Emit(const ClPassThrough& record);
    /// @brief 終了を出力する (フッタの後に終了コード)
    /// @param record 出力するレコード (参照しない)
    void Emit(const ClEnd& record);
    /// @brief プログラムの末尾 (フッタ、終了コード、`%`) と集計した警告を出力する
    void EndProgram();
    /// @brief 出力した行を改行で連結する
    /// @param newline 改行の種類
    /// @return 各行の末尾に改行を付けた文字列
    std::string Join(NewlineStyle newline) const;

 private:
    /// @brief `kWork`の直線移動を出力する
    /// @param record 出力するレコード
    /// @throw igesio::DataFormatError `kRotaryWords`で回転軸の指令が定まらない場合
    ///        (`AppendTcpWords`から伝播)
    void EmitGoto(const ClGoto& record);
    /// @brief 機械座標の移動 (G53) を出力する
    /// @param record 出力するレコード
    void EmitMachineGoto(const ClGoto& record);
    /// @brief 主平面上の円弧をG02/G03で出力する
    /// @param record 円弧
    /// @param start 始点 (ワーク座標)
    /// @param plane 円弧の平面
    /// @param positive 法線が平面の正の軸方向か (負なら向きを反転する)
    void EmitPlanarArc(const ClArc& record, const igesio::Vector3d& start,
                       ArcPlane plane, bool positive);
    /// @brief 円弧を折れ線化して出力する
    /// @param record 円弧 (法線はゼロベクトルでないこと)
    /// @param start 始点 (ワーク座標)
    void EmitLinearizedArc(const ClArc& record, const igesio::Vector3d& start);
    /// @brief 終了を出力する (フッタの後に終了コード. 2回目以降は何もしない)
    void EmitEnd();

    /// @brief 動作コード (G00/G01) をワード列に追加する (モーダル省略に従う)
    /// @param kind 動作の種類 (`kRapid`/`kLinear`)
    /// @param[out] words 追加先
    void AppendMotionCode(MotionKind kind, std::vector<std::string>& words);
    /// @brief 座標語 (X/Y/Z) をワード列に追加する (直前と同じ成分は省略できる)
    /// @param point 制御点
    /// @param[out] words 追加先
    void AppendCoordinates(const igesio::Vector3d& point,
                           std::vector<std::string>& words);
    /// @brief 工具軸のIJK指令をワード列に追加する
    /// @param axis 工具軸方向 (正規化済み)
    /// @param[out] words 追加先
    void AppendToolAxis(const igesio::Vector3d& axis,
                        std::vector<std::string>& words);
    /// @brief 軸の指令 (直進軸、回転軸) をワード列に追加する
    /// @param axis_words 軸の指令
    /// @param linear 直進軸を含める
    /// @param rotary 回転軸を含める
    /// @param[out] words 追加先
    void AppendAxisWords(const NcValues& axis_words, bool linear, bool rotary,
                         std::vector<std::string>& words);
    /// @brief Fコードを切削ブロックに追加する
    /// @param[out] words 追加先
    void AppendFeed(std::vector<std::string>& words);
    /// @brief 工具軸に関するワード (TCPの形式に応じたIJK指令または回転軸の指令) を追加する
    /// @param record 直線移動
    /// @param[out] words 追加先
    /// @throw igesio::DataFormatError `kRotaryWords`で回転軸の指令が定まらない場合
    void AppendTcpWords(const ClGoto& record, std::vector<std::string>& words);
    /// @brief 経路上の役割が変わったときのコメントを出力する
    /// @param role 経路上の役割
    void EmitRoleComment(PathRole role);
    /// @brief テンプレート行を展開して出力する
    /// @param lines テンプレート行
    /// @param first_occurrence `true`なら変数に最初の出現を使う (ヘッダ用)
    void EmitTemplate(const std::vector<std::string>& lines, bool first_occurrence);
    /// @brief テンプレート変数を集める
    /// @param first_occurrence `true`なら最初の出現、`false`なら直近の値
    /// @return 変数名 → 値
    std::map<std::string, std::string> Variables(bool first_occurrence) const;
    /// @brief 冒頭のモーダルコードを出力する
    /// @note 制御装置定義のデフォルトと異なるものだけ
    void EmitPreamble();
    /// @brief 事前検査 (`ValidateClProgram`、長さ補正の欠落) の警告を出す
    /// @param program 検査するプログラム
    void Precheck(const ClProgram& program);
    /// @brief ワード列を1ブロックにして出力する (ワードが無ければ何もしない)
    /// @param words ワードの一覧
    void PushBlock(const std::vector<std::string>& words);
    /// @brief 行を出力する (N番号を付ける)
    /// @param text 行の本文
    void PushLine(const std::string& text);
    /// @brief 行をそのまま出力する (`%`用. N番号を付けない)
    /// @param text 行の本文
    void PushRaw(const std::string& text);
    /// @brief コメント行の文字列を作る
    /// @param text コメント本文
    /// @return `comment_open` + 本文 + `comment_close`
    std::string FormatComment(const std::string& text) const;
    /// @brief 警告を追加する (現在のレコードの行番号を付ける)
    /// @param message 内容
    void Warn(const std::string& message);
    /// @brief 直前に出力した工具軸と同じか
    /// @param axis 比較する工具軸方向
    bool SameToolAxis(const igesio::Vector3d& axis) const;

    /// @brief 制御装置の定義
    const NcDialect& dialect_;
    /// @brief 出力の設定
    const NcWriteOptions& options_;
    /// @brief 解決した構文 (`options_.syntax`があればそれ、無ければ`dialect_.syntax`)
    NcSyntax syntax_;
    /// @brief 警告の追記先 (`nullptr`なら報告しない)
    std::vector<Diagnostic>* warnings_;
    /// @brief 出力した行
    std::vector<std::string> lines_;
    /// @brief モーダル状態 (各レコードの出力後に`Apply`する)
    ClState state_;
    /// @brief 現在のレコードの行番号 (不明なら0)
    int current_line_ = 0;
    /// @brief 直前に出力した動作コード (円弧の後は`std::nullopt`)
    std::optional<MotionKind> last_motion_code_;
    /// @brief 直前に出力した座標 (G53の後は`std::nullopt`)
    std::optional<igesio::Vector3d> last_point_;
    /// @brief 直前に出力した工具軸
    std::optional<igesio::Vector3d> last_tool_axis_;
    /// @brief 直前に出力した (または制御装置定義のデフォルトの) 円弧の平面
    ArcPlane last_plane_ = ArcPlane::kXY;
    /// @brief 直前に出力した送り [mm/s]
    std::optional<double> emitted_feed_;
    /// @brief 直前に出力した経路上の役割
    std::optional<PathRole> last_role_;
    /// @brief 終了コードを出力済みか
    bool ended_ = false;
    /// @brief 折れ線化した円弧の数
    std::size_t linearized_arcs_ = 0;
    /// @brief 工具軸を+Zと仮定した警告を出したか
    bool assumed_axis_warned_ = false;
    /// @brief 円弧の工具軸の変化を書けない警告を出したか
    bool arc_axis_warned_ = false;
    /// @brief 出力した行数 (N番号用)
    std::size_t line_count_ = 0;
    /// @brief 機械定義の軸名 → 回転軸の軸アドレス
    std::map<std::string, char> rotary_register_to_address_;
    /// @brief 機械定義の軸名 → 直進軸の軸アドレス
    std::map<std::string, char> linear_register_to_address_;
    /// @brief テンプレート変数の最初の出現 (ヘッダ用. `BeginProgram`で集める)
    std::map<std::string, std::string> first_variables_;
    /// @brief プログラムの表示名
    std::string program_name_;
};

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_TOOLPATH_NC_EMIT_H_
