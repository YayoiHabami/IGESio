/**
 * @file extensions/stl/stl_io.cpp
 * @brief STL (stereolithography) ファイルの入出力
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/stl/stl_io.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/mesh/algorithms.h"

namespace {

namespace fs = std::filesystem;
namespace i_ext = igesio::extensions;
namespace i_num = igesio::numerics;
using Vec3f = Eigen::Vector3f;

/// @brief バイナリSTLの固定部サイズ (80バイトヘッダ + uint32三角形数)
constexpr std::size_t kBinaryHeaderSize = 84;
/// @brief バイナリSTLの1三角形あたりのバイト数 (法線3f + 頂点9f + 属性uint16)
constexpr std::size_t kBinaryTriangleSize = 50;

/// @brief 読み込んだ三角形スープ (3頂点/三角形 + 面法線/三角形)
struct StlSoup {
    /// @brief 頂点座標 (3頂点 × 三角形数)
    std::vector<Vec3f> vertices;
    /// @brief 面法線 (1法線 × 三角形数)
    std::vector<Vec3f> facet_normals;
};

/// @brief ファイル全体をバイト列として読み込む
/// @param path 読み込むファイルのパス
/// @return ファイルの内容
/// @throw igesio::FileOpenError ファイルが開けなかった場合
std::vector<char> ReadAllBytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw igesio::FileOpenError("Failed to open STL file: " + path);
    }
    const auto size = static_cast<std::size_t>(file.tellg());
    std::vector<char> bytes(size);
    file.seekg(0, std::ios::beg);
    if (size > 0) file.read(bytes.data(), static_cast<std::streamsize>(size));
    return bytes;
}

/// @brief バイト列からリトルエンディアンの値を読み取る
/// @tparam T 読み取る型 (uint32_t / float)
/// @param bytes 読み取り元
/// @param offset 読み取り開始位置
/// @return 読み取った値
/// @note 本実装はリトルエンディアン環境 (x86/x64/ARM64) を前提とする
template <typename T>
T ReadLE(const std::vector<char>& bytes, const std::size_t offset) {
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

/// @brief バイト列がバイナリSTLか判定する
/// @param bytes ファイルの内容
/// @return バイナリSTLの場合はtrue
/// @note ヘッダの文字列 ("solid"等) には依存せず、ファイルサイズと三角形数の
///       整合のみで判定する (バイナリのヘッダが"solid"で始まる場合に備える)
bool IsBinaryStl(const std::vector<char>& bytes) {
    if (bytes.size() < kBinaryHeaderSize) return false;
    const auto count = ReadLE<std::uint32_t>(bytes, 80);
    return bytes.size() ==
           kBinaryHeaderSize + kBinaryTriangleSize * static_cast<std::size_t>(count);
}

/// @brief バイナリSTLを三角形スープへ読み込む
/// @param bytes ファイルの内容 (IsBinaryStlで判定済み)
/// @return 三角形スープ
StlSoup ReadBinarySoup(const std::vector<char>& bytes) {
    const auto count = ReadLE<std::uint32_t>(bytes, 80);
    StlSoup soup;
    soup.vertices.reserve(count * 3u);
    soup.facet_normals.reserve(count);

    std::size_t offset = kBinaryHeaderSize;
    for (std::uint32_t t = 0; t < count; ++t) {
        Vec3f normal;
        for (int i = 0; i < 3; ++i) {
            normal[i] = ReadLE<float>(bytes, offset + i * sizeof(float));
        }
        soup.facet_normals.push_back(normal);
        for (int v = 0; v < 3; ++v) {
            Vec3f vertex;
            for (int i = 0; i < 3; ++i) {
                vertex[i] = ReadLE<float>(
                        bytes, offset + (3 + v * 3 + i) * sizeof(float));
            }
            soup.vertices.push_back(vertex);
        }
        offset += kBinaryTriangleSize;  // 末尾2バイトの属性はスキップ
    }
    return soup;
}

/// @brief ASCII STLを三角形スープへ読み込む
/// @param bytes ファイルの内容
/// @param path エラーメッセージ用のファイルパス
/// @return 三角形スープ
/// @throw igesio::ParseError ASCII STLとして不正な場合
StlSoup ReadAsciiSoup(const std::vector<char>& bytes, const std::string& path) {
    std::istringstream stream(std::string(bytes.begin(), bytes.end()));
    StlSoup soup;
    Vec3f current_normal = Vec3f::Zero();

    std::string token;
    bool found_solid = false;
    while (stream >> token) {
        if (token == "solid") {
            found_solid = true;
            // solid名 (行末まで) はスキップ
            std::string rest;
            std::getline(stream, rest);
        } else if (token == "facet") {
            // "facet normal nx ny nz"
            stream >> token;  // "normal"
            if (token != "normal" ||
                !(stream >> current_normal[0] >> current_normal[1]
                         >> current_normal[2])) {
                throw igesio::ParseError(
                        "Invalid 'facet normal' record in STL file: " + path);
            }
        } else if (token == "vertex") {
            Vec3f vertex;
            if (!(stream >> vertex[0] >> vertex[1] >> vertex[2])) {
                throw igesio::ParseError(
                        "Invalid 'vertex' record in STL file: " + path);
            }
            soup.vertices.push_back(vertex);
            // 3頂点毎に1三角形 (facet normalは直前のfacet行のもの)
            if (soup.vertices.size() % 3 == 0) {
                soup.facet_normals.push_back(current_normal);
            }
        }
        // outer/loop/endloop/endfacet/endsolid等のキーワードは読み飛ばす
    }

    if (!found_solid || soup.vertices.size() % 3 != 0) {
        throw igesio::ParseError(
                "File is not a valid STL (neither binary nor ASCII): " + path);
    }
    return soup;
}

/// @brief 溶接キー (量子化した座標の組)
using WeldKey = std::tuple<std::int64_t, std::int64_t, std::int64_t>;

/// @brief WeldKeyのハッシュ関数
struct WeldKeyHash {
    /// @brief ハッシュ値を計算する
    std::size_t operator()(const WeldKey& key) const {
        constexpr std::uint64_t kGolden = 0x9e3779b97f4a7c15ULL;
        std::uint64_t seed = 0;
        for (const auto v : {std::get<0>(key), std::get<1>(key),
                             std::get<2>(key)}) {
            seed ^= static_cast<std::uint64_t>(v) + kGolden +
                    (seed << 6) + (seed >> 2);
        }
        return static_cast<std::size_t>(seed);
    }
};

/// @brief 頂点の溶接キーを計算する
/// @param vertex 頂点座標
/// @param tolerance 許容距離 (0以下の場合はビット単位の完全一致)
/// @return 溶接キー
WeldKey MakeWeldKey(const Vec3f& vertex, const double tolerance) {
    WeldKey key;
    if (tolerance > 0.0) {
        key = {static_cast<std::int64_t>(std::llround(vertex[0] / tolerance)),
               static_cast<std::int64_t>(std::llround(vertex[1] / tolerance)),
               static_cast<std::int64_t>(std::llround(vertex[2] / tolerance))};
    } else {
        // 完全一致: floatのビット表現をそのままキーにする
        std::uint32_t bits[3];
        std::memcpy(bits, vertex.data(), sizeof(bits));
        key = {bits[0], bits[1], bits[2]};
    }
    return key;
}

/// @brief 三角形スープを溶接してインデックスメッシュへ変換する
/// @param soup 三角形スープ
/// @param tolerance 溶接の許容距離
/// @return 共有頂点化したメッシュ (法線チャンネルなし)
i_num::TriangleMeshf WeldSoup(const StlSoup& soup, const double tolerance) {
    i_num::TriangleMeshf mesh;
    std::unordered_map<WeldKey, std::uint32_t, WeldKeyHash> index_of;
    std::vector<Vec3f> unique_vertices;
    mesh.indices.reserve(soup.vertices.size());

    for (const auto& vertex : soup.vertices) {
        const auto key = MakeWeldKey(vertex, tolerance);
        auto it = index_of.find(key);
        if (it == index_of.end()) {
            const auto index = static_cast<std::uint32_t>(unique_vertices.size());
            index_of.emplace(key, index);
            unique_vertices.push_back(vertex);
            mesh.indices.push_back(index);
        } else {
            mesh.indices.push_back(it->second);
        }
    }

    mesh.positions.resize(3, static_cast<Eigen::Index>(unique_vertices.size()));
    for (std::size_t c = 0; c < unique_vertices.size(); ++c) {
        mesh.positions.col(static_cast<Eigen::Index>(c)) = unique_vertices[c];
    }
    return mesh;
}

/// @brief 三角形スープを溶接せずにインデックスメッシュへ変換する
/// @param soup 三角形スープ
/// @return 3頂点/三角形のままのメッシュ (面法線を各頂点へ展開する)
i_num::TriangleMeshf ExpandSoup(const StlSoup& soup) {
    i_num::TriangleMeshf mesh;
    const auto vertex_count = soup.vertices.size();
    mesh.positions.resize(3, static_cast<Eigen::Index>(vertex_count));
    mesh.normals.resize(3, static_cast<Eigen::Index>(vertex_count));
    mesh.indices.resize(vertex_count);
    for (std::size_t v = 0; v < vertex_count; ++v) {
        const auto col = static_cast<Eigen::Index>(v);
        mesh.positions.col(col) = soup.vertices[v];
        mesh.normals.col(col) = soup.facet_normals[v / 3];
        mesh.indices[v] = static_cast<std::uint32_t>(v);
    }
    return mesh;
}

/// @brief 三角形の幾何から面法線 (単位ベクトル) を計算する
/// @param v0 頂点0
/// @param v1 頂点1
/// @param v2 頂点2
/// @return 面法線. 退化三角形の場合はゼロベクトル
Vec3f ComputeFacetNormal(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2) {
    const Vec3f cross = (v1 - v0).cross(v2 - v0);
    const float norm = cross.norm();
    if (norm <= 0.0f) return Vec3f::Zero();
    return cross / norm;
}

/// @brief バイナリSTLとして書き出す
/// @param mesh 書き出すメッシュ
/// @param file 出力ストリーム (バイナリモードで開かれていること)
void WriteBinaryStl(const i_num::TriangleMeshf& mesh, std::ofstream& file) {
    // 80バイトヘッダ (空き領域はゼロ埋め)
    char header[80] = {};
    constexpr char kHeaderText[] = "IGESio STL export";
    std::memcpy(header, kHeaderText, sizeof(kHeaderText));
    file.write(header, sizeof(header));

    const auto count = static_cast<std::uint32_t>(mesh.TriangleCount());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));

    const std::uint16_t attribute = 0;
    for (std::size_t t = 0; t < mesh.TriangleCount(); ++t) {
        const Vec3f v0 = mesh.positions.col(mesh.indices[t * 3]);
        const Vec3f v1 = mesh.positions.col(mesh.indices[t * 3 + 1]);
        const Vec3f v2 = mesh.positions.col(mesh.indices[t * 3 + 2]);
        const Vec3f normal = ComputeFacetNormal(v0, v1, v2);

        file.write(reinterpret_cast<const char*>(normal.data()),
                   3 * sizeof(float));
        file.write(reinterpret_cast<const char*>(v0.data()), 3 * sizeof(float));
        file.write(reinterpret_cast<const char*>(v1.data()), 3 * sizeof(float));
        file.write(reinterpret_cast<const char*>(v2.data()), 3 * sizeof(float));
        file.write(reinterpret_cast<const char*>(&attribute),
                   sizeof(attribute));
    }
}

/// @brief ASCII STLとして書き出す
/// @param mesh 書き出すメッシュ
/// @param file 出力ストリーム
void WriteAsciiStl(const i_num::TriangleMeshf& mesh, std::ofstream& file) {
    // floatの値を過不足なく往復できる桁数で出力する
    file << std::setprecision(std::numeric_limits<float>::max_digits10);

    file << "solid igesio\n";
    for (std::size_t t = 0; t < mesh.TriangleCount(); ++t) {
        const Vec3f v0 = mesh.positions.col(mesh.indices[t * 3]);
        const Vec3f v1 = mesh.positions.col(mesh.indices[t * 3 + 1]);
        const Vec3f v2 = mesh.positions.col(mesh.indices[t * 3 + 2]);
        const Vec3f normal = ComputeFacetNormal(v0, v1, v2);

        file << "  facet normal " << normal[0] << ' ' << normal[1] << ' '
             << normal[2] << '\n';
        file << "    outer loop\n";
        for (const auto& v : {v0, v1, v2}) {
            file << "      vertex " << v[0] << ' ' << v[1] << ' ' << v[2]
                 << '\n';
        }
        file << "    endloop\n";
        file << "  endfacet\n";
    }
    file << "endsolid igesio\n";
}

}  // namespace



igesio::numerics::TriangleMeshf i_ext::ReadStl(const std::string& path,
                                               const StlReadParams& params) {
    const auto bytes = ReadAllBytes(path);

    const auto soup = IsBinaryStl(bytes) ? ReadBinarySoup(bytes)
                                         : ReadAsciiSoup(bytes, path);
    return params.weld_vertices ? WeldSoup(soup, params.weld_tolerance)
                                : ExpandSoup(soup);
}

std::shared_ptr<igesio::entities::MeshEntity>
i_ext::ReadStlAsEntity(const std::string& path, const StlReadParams& params) {
    // CPU正準値の方針 (double) に合わせて変換して保持する
    return std::make_shared<entities::MeshEntity>(
            i_num::CastScalar<double>(ReadStl(path, params)));
}

bool i_ext::WriteStl(const numerics::TriangleMeshf& mesh,
                     const std::string& path, const bool binary) {
    // 不正なメッシュ (範囲外インデックス等) を書き出さない
    const auto validation = i_num::Validate(mesh);
    if (!validation.is_valid) {
        throw std::invalid_argument(
                "Invalid triangle mesh: " + validation.Message());
    }

    // 親ディレクトリが存在しない場合は作成する (WriteIgesと同じ挙動)
    const auto parent = fs::path(path).parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            throw igesio::FileOpenError(
                    "Failed to create directory: " + parent.string());
        }
    }

    std::ofstream file(path, binary ? (std::ios::binary | std::ios::trunc)
                                    : std::ios::trunc);
    if (!file.is_open()) {
        throw igesio::FileOpenError("Failed to open STL file: " + path);
    }

    if (binary) {
        WriteBinaryStl(mesh, file);
    } else {
        WriteAsciiStl(mesh, file);
    }
    return file.good();
}
