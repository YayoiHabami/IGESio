/**
 * @file extensions/machines/core/diagnostics.h
 * @brief machines拡張の警告/infoと運動学エラー
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 読込・動作生成の各段は、処理を中断する必要のない不備を`Diagnostic`
 *       として呼び出し側へ返す (例外にはしない). 仕様違反等の処理を終了すべき
 *       不備は`igesio::DataFormatError`等の既存例外を用い、到達不能姿勢のような
 *       運動学上の不備のみ本ヘッダの`KinematicsError`で表す.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_CORE_DIAGNOSTICS_H_
#define IGESIO_EXTENSIONS_MACHINES_CORE_DIAGNOSTICS_H_

#include <string>

#include "igesio/common/errors.h"

namespace igesio::extensions::machines {

/// @brief 重大度
enum class Severity {
    /// @brief 警告 (仕様上は許容されるが、記述ミスの可能性や可搬性の低下がある)
    kWarning,
    /// @brief 情報 (集計等。対処は不要)
    kInfo,
};

/// @brief 警告/infoの内容
/// @note `context`は発生箇所を示す文字列で、TOMLでは`"component[A].axis"`・
///       `"[collision].pairs[0]"`のようにテーブルの経路を,
///       NCでは`"block 1234"`のようにブロック番号を入れる.
struct Diagnostic {
    /// @brief 重大度
    Severity severity = Severity::kWarning;
    /// @brief 発生箇所
    std::string context;
    /// @brief 内容
    std::string message;
    /// @brief TOML/NCの行番号 (不明なら0)
    int line = 0;
};

/// @brief 到達不能姿勢・ストローク超過などの運動学上のエラー
/// @note 動作生成時にこれが発生した場合は、姿勢を変更せず直前の姿勢を維持する.
///       対応外の軸構成は`igesio::NotImplementedError`であり、本クラスではない.
class KinematicsError : public igesio::ComputationError {
 public:
    /// @brief 運動学エラーを初期化する
    /// @param message エラーの詳細メッセージ
    explicit KinematicsError(const std::string& message)
        : igesio::ComputationError(message) {}

    /// @brief デストラクタ
    virtual ~KinematicsError() noexcept = default;
};

/// @brief 警告/infoを1行の文字列にする
/// @param diagnostic 対象の警告/info
/// @return `"[warning] context: message (line N)"`の形式.
///         `context`が空なら`"context: "`を、`line`が0なら`" (line N)"`を省く
/// @note GUIのログ表示やテストの失敗メッセージに用いる
std::string FormatDiagnostic(const Diagnostic& diagnostic);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_CORE_DIAGNOSTICS_H_
