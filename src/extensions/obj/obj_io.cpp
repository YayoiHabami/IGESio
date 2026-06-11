/**
 * @file extensions/obj/obj_io.cpp
 * @brief OBJ (Wavefront OBJ) ファイルの入出力
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/obj/obj_io.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/meshes/algorithms.h"

namespace {

namespace fs = std::filesystem;
namespace i_ext = igesio::extensions;
namespace i_num = igesio::numerics;
using Vec3 = Eigen::Vector3d;
using Vec2 = Eigen::Vector2d;

/// @brief 解決済みのコーナー (面頂点) 参照
/// @note 各成分は0始まりのインデックス. -1はそのチャンネルを参照しないことを表す
struct Corner {
    /// @brief 頂点位置のインデックス
    std::int64_t v = -1;
    /// @brief テクスチャ座標のインデックス (-1=なし)
    std::int64_t t = -1;
    /// @brief 頂点法線のインデックス (-1=なし)
    std::int64_t n = -1;
};

/// @brief コーナー重複排除のキー (位置・UV・法線のインデックスの組)
using CornerKey = std::tuple<std::int64_t, std::int64_t, std::int64_t>;

/// @brief CornerKeyのハッシュ関数
struct CornerKeyHash {
    /// @brief ハッシュ値を計算する
    std::size_t operator()(const CornerKey& key) const {
        constexpr std::uint64_t kGolden = 0x9e3779b97f4a7c15ULL;
        std::uint64_t seed = 0;
        for (const auto value : {std::get<0>(key), std::get<1>(key),
                                 std::get<2>(key)}) {
            seed ^= static_cast<std::uint64_t>(value) + kGolden +
                    (seed << 6) + (seed >> 2);
        }
        return static_cast<std::size_t>(seed);
    }
};

/// @brief 文字列の前後の空白を取り除く
/// @param text 対象の文字列
/// @return 前後の空白を除いた文字列
std::string Trim(const std::string& text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

/// @brief OBJのインデックス文字列を解析する
/// @param part インデックス文字列 (空文字列は「なし」を表す)
/// @param path エラーメッセージ用のファイルパス
/// @return 解析した1始まり/負のインデックス. 空文字列の場合はstd::nullopt
/// @throw igesio::ParseError 数値として解釈できない場合
std::optional<std::int64_t> ParseIndex(const std::string& part,
                                       const std::string& path) {
    if (part.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        const auto value = std::stoll(part, &consumed);
        if (consumed != part.size()) {
            throw igesio::ParseError(
                    "Invalid face index '" + part + "' in OBJ file: " + path);
        }
        return value;
    } catch (const std::invalid_argument&) {
        throw igesio::ParseError(
                "Invalid face index '" + part + "' in OBJ file: " + path);
    } catch (const std::out_of_range&) {
        throw igesio::ParseError(
                "Face index out of range '" + part + "' in OBJ file: " + path);
    }
}

/// @brief 1始まり/負のOBJインデックスを0始まりへ解決する
/// @param index 1始まり (正) または末尾相対 (負) のインデックス
/// @param count 対象配列の現在の要素数
/// @param path エラーメッセージ用のファイルパス
/// @return 0始まりのインデックス
/// @throw igesio::ParseError インデックスが0または範囲外の場合
std::int64_t ResolveIndex(const std::int64_t index, const std::size_t count,
                          const std::string& path) {
    std::int64_t resolved = 0;
    if (index > 0) {
        resolved = index - 1;
    } else if (index < 0) {
        resolved = static_cast<std::int64_t>(count) + index;
    } else {
        throw igesio::ParseError("Face index 0 is invalid in OBJ file: " + path);
    }
    if (resolved < 0 || resolved >= static_cast<std::int64_t>(count)) {
        throw igesio::ParseError(
                "Face index out of range in OBJ file: " + path);
    }
    return resolved;
}

/// @brief 面頂点トークン (v / v/vt / v//vn / v/vt/vn) を解決済みコーナーへ変換する
/// @param token 面頂点トークン
/// @param vertex_count 位置配列の現在の要素数
/// @param uv_count テクスチャ座標配列の現在の要素数
/// @param normal_count 法線配列の現在の要素数
/// @param path エラーメッセージ用のファイルパス
/// @return 解決済みコーナー
/// @throw igesio::ParseError トークンが不正な場合
Corner ParseCorner(const std::string& token, const std::size_t vertex_count,
                   const std::size_t uv_count, const std::size_t normal_count,
                   const std::string& path) {
    // '/'区切りで最大3要素へ分割する (空要素は「なし」を表す)
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const auto pos = token.find('/', start);
        parts.push_back(token.substr(start, pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }

    const auto v = ParseIndex(parts[0], path);
    if (!v.has_value()) {
        throw igesio::ParseError(
                "Face vertex without position index in OBJ file: " + path);
    }
    Corner corner;
    corner.v = ResolveIndex(*v, vertex_count, path);
    if (parts.size() >= 2) {
        if (const auto t = ParseIndex(parts[1], path); t.has_value()) {
            corner.t = ResolveIndex(*t, uv_count, path);
        }
    }
    if (parts.size() >= 3) {
        if (const auto n = ParseIndex(parts[2], path); n.has_value()) {
            corner.n = ResolveIndex(*n, normal_count, path);
        }
    }
    return corner;
}

/// @brief OBJ読み込みの作業状態
struct ObjBuilder {
    /// @brief ファイルから読み取った頂点位置 (1始まりのv)
    std::vector<Vec3> positions;
    /// @brief ファイルから読み取ったテクスチャ座標 (1始まりのvt)
    std::vector<Vec2> texcoords;
    /// @brief ファイルから読み取った頂点法線 (1始まりのvn)
    std::vector<Vec3> normals;

    /// @brief コーナー → 出力頂点インデックスの対応表
    std::unordered_map<CornerKey, std::uint32_t, CornerKeyHash> corner_to_index;
    /// @brief 出力頂点の位置
    std::vector<Vec3> out_positions;
    /// @brief 出力頂点のUV (out_positionsと並走. 未参照は0)
    std::vector<Vec2> out_uvs;
    /// @brief 出力頂点の法線 (out_positionsと並走. 未参照は0)
    std::vector<Vec3> out_normals;
    /// @brief いずれかのコーナーがUVを参照したか
    bool any_uv = false;
    /// @brief いずれかのコーナーが法線を参照したか
    bool any_normal = false;

    /// @brief 三角形インデックス (3要素で1三角形)
    std::vector<std::uint32_t> indices;
    /// @brief 面グループ
    std::vector<i_num::MeshGroup> groups;
    /// @brief 現在のグループ名 (g / o)
    std::string current_name;
    /// @brief 現在のマテリアル名 (usemtl)
    std::string current_material;
    /// @brief グループ文脈が有効か (g / o / usemtl が一度でも現れたか)
    bool group_active = false;

    /// @brief コーナーに対応する出力頂点インデックスを取得する (なければ作成)
    /// @param corner 解決済みコーナー
    /// @return 出力頂点インデックス
    std::uint32_t GetOrCreateVertex(const Corner& corner) {
        const CornerKey key{corner.v, corner.t, corner.n};
        const auto it = corner_to_index.find(key);
        if (it != corner_to_index.end()) return it->second;

        const auto index = static_cast<std::uint32_t>(out_positions.size());
        out_positions.push_back(positions[static_cast<std::size_t>(corner.v)]);
        out_uvs.push_back(corner.t >= 0
                ? texcoords[static_cast<std::size_t>(corner.t)]
                : Vec2::Zero());
        out_normals.push_back(corner.n >= 0
                ? normals[static_cast<std::size_t>(corner.n)]
                : Vec3::Zero());
        if (corner.t >= 0) any_uv = true;
        if (corner.n >= 0) any_normal = true;
        corner_to_index.emplace(key, index);
        return index;
    }

    /// @brief 1三角形を追加し、グループ範囲を更新する
    /// @param a 三角形の頂点0の出力インデックス
    /// @param b 三角形の頂点1の出力インデックス
    /// @param c 三角形の頂点2の出力インデックス
    void AddTriangle(const std::uint32_t a, const std::uint32_t b,
                     const std::uint32_t c) {
        const auto triangle_index =
                static_cast<std::uint32_t>(indices.size() / 3);
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
        if (!group_active) return;
        // 文脈 (名前/マテリアル) が変わったら新しいグループを開始する
        if (groups.empty() || groups.back().name != current_name ||
            groups.back().material_name != current_material) {
            i_num::MeshGroup group;
            group.name = current_name;
            group.material_name = current_material;
            group.first_triangle = triangle_index;
            group.triangle_count = 0;
            groups.push_back(group);
        }
        ++groups.back().triangle_count;
    }
};

/// @brief 面行を処理する (多角形はファン分割する)
/// @param iss 面行のうち "f" の後のトークン列
/// @param builder 作業状態
/// @param path エラーメッセージ用のファイルパス
/// @throw igesio::ParseError 面が3頂点未満、または頂点参照が不正な場合
void ProcessFace(std::istringstream& iss, ObjBuilder& builder,
                 const std::string& path) {
    std::vector<std::uint32_t> face;
    std::string token;
    while (iss >> token) {
        const auto corner = ParseCorner(token, builder.positions.size(),
                                        builder.texcoords.size(),
                                        builder.normals.size(), path);
        face.push_back(builder.GetOrCreateVertex(corner));
    }
    if (face.size() < 3) {
        throw igesio::ParseError(
                "Face with fewer than 3 vertices in OBJ file: " + path);
    }
    // 三角形ファン: (0, k, k+1)
    for (std::size_t k = 1; k + 1 < face.size(); ++k) {
        builder.AddTriangle(face[0], face[k], face[k + 1]);
    }
}

/// @brief 作業状態を三角形メッシュへ確定する
/// @param builder 作業状態
/// @return 三角形メッシュ
i_num::TriangleMeshd Finalize(ObjBuilder& builder) {
    i_num::TriangleMeshd mesh;
    const auto count = builder.out_positions.size();
    mesh.positions.resize(3, static_cast<Eigen::Index>(count));
    for (std::size_t c = 0; c < count; ++c) {
        mesh.positions.col(static_cast<Eigen::Index>(c)) =
                builder.out_positions[c];
    }
    if (builder.any_normal) {
        mesh.normals.resize(3, static_cast<Eigen::Index>(count));
        for (std::size_t c = 0; c < count; ++c) {
            mesh.normals.col(static_cast<Eigen::Index>(c)) =
                    builder.out_normals[c];
        }
    }
    if (builder.any_uv) {
        mesh.uvs.resize(2, static_cast<Eigen::Index>(count));
        for (std::size_t c = 0; c < count; ++c) {
            mesh.uvs.col(static_cast<Eigen::Index>(c)) = builder.out_uvs[c];
        }
    }
    mesh.indices = std::move(builder.indices);
    mesh.groups = std::move(builder.groups);
    return mesh;
}

/// @brief 三角形ごとに属するグループのインデックスを求める
/// @param mesh 対象のメッシュ
/// @return 各三角形のグループインデックス (グループ無しは-1. 重複時は後勝ち)
std::vector<int> BuildTriangleGroups(const i_num::TriangleMeshd& mesh) {
    std::vector<int> triangle_group(mesh.TriangleCount(), -1);
    for (std::size_t g = 0; g < mesh.groups.size(); ++g) {
        const auto& group = mesh.groups[g];
        const auto end = static_cast<std::size_t>(group.first_triangle) +
                         group.triangle_count;
        for (auto t = static_cast<std::size_t>(group.first_triangle);
             t < end && t < triangle_group.size(); ++t) {
            triangle_group[t] = static_cast<int>(g);
        }
    }
    return triangle_group;
}

/// @brief 1三角形の面行を書き出す (単一インデックス空間のためv=vt=vn)
/// @param file 出力ストリーム
/// @param mesh 対象のメッシュ
/// @param triangle 三角形のインデックス
/// @param wrote_uvs UVを出力したか
/// @param wrote_normals 法線を出力したか
void WriteFace(std::ofstream& file, const i_num::TriangleMeshd& mesh,
               const std::size_t triangle, const bool wrote_uvs,
               const bool wrote_normals) {
    file << 'f';
    for (int i = 0; i < 3; ++i) {
        // OBJは1始まり
        const auto index = mesh.indices[triangle * 3 + i] + 1;
        file << ' ' << index;
        if (wrote_uvs && wrote_normals) {
            file << '/' << index << '/' << index;
        } else if (wrote_uvs) {
            file << '/' << index;
        } else if (wrote_normals) {
            file << "//" << index;
        }
    }
    file << '\n';
}

}  // namespace



i_num::TriangleMeshd i_ext::ReadObj(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw igesio::FileOpenError("Failed to open OBJ file: " + path);
    }

    ObjBuilder builder;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyword;
        if (!(iss >> keyword)) continue;  // 空行

        if (keyword == "v") {
            Vec3 position;
            // x y z のみ使用する (w・頂点カラー等の余剰成分は無視)
            if (!(iss >> position[0] >> position[1] >> position[2])) {
                throw igesio::ParseError(
                        "Invalid 'v' record in OBJ file: " + path);
            }
            builder.positions.push_back(position);
        } else if (keyword == "vt") {
            Vec2 uv = Vec2::Zero();
            // u は必須、v は省略時0 (w成分は無視)
            if (!(iss >> uv[0])) {
                throw igesio::ParseError(
                        "Invalid 'vt' record in OBJ file: " + path);
            }
            iss >> uv[1];
            builder.texcoords.push_back(uv);
        } else if (keyword == "vn") {
            Vec3 normal;
            if (!(iss >> normal[0] >> normal[1] >> normal[2])) {
                throw igesio::ParseError(
                        "Invalid 'vn' record in OBJ file: " + path);
            }
            builder.normals.push_back(normal);
        } else if (keyword == "f") {
            ProcessFace(iss, builder, path);
        } else if (keyword == "g" || keyword == "o") {
            std::string rest;
            std::getline(iss, rest);
            builder.current_name = Trim(rest);
            builder.group_active = true;
        } else if (keyword == "usemtl") {
            std::string rest;
            std::getline(iss, rest);
            builder.current_material = Trim(rest);
            builder.group_active = true;
        }
        // mtllib / s / # / その他のキーワードは読み飛ばす
    }
    return Finalize(builder);
}

std::shared_ptr<igesio::entities::MeshEntity>
i_ext::ReadObjAsEntity(const std::string& path) {
    return std::make_shared<entities::MeshEntity>(ReadObj(path));
}

bool i_ext::WriteObj(const numerics::TriangleMeshd& mesh,
                     const std::string& path, const WriteObjParams& params) {
    // 不正なメッシュ (範囲外インデックス等) を書き出さない
    const auto validation = i_num::Validate(mesh);
    if (!validation.is_valid) {
        throw std::invalid_argument(
                "Invalid triangle mesh: " + validation.Message());
    }

    // 親ディレクトリが存在しない場合は作成する (WriteIges/WriteStlと同じ挙動)
    const auto parent = fs::path(path).parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            throw igesio::FileOpenError(
                    "Failed to create directory: " + parent.string());
        }
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw igesio::FileOpenError("Failed to open OBJ file: " + path);
    }
    // doubleの値を過不足なく往復できる桁数で出力する
    file << std::setprecision(std::numeric_limits<double>::max_digits10);

    const bool wrote_uvs = params.write_uvs && mesh.HasUVs();
    const bool wrote_normals = params.write_normals && mesh.HasNormals();
    const bool wrote_groups = params.write_groups && !mesh.groups.empty();

    file << "# IGESio OBJ export\n";

    // 頂点位置 → UV → 法線 の順に出力する
    for (Eigen::Index c = 0; c < mesh.positions.cols(); ++c) {
        file << "v " << mesh.positions(0, c) << ' ' << mesh.positions(1, c)
             << ' ' << mesh.positions(2, c) << '\n';
    }
    if (wrote_uvs) {
        for (Eigen::Index c = 0; c < mesh.uvs.cols(); ++c) {
            file << "vt " << mesh.uvs(0, c) << ' ' << mesh.uvs(1, c) << '\n';
        }
    }
    if (wrote_normals) {
        for (Eigen::Index c = 0; c < mesh.normals.cols(); ++c) {
            file << "vn " << mesh.normals(0, c) << ' ' << mesh.normals(1, c)
                 << ' ' << mesh.normals(2, c) << '\n';
        }
    }

    if (!wrote_groups) {
        for (std::size_t t = 0; t < mesh.TriangleCount(); ++t) {
            WriteFace(file, mesh, t, wrote_uvs, wrote_normals);
        }
    } else {
        // 三角形順に走査し、グループの境界で g / usemtl を挟む
        const auto triangle_group = BuildTriangleGroups(mesh);
        int current = -2;  // 直前に出力したグループ (-2=未出力, -1=グループ無し)
        for (std::size_t t = 0; t < mesh.TriangleCount(); ++t) {
            if (triangle_group[t] != current) {
                current = triangle_group[t];
                if (current >= 0) {
                    const auto& group =
                            mesh.groups[static_cast<std::size_t>(current)];
                    file << "g " << group.name << '\n';
                    if (!group.material_name.empty()) {
                        file << "usemtl " << group.material_name << '\n';
                    }
                }
            }
            WriteFace(file, mesh, t, wrote_uvs, wrote_normals);
        }
    }
    return file.good();
}
