/**
 * @file extensions/stl/stl_io.cpp
 * @brief STL（Stereolithography）ファイルの入出力
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/stl/stl_io.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
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
using Vec3f = Eigen::Vector3f;

/// @brief バイナリSTLの固定部サイズ (80バイトヘッダ + uint32三角形数)
constexpr std::size_t kBinaryHeaderSize = 84;
/// @brief バイナリSTLの1三角形あたりのバイト数 (法線3f + 頂点9f + 属性uint16)
constexpr std::size_t kBinaryTriangleSize = 50;

/// @brief 読み込んだポリゴンスープ (3頂点/三角形 + 面法線/三角形)
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
    std::ifstream file(igesio::utils::PathFromUtf8(path),
                       std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw igesio::FileOpenError("Failed to open STL file: " + path);
    }
    const auto size = static_cast<std::size_t>(file.tellg());
    std::vector<char> bytes(size);
    file.seekg(0, std::ios::beg);
    if (size > 0) file.read(bytes.data(), static_cast<std::streamsize>(size));
    return bytes;
}

/// @brief バイト列から値を読み取る
/// @tparam T 読み取る型（uint32_t/float）
/// @param bytes 読み取り元
/// @param offset 読み取り開始位置
/// @return 読み取った値
/// @note リトルエンディアンを前提とする
template <typename T>
T ReadLE(const std::vector<char>& bytes, const std::size_t offset) {
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

/// @brief バイト列がバイナリSTLか判定する
/// @param bytes ファイルの内容
/// @return バイナリSTLの場合はtrue
/// @note バイナリのヘッダが"solid"で始まる場合でも読めるように,
///       ヘッダの文字列（"solid"等）には依存せず、ファイルサイズと三角形数の
///       整合性で判定する
bool IsBinaryStl(const std::vector<char>& bytes) {
    if (bytes.size() < kBinaryHeaderSize) return false;
    const auto count = ReadLE<std::uint32_t>(bytes, 80);
    return bytes.size() ==
           kBinaryHeaderSize + kBinaryTriangleSize * static_cast<std::size_t>(count);
}

/// @brief バイナリSTLをポリゴンスープへ読み込む
/// @param bytes ファイルの内容（IsBinaryStlで判定済み）
/// @return ポリゴンスープ
StlSoup ReadBinarySoup(const std::vector<char>& bytes) {
    const auto count = ReadLE<std::uint32_t>(bytes, 80);
    StlSoup soup;
    soup.vertices.resize(static_cast<std::size_t>(count) * 3);
    soup.facet_normals.resize(count);

    // 1三角形分の法線と3頂点（計12個のfloat）を読み込む
    const char* record = bytes.data() + kBinaryHeaderSize;
    for (std::size_t t = 0; t < count; ++t) {
        std::array<float, 12> values;
        std::memcpy(values.data(), record, sizeof(values));
        soup.facet_normals[t] = Vec3f(values[0], values[1], values[2]);
        for (std::size_t v = 0; v < 3; ++v) {
            soup.vertices[3 * t + v] = Vec3f(values[3 + 3 * v],
                                             values[4 + 3 * v],
                                             values[5 + 3 * v]);
        }
        record += kBinaryTriangleSize;  // 末尾2バイトの属性はスキップ
    }
    return soup;
}

/// @brief 空白で区切られた3つの実数を読み取る
/// @param[in,out] scanner TextScanner
/// @param[out] vector 読み取ったベクトル
/// @return 3つとも読み取れた場合はtrue
bool ReadVector(i_detail::TextScanner& scanner, Vec3f& vector) {
    return scanner.ReadReals(vector.data(), 3);
}

/// @brief ASCII STLをポリゴンスープへ読み込む
/// @param bytes ファイルの内容
/// @param path エラーメッセージ用のファイルパス
/// @return ポリゴンスープ
/// @throw igesio::ParseError ASCII STLとして不正な場合
StlSoup ReadAsciiSoup(const std::vector<char>& bytes, const std::string& path) {
    i_detail::TextScanner scanner(std::string_view(bytes.data(), bytes.size()));
    StlSoup soup;
    // 1三角形あたり約250バイトとして確保する要素数を見積もる
    soup.vertices.reserve(bytes.size() / 250 * 3);
    soup.facet_normals.reserve(bytes.size() / 250);
    Vec3f current_normal = Vec3f::Zero();

    bool found_solid = false;
    for (auto token = scanner.NextToken(); !token.empty();
         token = scanner.NextToken()) {
        if (token == "solid") {
            found_solid = true;
            // solid名 (行末まで) はスキップ
            scanner.SkipToLineEnd();
        } else if (token == "facet") {
            // "facet normal nx ny nz"
            if (scanner.NextToken() != "normal" ||
                !ReadVector(scanner, current_normal)) {
                throw igesio::ParseError(
                        "Invalid 'facet normal' record in STL file: " + path);
            }
        } else if (token == "vertex") {
            Vec3f vertex;
            if (!ReadVector(scanner, vertex)) {
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

/// @brief 頂点を統合する際の判定用キー（量子化した座標の組）
/// @note 許容距離が0より大きければ、頂点のx,y,z座標を許容距離で割った結果
///       （整数値）、許容距離が0以下であれば、x,y,z座標floatのビット表現.
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

/// @brief WeldKeyを計算する
/// @param vertex 頂点座標
/// @param tolerance 許容距離 (0以下の場合はビット単位の完全一致)
/// @return WeldKey
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

/// @brief 頂点を統合する際の許容距離を決める
/// @param soup ポリゴンスープ
/// @param params 読み込み設定
/// @return 許容距離. 0以下の場合はビット単位の完全一致の頂点のみを統合する
/// @note 絶対値 (weld_tolerance) の指定を優先し、未指定の場合は
///       バウンディングボックスの対角長に相対値を掛けた値とする. 頂点が無い場合や
///       全頂点が一致して対角長がゼロになる場合は完全一致のみ統合する
double ResolveWeldTolerance(const StlSoup& soup,
                            const i_ext::StlReadParams& params) {
    if (params.weld_tolerance > 0.0) return params.weld_tolerance;
    if (params.weld_relative_tolerance <= 0.0 || soup.vertices.empty()) {
        return 0.0;
    }

    // Eigenの式（cwiseMin等）はデバッグビルドで遅いため成分ごとに比較
    std::array<float, 3> lower = {soup.vertices.front()[0],
                                  soup.vertices.front()[1],
                                  soup.vertices.front()[2]};
    std::array<float, 3> upper = lower;
    for (const auto& vertex : soup.vertices) {
        const float* p = vertex.data();
        for (std::size_t i = 0; i < 3; ++i) {
            lower[i] = std::min(lower[i], p[i]);
            upper[i] = std::max(upper[i], p[i]);
        }
    }
    const Vec3f diagonal(upper[0] - lower[0], upper[1] - lower[1],
                         upper[2] - lower[2]);
    return static_cast<double>(diagonal.norm())
           * params.weld_relative_tolerance;
}

/// @brief WeldKeyから統合後の頂点番号を取得するオープンアドレス法のハッシュ表
/// @note 枠の数は2のべき乗. 衝突時は次の枠を順に調べる（線形探索）
struct WeldTable {
    /// @brief 空きスロットを示すindicesの値
    static constexpr std::uint32_t kEmptySlot = 0xffffffffU;

    /// @brief 各スロットのWeldKey
    std::vector<WeldKey> keys;
    /// @brief 各枠の頂点番号（空のスロットはkEmptySlot）
    std::vector<std::uint32_t> indices;

    /// @brief WeldKeyの格納先を探す（同じキーのスロット、なければ空きスロット）
    /// @param key WeldKey
    /// @return スロットの番号
    std::size_t FindSlot(const WeldKey& key) const {
        // WeldKeyHashの値は下位ビットの偏りが大きいため、上位ビットに混ぜてから使う
        constexpr std::uint64_t kFibonacci = 0x9e3779b97f4a7c15ULL;
        const std::size_t mask = indices.size() - 1;
        std::size_t slot = static_cast<std::size_t>(
                (static_cast<std::uint64_t>(WeldKeyHash{}(key)) * kFibonacci)
                >> 20) & mask;
        while (indices[slot] != kEmptySlot && keys[slot] != key) {
            slot = (slot + 1) & mask;
        }
        return slot;
    }

    /// @brief リサイズ
    /// @param capacity スロットの数（2のべき乗）
    void Resize(const std::size_t capacity) {
        keys.resize(capacity);
        indices.assign(capacity, kEmptySlot);
    }

    /// @brief スロット数を2倍にする
    /// @note 既存のキーとインデックスは保持する
    void Grow() {
        WeldTable grown;
        grown.Resize(keys.size() * 2);
        for (std::size_t s = 0; s < indices.size(); ++s) {
            if (indices[s] == kEmptySlot) continue;
            const auto slot = grown.FindSlot(keys[s]);
            grown.keys[slot] = keys[s];
            grown.indices[slot] = indices[s];
        }
        *this = std::move(grown);
    }
};

/// @brief ポリゴンスープの頂点を統合してインデックスメッシュへ変換する
/// @param soup ポリゴンスープ
/// @param tolerance 頂点統合の許容距離
/// @return 共有頂点化したメッシュ（頂点法線は空）
/// @note 頂点番号は各頂点がスープに初めて現れた順に付ける
i_num::TriangleMeshf WeldSoup(const StlSoup& soup, const double tolerance) {
    // 閉じたメッシュでは統合後の頂点数がスープの約1/6になるため、
    // スープの頂点数の半分以上を初期の枠の数とし、使用率が1/2を超えたら広げる
    std::size_t capacity = 16;
    while (capacity < soup.vertices.size() / 2) capacity *= 2;
    WeldTable table;
    table.Resize(capacity);

    i_num::TriangleMeshf mesh;
    std::vector<Vec3f> unique_vertices;
    unique_vertices.reserve(soup.vertices.size() / 5);
    mesh.indices.reserve(soup.vertices.size());
    for (const auto& vertex : soup.vertices) {
        const auto key = MakeWeldKey(vertex, tolerance);
        auto slot = table.FindSlot(key);
        if (table.indices[slot] == WeldTable::kEmptySlot) {
            if ((unique_vertices.size() + 1) * 2 > table.indices.size()) {
                table.Grow();
                slot = table.FindSlot(key);
            }
            table.keys[slot] = key;
            table.indices[slot] =
                    static_cast<std::uint32_t>(unique_vertices.size());
            unique_vertices.push_back(vertex);
        }
        mesh.indices.push_back(table.indices[slot]);
    }

    mesh.positions.resize(3, static_cast<Eigen::Index>(unique_vertices.size()));
    for (std::size_t c = 0; c < unique_vertices.size(); ++c) {
        mesh.positions.col(static_cast<Eigen::Index>(c)) = unique_vertices[c];
    }
    return mesh;
}

/// @brief ポリゴンスープの頂点を統合せずにインデックスメッシュへ変換する
/// @param soup ポリゴンスープ
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
    return params.weld_vertices
            ? WeldSoup(soup, ResolveWeldTolerance(soup, params))
            : ExpandSoup(soup);
}

std::shared_ptr<igesio::entities::MeshEntity>
i_ext::ReadStlAsEntity(const std::string& path, const StlReadParams& params) {
    // CPU側の値はdoubleで保持するため倍精度に変換
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

    std::ofstream file(igesio::utils::PathFromUtf8(path),
                       binary ? (std::ios::binary | std::ios::trunc)
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
