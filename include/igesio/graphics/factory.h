/**
 * @file graphics/factory.h
 * @brief エンティティクラスから描画オブジェクトを作成する
 * @author Yayoi Habami
 * @date 2025-08-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_FACTORY_H_
#define IGESIO_GRAPHICS_FACTORY_H_

#include <memory>
#include <vector>

#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief 曲線の描画オブジェクトを作成する
/// @param entity ICurveエンティティのポインタ
/// @param gl OpenGL関数のラッパー
/// @return 作成された描画オブジェクト、無効な場合はnullptr
std::unique_ptr<IEntityGraphics>
CreateCurveGraphics(const std::shared_ptr<const entities::ICurve>&,
                    const std::shared_ptr<IOpenGL>&);

/// @brief 曲面の描画オブジェクトを作成する
/// @param entity ISurfaceエンティティのポインタ
/// @param gl OpenGL関数のラッパー
/// @return 作成された描画オブジェクト、無効な場合はnullptr
std::unique_ptr<IEntityGraphics>
CreateSurfaceGraphics(const std::shared_ptr<const entities::ISurface>&,
                      const std::shared_ptr<IOpenGL>&);

/// @brief エンティティの描画オブジェクト作成する関数
/// @param entity 描画するエンティティのポインタ
/// @param gl OpenGL関数のラッパー
/// @param synchronize 生成時に同期 (CPU構築+GL転送) まで行うか (既定: true)
/// @return 作成された描画オブジェクト、無効な場合はnullptr
/// @note 既定では生成時にSynchronizeまで行い、即描画可能な状態で返す. レンダラは
///       reconcile経路でCPU相を並列に前倒ししてからGL転送を直列に行うため、
///       synchronize=falseを指定して生成時の同期を省く (子孫も未同期で返る)。
/// @note 不変条件: nullptrの返却は「型起因の恒久的失敗」(未サポート型・
///       描画対応のないインターフェース) のみとすること. レンダラはnullptrを
///       負キャッシュとして保持し、同一エンティティの生成を再試行しない.
///       将来データ依存の失敗 (一時的に不完全な形状等) を導入する場合は、
///       失敗時のジオメトリリビジョンを記録し変化時に再試行する設計が必要
std::unique_ptr<IEntityGraphics>
CreateEntityGraphics(const std::shared_ptr<const entities::IEntityIdentifier>&,
                     const std::shared_ptr<IOpenGL>&,
                     bool synchronize = true);

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_FACTORY_H_
