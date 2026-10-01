/**
 * @file tests/unicode_test_path.h
 * @brief 全角文字を含むパスでの入出力を検証するためのテスト用ヘルパー
 * @author Yayoi Habami
 * @date 2026-10-01
 * @copyright 2026 Yayoi Habami
 * @note 名前は16進エスケープで書いたUTF-8のバイト列とし、ソースの文字コードや
 *       コンパイラのオプション (MSVCの`/utf-8`等) に依存しないようにする.
 *       日本語に加えてCP932で表せない文字 (絵文字) を含めることで、
 *       日本語版Windows (CP932) でもANSIコードページ経由の変換を検出する.
 *       macOSで列挙時の正規化により名前が変わらないよう、濁点を含む文字は避ける.
 */
#ifndef IGESIO_TESTS_UNICODE_TEST_PATH_H_
#define IGESIO_TESTS_UNICODE_TEST_PATH_H_

#include <filesystem>
#include <string>
#include <system_error>

#include "igesio/utils/path_encoding.h"

namespace igesio::tests {

/// @brief 全角文字とCP932外の文字を含むUTF-8の名前 ("全角_" + U+1F642)
inline const std::string kUnicodeName =
        "\xE5\x85\xA8\xE8\xA7\x92_\xF0\x9F\x99\x82";

/// @brief 全角文字を含む名前の一時ディレクトリを作成し、破棄時に削除するクラス
/// @note テストごとに`tag`を変え、並行実行されるテスト実行ファイル間で
///       ディレクトリが重ならないようにする
class UnicodeTempDir {
 public:
    /// @brief 一時ディレクトリを作成する
    /// @param tag ディレクトリ名に付ける識別子 (ASCII)
    /// @throw std::filesystem::filesystem_error ディレクトリを作成できない場合
    explicit UnicodeTempDir(const std::string& tag)
        : path_(std::filesystem::temp_directory_path() /
                utils::PathFromUtf8("igesio_" + tag + "_" + kUnicodeName)) {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_);
    }

    /// @brief 一時ディレクトリを削除する
    ~UnicodeTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    /// @brief コピーを禁止する
    UnicodeTempDir(const UnicodeTempDir&) = delete;
    /// @brief コピー代入を禁止する
    UnicodeTempDir& operator=(const UnicodeTempDir&) = delete;

    /// @brief 一時ディレクトリのパスを取得する
    /// @return 一時ディレクトリのパス
    const std::filesystem::path& Path() const { return path_; }

    /// @brief 一時ディレクトリ内のパスを作成する
    /// @param utf8_name 一時ディレクトリからの相対パス (UTF-8)
    /// @return 作成したパス
    std::filesystem::path Join(const std::string& utf8_name) const {
        return path_ / utils::PathFromUtf8(utf8_name);
    }

    /// @brief 一時ディレクトリ内のパスをUTF-8文字列として作成する
    /// @param utf8_name 一時ディレクトリからの相対パス (UTF-8)
    /// @return UTF-8で表したパス
    std::string JoinUtf8(const std::string& utf8_name) const {
        return utils::PathToUtf8(Join(utf8_name));
    }

 private:
    /// @brief 一時ディレクトリのパス
    std::filesystem::path path_;
};

}  // namespace igesio::tests

#endif  // IGESIO_TESTS_UNICODE_TEST_PATH_H_
