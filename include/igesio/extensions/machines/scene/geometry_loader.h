/**
 * @file extensions/machines/scene/geometry_loader.h
 * @brief 機械定義・プロジェクト定義の形状 (`GeometrySpec`) の読込
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note ファイル参照形式 (STL・OBJ・IGES) とプリミティブ形式の形状を、色・不透明度を
 *       設定したアセンブリとして作る. STL/OBJは`MeshEntity`、プリミティブは
 *       `MakePrimitiveMesh`のメッシュ、IGESは`ReadIges`のルートアセンブリを子として
 *       持つ.
 * @note 返すアセンブリの変換は単位行列、名前は空で、表示状態にする. 配置先
 *       (`GeometryInstance::placement`)、名前、可視性は呼び出し側 (シーン構築)
 *       が設定すること (`MakeToolAssembly`と同様). 本ヘッダは形状の取得と見た目
 *       だけを扱い、配置先/役割 (`collision`・`visible`) は扱わない.
 * @note メッシュは頂点統合済み (共有頂点) で読み込み、法線を持たなければ折り目を
 *       保った頂点法線を再計算する. STEPと対応外の形式は読込側で既に警告済み
 *       なので、ここではinfoを追加して`nullptr`を返す.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SCENE_GEOMETRY_LOADER_H_
#define IGESIO_EXTENSIONS_MACHINES_SCENE_GEOMETRY_LOADER_H_

#include <cmath>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "igesio/models/assembly.h"
#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/extensions/stl/stl_io.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/machine_definition.h"

namespace igesio::extensions::machines {

/// @brief 形状読込の設定
struct GeometryLoadOptions {
    /// @brief STL読込の制御 (デフォルト: 頂点統合する)
    extensions::StlReadParams stl;
    /// @brief 折り目法線のしきい値 (稜線を挟む2面の法線の内積)
    /// @note デフォルトはcos 30°. 頂点統合済みで法線を持たないメッシュに
    ///       `RecomputeNormalsWithCrease`で頂点法線を設定するときに用いる
    double crease_angle_cos = std::cos(ToRadians(30.0));
};

/// @brief メッシュ形式の形状 (STL・OBJ・プリミティブ) を読み込む
/// @param spec 形状
/// @param options 読込の設定
/// @param context 診断の発生個所 (`"component[X].geometry[0]"`等)
/// @param[out] warnings 警告・infoの追加先 (nullptrなら追加しない)
/// @return 単位換算済み (mm) で頂点法線を持つメッシュ.
///         IGES・STEP・対応外の形式はinfoを追加して`std::nullopt`
/// @throw igesio::FileOpenError ファイルが開けない場合
/// @throw igesio::ParseError ファイルがSTL/OBJとして不正な場合
/// @throw std::invalid_argument プリミティブの寸法が正でない場合
std::optional<numerics::TriangleMeshd> LoadGeometryMesh(
        const GeometrySpec& spec, const GeometryLoadOptions& options,
        std::string_view context, std::vector<Diagnostic>* warnings);

/// @brief 形状をアセンブリとして作る
/// @param spec 形状 (色・不透明度を反映する)
/// @param options 読込の設定
/// @param context 診断の発生個所 (`"[[model]](stock)"`等)
/// @param[out] warnings 警告・infoの追加先 (nullptrなら追加しない)
/// @return 形状のアセンブリ. メッシュ形式は`MeshEntity`を1つ持ち,
///         IGESは`ReadIges`のルートを子アセンブリとして持つ.
///         読めなかった場合 (STEP・対応外の形式・mm以外の単位のIGES)
///         は診断を追加して`nullptr`
/// @throw igesio::FileOpenError ファイルが開けない場合
/// @throw igesio::ParseError ファイルがSTL/OBJとして不正な場合
/// @throw igesio::DataFormatError IGESファイルの内容が不正な場合
/// @throw std::invalid_argument プリミティブの寸法が正でない場合
/// @note 戻り値の変換は単位行列、名前は空で、表示状態にする. 色・不透明度は
///       オーバーライドとして設定する
std::shared_ptr<models::Assembly> LoadGeometry(
        const GeometrySpec& spec, const GeometryLoadOptions& options,
        std::string_view context, std::vector<Diagnostic>* warnings);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SCENE_GEOMETRY_LOADER_H_
