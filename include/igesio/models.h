/**
 * @file models.h
 * @brief modelsディレクトリの主要な公開型をまとめるヘッダーファイル
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 * @note 中間データ構造(intermediate.h)およびシーン・選択状態管理
 *       (scene.h/selection_set.h/pick_filter.h)は集約対象外とする.
 *       後者は`igesio/scene.h`からまとめてincludeできる.
 */
#ifndef IGESIO_MODELS_H_
#define IGESIO_MODELS_H_

// グローバルパラメータ (IGESファイルのグローバルセクション情報)
#include "igesio/models/global_param.h"

// アセンブリツリーと平坦化
#include "igesio/models/assembly.h"
#include "igesio/models/flatten.h"

// IGESデータ (トップレベルコンテナ)
#include "igesio/models/iges_data.h"

namespace igesio {

// 頻用型をigesio直下に昇格させる別名
// (個別ヘッダのみをincludeした場合は従来どおりigesio::models::で参照する)
using models::IgesData;
using models::Assembly;

}  // namespace igesio

#endif  // IGESIO_MODELS_H_
