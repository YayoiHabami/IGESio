/**
 * @file extensions/stl/stl_io.h
 * @brief STL (stereolithography) ファイルの入出力
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note IGESIO_ENABLE_STL_EXTENSION有効時にのみビルドされる拡張モジュール.
 *       メッシュデータ (numerics::TriangleMeshf) との相互変換と、
 *       エンティティ直結の読み込み (ReadStlAsEntity) を提供する.
 */
#ifndef IGESIO_EXTENSIONS_STL_STL_IO_H_
#define IGESIO_EXTENSIONS_STL_STL_IO_H_

#include <memory>
#include <string>

#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/entities/mesh_entity.h"

/// @brief IGESio本体の拡張モジュール (外部フォーマット入出力等)
namespace igesio::extensions {

/// @brief STL読み込みの制御パラメータ
struct StlReadParams {
    /// @brief 三角形スープを溶接して共有頂点化するか
    /// @note true: 一致する頂点を共有化したインデックスメッシュを返す.
    ///       ファイルの面法線は破棄される (法線チャンネルなし. 必要なら
    ///       numerics::RecomputeNormalsで滑らか法線を再計算する.
    ///       描画 (TriangleMeshGraphics) は法線なしを自動で補う).
    /// @note false: 三角形毎に独立な頂点 (3頂点/三角形) のまま返し、
    ///       ファイルの面法線を各頂点へ展開する (ハードエッジが保たれる)
    bool weld_vertices = true;
    /// @brief 溶接時の許容距離
    /// @note 各座標成分をこの値で量子化して一致判定する.
    ///       0以下の場合はビット単位の完全一致のみを溶接する
    double weld_tolerance = 0.0;
};

/// @brief STLファイルを読み込む
/// @param path STLファイルのパス
/// @param params 読み込みの制御パラメータ
/// @return 読み込んだ三角形メッシュ (STLはfloat32のため単精度)
/// @throw igesio::FileOpenError ファイルが開けなかった場合
/// @throw igesio::ParseError ファイルがSTL (バイナリ/ASCII) として不正な場合
/// @note バイナリ/ASCIIはファイルサイズとレコード数の整合で自動判別する
///       (バイナリ形式のヘッダが"solid"で始まるファイルも正しく扱う)
numerics::TriangleMeshf ReadStl(const std::string& path,
                                const StlReadParams& params = {});

/// @brief STLファイルを読み込み、メッシュエンティティとして返す
/// @param path STLファイルのパス
/// @param params 読み込みの制御パラメータ
/// @return 読み込んだメッシュを保持するMeshEntity (倍精度へ変換して保持).
///         Assemblyへ追加すればそのまま描画パイプラインに乗る
/// @throw igesio::FileOpenError ファイルが開けなかった場合
/// @throw igesio::ParseError ファイルがSTLとして不正な場合
std::shared_ptr<entities::MeshEntity>
ReadStlAsEntity(const std::string& path, const StlReadParams& params = {});

/// @brief メッシュをSTLファイルへ書き出す
/// @param mesh 書き出す三角形メッシュ
/// @param path 出力するSTLファイルのパス (同名ファイルは上書きする)
/// @param binary バイナリ形式で出力するか (false: ASCII形式)
/// @return 書き込みに成功したか
/// @throw std::invalid_argument meshの構造が不正な場合
///        (numerics::Validateに失敗する場合)
/// @throw igesio::FileOpenError ファイルが開けなかった場合
/// @note 面法線は三角形の幾何 (頂点の外積) から計算して出力する
///       (頂点法線チャンネルは使用しない. 退化三角形の法線はゼロベクトル).
///       面グループ・UVはSTLでは表現できないため出力されない
bool WriteStl(const numerics::TriangleMeshf& mesh, const std::string& path,
              bool binary = true);

}  // namespace igesio::extensions

#endif  // IGESIO_EXTENSIONS_STL_STL_IO_H_
