/**
 * @file extensions/machines.h
 * @brief 工作機械の機械定義・運動学・加工シミュレーション関連の拡張機能
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note IGESIO_ENABLE_MACHINES_EXTENSION有効時にのみビルドされる拡張モジュール.
 *       機械定義ファイル (TOML形式) の入出力、順/逆運動学、工具輪郭の実体化,
 *       工具経路 (NC/CL) の解釈、動作生成、シーン構築、アニメーション生成を
 *       サブフォルダ単位で提供する.
 *       - core/       診断・単位・回転等の共通基盤
 *       - machine/    機械定義のデータモデルと入出力、運動学モデル、順/逆運動学
 *       - tools/      工具輪郭とその実体化
 *       - project/    プロジェクト定義とセットアップ
 *       - toolpath/   工具経路の公開型とNC/CLの読込
 *       - simulation/ 動作生成とアニメーション生成
 *       - scene/      プリミティブ、形状読込、シーン構築
 * @note 全モジュールがGL非依存 (models層まで) であり、ヘッドレスでも利用できる.
 *       描画は既存の描画クラス (SurfaceOfRevolution・MeshEntity・LinearPath等)
 *       を利用するため、描画クラスの追加はない.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_H_
#define IGESIO_EXTENSIONS_MACHINES_H_

// 共通基盤
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/rotation.h"

// 機械定義・運動学
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/inverse_kinematics.h"

#endif  // IGESIO_EXTENSIONS_MACHINES_H_
