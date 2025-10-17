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
CreateCurveGraphics(const std::shared_ptr<const entities::ICurve>,
                    const std::shared_ptr<IOpenGL>);

/// @brief 曲面の描画オブジェクトを作成する
/// @param entity ISurfaceエンティティのポインタ
/// @param gl OpenGL関数のラッパー
/// @return 作成された描画オブジェクト、無効な場合はnullptr
std::unique_ptr<IEntityGraphics>
CreateSurfaceGraphics(const std::shared_ptr<const entities::ISurface>,
                      const std::shared_ptr<IOpenGL>);

/// @brief エンティティの描画オブジェクト作成する関数
/// @param entity 描画するエンティティのポインタ
/// @param gl OpenGL関数のラッパー
/// @return 作成された描画オブジェクト、無効な場合はnullptr
std::unique_ptr<IEntityGraphics>
CreateEntityGraphics(const std::shared_ptr<const entities::IEntityIdentifier>,
                     const std::shared_ptr<IOpenGL>);

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_FACTORY_H_
