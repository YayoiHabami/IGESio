/**
 * @file extensions/inspection.h
 * @brief 検査系のエンティティ等の拡張機能
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note IGESIO_ENABLE_INSPECTION_EXTENSION有効時にのみビルドされる拡張モジュール.
 *       細かい検査表示の追加エンティティ群 (座標系群等) をまとめる.
 * @note 描画クラスはgraphics有効時にのみ提供される. 描画を行う場合は以下を呼ぶこと:
 *       - `RegisterCoordinateFrameGroupGraphics()`: CoordinateFrameGroup描画用
 */
#ifndef IGESIO_EXTENSIONS_INSPECTION_H_
#define IGESIO_EXTENSIONS_INSPECTION_H_

#include "igesio/extensions/inspection/coordinate_frame_group.h"



// 描画クラスはgraphics有効時にのみ提供される
#ifdef IGESIO_ENABLE_GRAPHICS

#include "igesio/extensions/inspection/coordinate_frame_group_graphics.h"

#endif  // IGESIO_ENABLE_GRAPHICS

#endif  // IGESIO_EXTENSIONS_INSPECTION_H_
