/**
 * @file igesio.h
 * @brief IGESioライブラリの公開APIをまとめるヘッダーファイル (公開APIの入口)
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 * @note コンパイル時間を絞りたい場合は、カテゴリ別集約ヘッダ
 *       (`igesio/entities/curves.h`等)や個別ヘッダを直接includeすること.
 */
#ifndef IGESIO_IGESIO_H_
#define IGESIO_IGESIO_H_

// エラー型 (IGESioError階層)
#include "igesio/common/errors.h"

// 数値計算 (行列・ベクトル、許容誤差、バウンディングボックス等)
#include "igesio/numerics.h"

// エンティティ (全エンティティ・インターフェース・ファクトリ)
#include "igesio/entities.h"

// データモデル (IgesData、Assembly、GlobalParam)
#include "igesio/models.h"

// 入出力 (ReadIges/WriteIges)
#include "igesio/reader.h"
#include "igesio/writer.h"

#endif  // IGESIO_IGESIO_H_
