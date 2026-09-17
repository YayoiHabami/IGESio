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
 *       - machine/    機械定義のデータモデルと入出力、プリミティブ形状,
 *                     運動学モデル、順/逆運動学
 *       - tools/      工具輪郭とその実体化
 *       - project/    プロジェクト定義とセットアップ
 *       - toolpath/   工具経路 (CLプログラム) とNC/CLの読み書き
 *       - simulation/ 動作生成、アニメーション生成、動作の表示オブジェクト
 *       - scene/      形状読込、シーン構築、工具軌跡
 * @note 全モジュールがGL非依存 (models層まで) であり、ヘッドレスでも利用できる.
 *       描画は既存の描画クラス (SurfaceOfRevolution・MeshEntity・LinearPath等)
 *       を利用するため、描画クラスの追加はない. 工具軌跡はinspection拡張の
 *       `InstancedEntity`で表示するため、描画側では
 *       `RegisterInstancedEntityGraphics()`を一度呼ぶこと.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_H_
#define IGESIO_EXTENSIONS_MACHINES_H_

// 共通基盤
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/opaque_toml.h"
#include "igesio/extensions/machines/core/text_file.h"

// 機械定義・運動学
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/machine/primitives.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/inverse_kinematics.h"
#include "igesio/extensions/machines/machine/virtual_machines.h"

// 工具輪郭
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_entities.h"

// プロジェクト定義・セットアップ
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/project_io.h"
#include "igesio/extensions/machines/project/setup.h"

// 工具経路 (CLプログラムとNC/CLの読み書き)
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"
#include "igesio/extensions/machines/toolpath/cl_io.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_interpreter.h"
#include "igesio/extensions/machines/toolpath/nc_writer.h"

// 動作生成 (工具軸→回転角の計算、プログラム読込、動作のサンプル列)
#include "igesio/extensions/machines/simulation/axis_resolution.h"
#include "igesio/extensions/machines/simulation/program_loading.h"
#include "igesio/extensions/machines/simulation/motion.h"

// シーン (形状読込、シーン構築)
#include "igesio/extensions/machines/scene/geometry_loader.h"
#include "igesio/extensions/machines/scene/machine_scene.h"

// アニメーション (動作のサンプル列からのクリップ生成)
#include "igesio/extensions/machines/simulation/animation_bridge.h"

// 表示オブジェクト (工具軌跡、動作軌跡)
#include "igesio/extensions/machines/scene/tool_trajectory.h"
#include "igesio/extensions/machines/simulation/motion_scene.h"

#endif  // IGESIO_EXTENSIONS_MACHINES_H_
