/**
 * @file extensions/common/text_scanner.h
 * @brief テキスト形式のメッシュファイル（STL/OBJ）用の字句解析と数値の読取 (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-10-09
 * @copyright 2026 Yayoi Habami
 * @note `stl_io.cpp`/`obj_io.cpp`が共用する. 入力はバイト列のまま扱い,
 *       空白として扱う文字は「空白、\t、\n、\v、\f、\r」のいずれかとする.
 *       数値の読み取りは`std::istream`の`>>`と同じ結果（成否と値）になるようにする.
 *       公開ヘッダには含めない
 */
#ifndef SRC_EXTENSIONS_COMMON_TEXT_SCANNER_H_
#define SRC_EXTENSIONS_COMMON_TEXT_SCANNER_H_

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace igesio::extensions::detail {

/// @brief 空白か判定する
/// @param c 判定する文字
/// @return 空白、\t、\n、\v、\f、\rのいずれかの場合はtrue
inline bool IsSpaceChar(const char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
           c == '\r';
}

/// @brief 空白区切りのテキストを先頭から順に読み取る字句解析用クラス
/// @note 文字列は本クラス側では管理しない. 本オブジェクトおよび戻り値のstring_view
///       より先に対象の文字列を破棄しないこと. 読み取り範囲外の文字は参照しない
class TextScanner {
 public:
    /// @brief コンストラクタ
    /// @param text 読み取る文字列（先頭から読み取る）
    explicit TextScanner(const std::string_view text)
            : p_(text.data()), end_(text.data() + text.size()) {}

    /// @brief 読み取り位置が終端に達したか
    bool AtEnd() const { return p_ >= end_; }

    /// @brief 読み取り位置を空白の後まで進める
    void SkipSpaces() {
        while (p_ < end_ && IsSpaceChar(*p_)) ++p_;
    }

    /// @brief 読み取り位置を次の改行文字の直前まで進める
    /// @note 改行文字は読まずに残す. 改行文字が無い場合は終端まで進める
    void SkipToLineEnd() {
        while (p_ < end_ && *p_ != '\n') ++p_;
    }

    /// @brief 空白で区切られた次の語を取得する
    /// @return 読み取り結果（読み取り対象の文字列の参照）. 無い場合は空文字列
    /// @note 読み取り位置は読み取り結果の直後まで進める
    std::string_view NextToken() {
        SkipSpaces();
        const char* begin = p_;
        while (p_ < end_ && !IsSpaceChar(*p_)) ++p_;
        return std::string_view(begin, static_cast<std::size_t>(p_ - begin));
    }

    /// @brief 残り全体を取得する
    /// @return 現在の読み取り位置から終端までの文字列（読み取り対象の文字列を参照）
    /// @note 読み取り位置は進めない
    std::string_view Rest() const {
        return std::string_view(p_, static_cast<std::size_t>(end_ - p_));
    }

    /// @brief 空白の後の倍精度の実数を読み取る
    /// @param[out] value 読み取った値. 失敗した場合は、オーバーフローなら
    ///        符号付きの最大値、それ以外は0を設定する（`std::istream`の`>>`に同じ）
    /// @return 読み取れた場合はtrue
    /// @note 成功した場合は読み取り位置を数値の直後まで進める.
    ///       数値の直後の文字は読まずに残す（例: "1.5abc"は1.5を読み、"abc"を残す）.
    ///       先頭の`+`、小数点で始まる値（".5"）、アンダーフロー（0になる）は
    ///       読み取るが、"inf"/"nan"、オーバーフロー、不完全な指数（"1e"）falseを返す.
    bool ReadReal(double& value);

    /// @brief 空白の後の単精度の実数を読み取る
    /// @param[out] value 読み取った値
    /// @return 読み取れた場合はtrue
    /// @note 読み込み方と読み取り結果はReadReal(double&)と同じ
    bool ReadReal(float& value);

    /// @brief 空白で区切られた実数を指定した個数だけ連続して読み取る
    /// @tparam T 読み取る型（float/double）
    /// @param[out] values 読み取った値の格納先（count個分の領域を確保すること）
    /// @param count 読み取る個数
    /// @return count個すべて読み取れた場合はtrue
    /// @note 途中で失敗した場合はそこで中断する. それまでに読み取った値と
    ///       読み取り位置はそのまま残る
    template <typename T>
    bool ReadReals(T* values, const std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            if (!ReadReal(values[i])) return false;
        }
        return true;
    }

 private:
    /// @brief 次に読み取る文字の位置
    const char* p_;
    /// @brief 読み取り範囲の終端（この位置は含まない）
    const char* end_;
};

/// @brief text全体を10進の符号付き整数として読み取る
/// @param text 読み取り内容（空白を含まないこと）
/// @param[out] value 読み取った値（成功時のみ設定）
/// @return 成功した場合はstd::errc{}. text全体が整数でない場合は
///         std::errc::invalid_argument、64bitの範囲外の場合は
///         std::errc::result_out_of_range
/// @note 先頭に1つだけ`+`または`-`がつくことを許容する（`std::stoll`と同じ）
std::errc ParseInteger(std::string_view text, std::int64_t& value);

}  // namespace igesio::extensions::detail

#endif  // SRC_EXTENSIONS_COMMON_TEXT_SCANNER_H_
