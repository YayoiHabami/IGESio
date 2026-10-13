/**
 * @file extensions/obj/obj_io.cpp
 * @brief OBJ（Wavefront OBJ）ファイルの入出力
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/obj/obj_io.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/meshes/algorithms.h"
#include "igesio/utils/path_encoding.h"
#include "extensions/common/text_scanner.h"

namespace {

namespace fs = std::filesystem;
namespace i_ext = igesio::extensions;
namespace i_detail = igesio::extensions::detail;
namespace i_num = igesio::numerics;
using Vec3 = Eigen::Vector3d;
using Vec2 = Eigen::Vector2d;

/// @brief ファイルを読み込むブロックの大きさ [byte]
/// @note ファイル全体を一度に読み込むとファイルの大きさだけメモリを使うため、
///       一定の大きさずつ読み込む
constexpr std::size_t kReadBlockSize = std::size_t(1) << 20;

/// @brief どの出力頂点も指さないことを表す番号
constexpr std::uint32_t kNoVertex = 0xffffffffU;

/// @brief 0始まりのインデックスに変換したコーナー（面頂点）の参照
/// @note 各成分は0始まりのインデックス. -1はそのチャンネルを参照しないことを表す
struct Corner {
    /// @brief 頂点位置のインデックス
    std::int64_t v = -1;
    /// @brief テクスチャ座標のインデックス（参照しない場合は-1）
    std::int64_t t = -1;
    /// @brief 頂点法線のインデックス（参照しない場合は-1）
    std::int64_t n = -1;
};

/// @brief 文字列の前後の空白を取り除く
/// @param text 対象の文字列
/// @return 前後の空白を除いた部分（textの参照）
std::string_view Trim(const std::string_view text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) return {};
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

/// @brief OBJのインデックス文字列を解析する
/// @param part インデックス文字列（インデックスの指定なしの場合は空文字列）
/// @param path エラーメッセージ用のファイルパス
/// @return 解析した1始まり/負のインデックス. 空文字列の場合はstd::nullopt
/// @throw igesio::ParseError 数値として解釈できない場合
std::optional<std::int64_t> ParseIndex(const std::string_view part,
                                       const std::string& path) {
    if (part.empty()) return std::nullopt;
    std::int64_t value = 0;
    const auto error = i_detail::ParseInteger(part, value);
    if (error == std::errc::result_out_of_range) {
        throw igesio::ParseError("Face index out of range '" +
                                 std::string(part) + "' in OBJ file: " + path);
    }
    if (error != std::errc()) {
        throw igesio::ParseError("Invalid face index '" + std::string(part) +
                                 "' in OBJ file: " + path);
    }
    return value;
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

/// @brief 面頂点トークン (v、v/vt、v//vn、v/vt/vn) を
///        0始まりのインデックスのコーナーに変換する
/// @param token 面頂点トークン
/// @param vertex_count 位置配列の現在の要素数
/// @param uv_count テクスチャ座標配列の現在の要素数
/// @param normal_count 法線配列の現在の要素数
/// @param path エラーメッセージ用のファイルパス
/// @return 変換後のコーナー
/// @throw igesio::ParseError トークンが不正な場合
Corner ParseCorner(const std::string_view token, const std::size_t vertex_count,
                   const std::size_t uv_count, const std::size_t normal_count,
                   const std::string& path) {
    // '/'区切りの先頭3要素を取り出す.
    // 空要素ならその属性の指定はなし. 4要素目以降は使わない
    std::array<std::string_view, 3> parts;
    std::size_t part_count = 0;
    std::size_t start = 0;
    while (true) {
        const auto pos = token.find('/', start);
        if (part_count < parts.size()) {
            parts[part_count] = token.substr(
                    start, pos == std::string_view::npos ? pos : pos - start);
        }
        ++part_count;
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }

    const auto v = ParseIndex(parts[0], path);
    if (!v.has_value()) {
        throw igesio::ParseError(
                "Face vertex without position index in OBJ file: " + path);
    }
    Corner corner;
    corner.v = ResolveIndex(*v, vertex_count, path);
    if (part_count >= 2) {
        if (const auto t = ParseIndex(parts[1], path); t.has_value()) {
            corner.t = ResolveIndex(*t, uv_count, path);
        }
    }
    if (part_count >= 3) {
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

    /// @brief 位置インデックスごとの、その位置を参照する最初の出力頂点
    /// @note kNoVertexは参照する出力頂点がないことを表す. 同じ位置を参照する出力頂点
    ///       （UV/法線の組が異なるもの）はnext_vertex_with_positionで順に参照する
    std::vector<std::uint32_t> first_vertex_of_position;
    /// @brief 出力頂点ごとの、同じ位置を参照する次の出力頂点
    /// @note 存在しない場合はkNoVertex
    std::vector<std::uint32_t> next_vertex_with_position;
    /// @brief 出力頂点ごとの、元のコーナーのテクスチャ座標のインデックス
    /// @note 参照しない場合は-1
    std::vector<std::int64_t> vertex_uv_index;
    /// @brief 出力頂点ごとの、元のコーナーの法線のインデックス
    /// @note 参照しない場合は-1
    std::vector<std::int64_t> vertex_normal_index;
    /// @brief 出力頂点の位置
    std::vector<Vec3> out_positions;
    /// @brief 出力頂点のUV (インデックスはout_positionsと共通. 未参照は0)
    /// @note いずれかのコーナーがUVを参照するまでは空のままとする
    std::vector<Vec2> out_uvs;
    /// @brief 出力頂点の法線 (インデックスはout_positionsと共通. 未参照は0)
    /// @note いずれかのコーナーが法線を参照するまでは空のままとする
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
    /// @brief グループの指定が有効か (g、o、usemtlが一度でも現れたか)
    bool group_active = false;
    /// @brief 最後の三角形の追加以降に、g、o、usemtlが現れたか
    bool group_context_changed = false;
    /// @brief 面の行（fの行）の出力頂点インデックス（fの行ごとに再利用する作業領域）
    std::vector<std::uint32_t> face;

    /// @brief ファイルの大きさから各配列の要素数を見積もって領域を確保する
    /// @param file_size ファイルの大きさ [byte]
    void ReserveForFileSize(const std::size_t file_size) {
        // 1行あたり約40バイト、行の約1/3が頂点行と見積もる
        const std::size_t lines = file_size / 40;
        positions.reserve(lines / 3);
        out_positions.reserve(lines / 3);
        indices.reserve(lines);
    }

    /// @brief コーナーに対応する出力頂点を新たに作成する
    /// @param corner 変換後のコーナー
    /// @return 作成した出力頂点のインデックス
    std::uint32_t CreateVertex(const Corner& corner) {
        const auto index = static_cast<std::uint32_t>(out_positions.size());
        out_positions.push_back(positions[static_cast<std::size_t>(corner.v)]);
        // UV/法線は初めて参照された時点で、既存の出力頂点の分を0で埋めて作る
        if (corner.t >= 0 && !any_uv) {
            out_uvs.assign(index, Vec2::Zero());
            any_uv = true;
        }
        if (corner.n >= 0 && !any_normal) {
            out_normals.assign(index, Vec3::Zero());
            any_normal = true;
        }
        if (any_uv) {
            out_uvs.push_back(corner.t >= 0
                    ? texcoords[static_cast<std::size_t>(corner.t)]
                    : Vec2::Zero());
        }
        if (any_normal) {
            out_normals.push_back(corner.n >= 0
                    ? normals[static_cast<std::size_t>(corner.n)]
                    : Vec3::Zero());
        }
        vertex_uv_index.push_back(corner.t);
        vertex_normal_index.push_back(corner.n);
        next_vertex_with_position.push_back(kNoVertex);
        return index;
    }

    /// @brief コーナーに対応する出力頂点インデックスを取得する (なければ作成)
    /// @param corner 変換後のコーナー
    /// @return 出力頂点インデックス
    /// @note 出力頂点のインデックスは、(位置, UV, 法線)の組が初めて現れた順に付ける
    std::uint32_t GetOrCreateVertex(const Corner& corner) {
        if (first_vertex_of_position.size() < positions.size()) {
            first_vertex_of_position.resize(positions.size(), kNoVertex);
        }
        const auto position = static_cast<std::size_t>(corner.v);
        auto previous = kNoVertex;
        for (auto vertex = first_vertex_of_position[position];
             vertex != kNoVertex; vertex = next_vertex_with_position[vertex]) {
            if (vertex_uv_index[vertex] == corner.t &&
                vertex_normal_index[vertex] == corner.n) {
                return vertex;
            }
            previous = vertex;
        }

        const auto index = CreateVertex(corner);
        if (previous == kNoVertex) {
            first_vertex_of_position[position] = index;
        } else {
            next_vertex_with_position[previous] = index;
        }
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
        // グループ名またはマテリアル名が変わったら新しいグループを開始する.
        // 名前の比較は、前の三角形の後にg、o、usemtlが出てきた場合だけ行う
        const bool changed =
                groups.empty() ||
                (group_context_changed &&
                 (groups.back().name != current_name ||
                  groups.back().material_name != current_material));
        group_context_changed = false;
        if (changed) {
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

/// @brief fの行を処理する（多角形は先頭頂点を共有する三角形に分割する）
/// @param[in,out] scanner TextScanner（fの行のうち"f"の後を読み込む）
/// @param builder 作業状態
/// @param path エラーメッセージ用のファイルパス
/// @throw igesio::ParseError 面が3頂点未満、または頂点参照が不正な場合
void ProcessFace(i_detail::TextScanner& scanner, ObjBuilder& builder,
                 const std::string& path) {
    auto& face = builder.face;
    face.clear();
    for (auto token = scanner.NextToken(); !token.empty();
         token = scanner.NextToken()) {
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

/// @brief 空白で区切られた3つの実数を読み取る
/// @param[in,out] scanner TextScanner
/// @param[out] vector 読み取ったベクトル
/// @return 3つとも読み取れた場合はtrue
bool ReadVector(i_detail::TextScanner& scanner, Vec3& vector) {
    return scanner.ReadReals(vector.data(), 3);
}

/// @brief OBJの1行を処理する
/// @param line 行の内容（改行文字を含まない）
/// @param builder 作業状態
/// @param path エラーメッセージ用のファイルパス
/// @throw igesio::ParseError v、vt、vn、fの行が不正な場合
void ProcessLine(const std::string_view line, ObjBuilder& builder,
                 const std::string& path) {
    i_detail::TextScanner scanner(line);
    const auto keyword = scanner.NextToken();
    if (keyword == "v") {
        // x y z のみ使用する (w・頂点カラー等の余剰成分は無視)
        Vec3 position;
        if (!ReadVector(scanner, position)) {
            throw igesio::ParseError("Invalid 'v' record in OBJ file: " + path);
        }
        builder.positions.push_back(position);
    } else if (keyword == "vt") {
        // uは必須、vは省略時0（w成分は無視）. vの読み取りに失敗した場合も
        // エラーにせず、読み取り関数が設定した値（通常は0）を使う
        Vec2 uv = Vec2::Zero();
        if (!scanner.ReadReal(uv[0])) {
            throw igesio::ParseError("Invalid 'vt' record in OBJ file: " + path);
        }
        scanner.ReadReal(uv[1]);
        builder.texcoords.push_back(uv);
    } else if (keyword == "vn") {
        Vec3 normal;
        if (!ReadVector(scanner, normal)) {
            throw igesio::ParseError("Invalid 'vn' record in OBJ file: " + path);
        }
        builder.normals.push_back(normal);
    } else if (keyword == "f") {
        ProcessFace(scanner, builder, path);
    } else if (keyword == "g" || keyword == "o" || keyword == "usemtl") {
        const auto rest = Trim(scanner.Rest());
        auto& target = (keyword == "usemtl") ? builder.current_material
                                             : builder.current_name;
        target.assign(rest.data(), rest.size());
        builder.group_active = true;
        builder.group_context_changed = true;
    }
    // 空行、mtllib、s、#、その他のキーワードは無視する
}

/// @brief ファイルを一定の大きさずつ読み込み、各行を処理する
/// @tparam LineHandler void(std::string_view)またはそれと変換可能な関数
/// @param file 読み込むファイル（バイナリモードで開き、先頭に位置すること）
/// @param handle_line 1行（改行文字を含まない）を処理する関数
/// @note ブロックの末尾で途切れた行は次のブロックに繰り越す. ブロックより
///       長い行は、改行が現れるまでバッファを広げて読み込む. 読み込みに
///       失敗した場合はファイルの終端と同様に扱う
template <typename LineHandler>
void ForEachLineInFile(std::ifstream& file, const LineHandler& handle_line) {
    std::vector<char> buffer(kReadBlockSize);
    // 前のブロックから繰り越した、行の途中までの文字数
    std::size_t carried = 0;
    while (true) {
        if (buffer.size() < carried + kReadBlockSize) {
            buffer.resize(carried + kReadBlockSize);
        }
        file.read(buffer.data() + carried,
                  static_cast<std::streamsize>(kReadBlockSize));
        const bool at_end = !file;
        const char* line = buffer.data();
        const char* const end =
                line + carried + static_cast<std::size_t>(file.gcount());

        for (const char* newline = std::find(line, end, '\n'); newline != end;
             newline = std::find(line, end, '\n')) {
            handle_line(std::string_view(
                    line, static_cast<std::size_t>(newline - line)));
            line = newline + 1;
        }
        carried = static_cast<std::size_t>(end - line);
        if (at_end) {
            // 改行で終わらない最終行
            if (carried > 0) handle_line(std::string_view(line, carried));
            return;
        }
        std::copy(line, end, buffer.data());
    }
}

/// @brief 列ベクトルの配列を、各列が1要素の行列に変換する
/// @tparam Rows 列ベクトルの次元
/// @param columns 列ベクトルの配列
/// @return Rows × columns.size()の行列
template <int Rows>
Eigen::Matrix<double, Rows, Eigen::Dynamic> ToColumnMatrix(
        const std::vector<Eigen::Matrix<double, Rows, 1>>& columns) {
    Eigen::Matrix<double, Rows, Eigen::Dynamic> matrix(
            Rows, static_cast<Eigen::Index>(columns.size()));
    for (std::size_t c = 0; c < columns.size(); ++c) {
        matrix.col(static_cast<Eigen::Index>(c)) = columns[c];
    }
    return matrix;
}

/// @brief 作業状態を三角形メッシュへ確定する
/// @param builder 作業状態
/// @return 三角形メッシュ
i_num::TriangleMeshd Finalize(ObjBuilder& builder) {
    i_num::TriangleMeshd mesh;
    mesh.positions = ToColumnMatrix(builder.out_positions);
    if (builder.any_normal) mesh.normals = ToColumnMatrix(builder.out_normals);
    if (builder.any_uv) mesh.uvs = ToColumnMatrix(builder.out_uvs);
    mesh.indices = std::move(builder.indices);
    mesh.groups = std::move(builder.groups);
    return mesh;
}

/// @brief 三角形ごとに属するグループのインデックスを求める
/// @param mesh 対象のメッシュ
/// @return 各三角形のグループインデックス（グループ無しは-1. 重複時は後のグループ）
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

/// @brief 1三角形のfの行を書き出す（位置・UV・法線は共通のインデックスのためv=vt=vn）
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
    std::ifstream file(igesio::utils::PathFromUtf8(path),
                       std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw igesio::FileOpenError("Failed to open OBJ file: " + path);
    }
    const auto end_position = file.tellg();
    file.seekg(0, std::ios::beg);

    ObjBuilder builder;
    if (end_position > 0) {
        builder.ReserveForFileSize(static_cast<std::size_t>(end_position));
    }
    ForEachLineInFile(file, [&builder, &path](const std::string_view line) {
        ProcessLine(line, builder, path);
    });
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
    const auto parent = igesio::utils::PathFromUtf8(path).parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            throw igesio::FileOpenError(
                    "Failed to create directory: " +
                    igesio::utils::PathToUtf8(parent));
        }
    }

    std::ofstream file(igesio::utils::PathFromUtf8(path), std::ios::trunc);
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
