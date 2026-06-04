/**
 * @file scene.h
 * @brief シーン・選択状態管理に関するヘッダーファイルをまとめる
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 * @note ビューア向けの状態管理機能のため、`igesio/igesio.h`には含めない.
 */
#ifndef IGESIO_SCENE_H_
#define IGESIO_SCENE_H_

// 選択状態の保持
#include "igesio/models/selection_set.h"

// ピッキング対象のフィルタ
#include "igesio/models/pick_filter.h"

// シーン (表示・選択状態の統合管理)
#include "igesio/models/scene.h"

#endif  // IGESIO_SCENE_H_
