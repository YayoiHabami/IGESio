/**
 * @file models/pick_filter.h
 * @brief ピック対象の種別フィルタを定義する
 * @author Yayoi Habami
 * @date 2026-05-30
 * @copyright 2026 Yayoi Habami
 * @note フィルタは選択の意味論を左右し、ヘッドレスなクエリ選択でも使うため、(c)ビューでなく
 *       (a')セッション状態として`Scene`が既定値を保持する.
 */
#ifndef IGESIO_MODELS_PICK_FILTER_H_
#define IGESIO_MODELS_PICK_FILTER_H_

namespace igesio::models {

/// @brief ピック対象の種別フィルタ (組合せ可能)
/// @note 背反でなくビットの集合として扱う. v1は`bodies`(エンティティ単位)のみ尊重し、
///       面/エッジ/頂点のサブエンティティ選択は将来課題.
struct PickFilter {
    /// @brief 面を対象とするか (将来)
    bool faces = true;
    /// @brief エッジを対象とするか (将来)
    bool edges = true;
    /// @brief 頂点を対象とするか (将来)
    bool vertices = true;
    /// @brief 部品/エンティティ単位を対象とするか
    bool bodies = true;
};

}  // namespace igesio::models

#endif  // IGESIO_MODELS_PICK_FILTER_H_
