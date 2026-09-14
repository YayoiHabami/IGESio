/**
 * @file extensions/machines/core/opaque_toml.h
 * @brief machine拡張側で読み込まないTOML要素を保持するための型定義
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note プロジェクト定義のうち本拡張が読まないセクションを、書き出し時にそのまま
 *       戻せるようにTOML形式の文字列として保持する. 公開ヘッダにtoml11の型を
 *       出さないようにするため、toml11の値との相互変換は内部ヘッダで行う.
 *        (`toml_reading.h`の`ToOpaque`、`toml_writing.h`の`FromOpaque`)
 * @note 各要素は、トップレベルのキーを1つだけ持つTOMLの形 (`key = value`,
 *       `[key] ...`, `[[key]] ...`) とする. テーブル配列 (`[[key]]`) はキー無しでは
 *       整形できず、キー無しの文字列ではテーブル本体と値 (`[1]`等) を判別できないため.
 *       値は保持するが、コメント・書式の再現はtoml11の保持範囲に留める.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_CORE_OPAQUE_TOML_H_
#define IGESIO_EXTENSIONS_MACHINES_CORE_OPAQUE_TOML_H_

#include <string>
#include <utility>

namespace igesio::extensions::machines {

/// @brief 読み込まないTOML要素を保持するための型 (トップレベルのキー1つとその値)
/// @note 文字列の妥当性は構築時に検査しない. 書き出し時 (`FromOpaque`) に
///       解析できなければ`std::invalid_argument`となる
class OpaqueToml {
 public:
    /// @brief 空のTOML要素を構築する
    OpaqueToml() = default;
    /// @brief TOML形式の文字列から構築する
    /// @param toml_text トップレベルのキーを1つだけ持つTOMLの文字列
    ///        (`key = value` / `[key] ...` / `[[key]] ...`)
    explicit OpaqueToml(std::string toml_text) : text_(std::move(toml_text)) {}

    /// @brief TOML形式の文字列
    /// @return 構築時に与えた文字列
    const std::string& Text() const { return text_; }
    /// @brief 文字列が空か
    bool Empty() const { return text_.empty(); }

 private:
    /// @brief TOML形式の文字列
    std::string text_;
};

/// @brief 文字列の完全一致
/// @return TOML形式の文字列が一致すれば`true`
inline bool operator==(const OpaqueToml& lhs, const OpaqueToml& rhs) {
    return lhs.Text() == rhs.Text();
}

/// @brief `operator==`の否定
/// @return TOML形式の文字列が異なれば`true`
inline bool operator!=(const OpaqueToml& lhs, const OpaqueToml& rhs) {
    return !(lhs == rhs);
}

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_CORE_OPAQUE_TOML_H_
