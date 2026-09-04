/**
 * @file extensions/animation.h
 * @brief キーフレームアニメーション関連の拡張機能
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 * @note IGESIO_ENABLE_ANIMATION_EXTENSION有効時にのみビルドされる拡張モジュール.
 *       アセンブリのIDを対象に、時刻とともに変換行列を切り替えるステップ型の
 *       キーフレームアニメーション (データモデル+再生用クラス) を提供する.
 * @note 姿勢の適用は`Assembly::SetGlobalTransform`経由で行うため、描画クラスの
 *       追加は不要 (レンダラのReconcileが自動で追従する).
 *       GL非依存であり、ヘッドレスでも利用できる.
 */
#ifndef IGESIO_EXTENSIONS_ANIMATION_H_
#define IGESIO_EXTENSIONS_ANIMATION_H_

#include "igesio/extensions/animation/animation_clip.h"
#include "igesio/extensions/animation/animation_player.h"

#endif  // IGESIO_EXTENSIONS_ANIMATION_H_
