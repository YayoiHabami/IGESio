/**
 * @file extensions/obj/obj_io.h
 * @brief OBJ (Wavefront OBJ) ファイルの入出力
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note IGESIO_ENABLE_OBJ_EXTENSION有効時にのみビルドされる拡張モジュール.
 *       メッシュデータ (numerics::TriangleMeshd) との相互変換と、
 *       エンティティ直結の読み込み (ReadObjAsEntity) を提供する.
 * @note マテリアルライブラリ (.mtl) の実体 (色・テクスチャパス等) は扱わない.
 *       usemtlの参照名のみをMeshGroup::material_nameへ保持/出力する
 *       (TODO: .mtlの読み書き対応).
 */
#ifndef IGESIO_EXTENSIONS_OBJ_OBJ_IO_H_
#define IGESIO_EXTENSIONS_OBJ_OBJ_IO_H_

#include <memory>
#include <string>

#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/entities/mesh_entity.h"

/// @brief IGESio本体の拡張モジュール (外部フォーマット入出力等)
namespace igesio::extensions {

/// @brief OBJ書き出しの制御パラメータ
struct WriteObjParams {
    /// @brief 頂点法線 (vn) を出力するか
    /// @note メッシュが法線チャンネルを持つ場合にのみ有効
    bool write_normals = true;
    /// @brief テクスチャ座標 (vt) を出力するか
    /// @note メッシュがUVチャンネルを持つ場合にのみ有効
    bool write_uvs = true;
    /// @brief 面グループ (g / usemtl) を出力するか
    /// @note メッシュが面グループを持つ場合にのみ有効.
    ///       マテリアル実体 (.mtl) は出力しない (usemtlの参照名のみ)
    bool write_groups = true;
};

/// @brief OBJファイルを読み込む
/// @param path OBJファイルのパス
/// @return 読み込んだ三角形メッシュ (OBJはASCII十進数のため倍精度).
///         多角形フェイスはファン分割し、v/vt/vnの多重インデックスは
///         コーナー重複排除で単一インデックス空間へ正規化する
/// @throw igesio::FileOpenError ファイルが開けなかった場合
/// @throw igesio::ParseError ファイルがOBJとして不正な場合
///        (頂点インデックスが範囲外・面の頂点が3未満等)
/// @note 対応キーワード: v / vt / vn / f / g / o / usemtl.
///       それ以外 (s / mtllib / コメント等) は読み飛ばす.
///       usemtlはMeshGroup::material_nameへ参照名のみ保持する
numerics::TriangleMeshd ReadObj(const std::string& path);

/// @brief OBJファイルを読み込み、メッシュエンティティとして返す
/// @param path OBJファイルのパス
/// @return 読み込んだメッシュを保持するMeshEntity.
///         Assemblyへ追加すればそのまま描画パイプラインに乗る
/// @throw igesio::FileOpenError ファイルが開けなかった場合
/// @throw igesio::ParseError ファイルがOBJとして不正な場合
std::shared_ptr<entities::MeshEntity> ReadObjAsEntity(const std::string& path);

/// @brief メッシュをOBJファイルへ書き出す
/// @param mesh 書き出す三角形メッシュ
/// @param path 出力するOBJファイルのパス (同名ファイルは上書きする)
/// @param params 書き出しの制御パラメータ
/// @return 書き込みに成功したか
/// @throw std::invalid_argument meshの構造が不正な場合
///        (numerics::Validateに失敗する場合)
/// @throw igesio::FileOpenError ファイルが開けなかった場合
/// @note 倍精度の値を往復可能な桁数で出力する.
///       マテリアル実体 (.mtl) は出力しない (usemtlの参照名のみ)
bool WriteObj(const numerics::TriangleMeshd& mesh, const std::string& path,
              const WriteObjParams& params = {});

}  // namespace igesio::extensions

#endif  // IGESIO_EXTENSIONS_OBJ_OBJ_IO_H_
