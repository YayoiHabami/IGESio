/**
 * @file entities/interfaces.h
 * @brief エンティティクラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-07-12
 * @copyright 2025 Yayoi Habami
 * @note `IEntityIdentifier`は、全てのエンティティクラス・インターフェースクラスの
 *       規定クラスであり、エンティティのID、タイプ、およびフォーム番号を
 *       取得するためのインターフェースを提供する. すべての個別エンティティクラスは
 *       このクラスを継承した`EntityBase`クラスを継承する.
 *       その他のインターフェースは、特定のエンティティタイプに関連する
 *       機能を提供する.
 */
#ifndef IGESIO_ENTITIES_INTERFACES_H_
#define IGESIO_ENTITIES_INTERFACES_H_

// IEntityIdentifier: 全エンティティクラスの基底インターフェース
#include "igesio/entities/interfaces/i_entity_identifier.h"

// IStructure~ITransformation
#include "igesio/entities/interfaces/de_related.h"

// ICurve
#include "igesio/entities/interfaces/i_curve.h"

// ISurface
#include "igesio/entities/interfaces/i_surface.h"

#endif  // IGESIO_ENTITIES_INTERFACES_H_
