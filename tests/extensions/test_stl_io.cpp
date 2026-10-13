/**
 * @file extensions/test_stl_io.cpp
 * @brief extensions/stl/stl_io.h (STL入出力) のテスト
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `WriteStl` / `ReadStl` の往復 (バイナリ・ASCII)
 *       - バイナリ/ASCIIの自動判別
 *       - ASCII形式の数値の書式 (`std::istream`の`>>`と同じ成否と値) と区切り
 *       - 溶接 (weld_vertices / weld_tolerance / weld_relative_tolerance) の挙動
 *         (溶接用ハッシュ表が拡張される規模での、素直な実装との一致を含む)
 *       - `ReadStlAsEntity`
 *       - 異常系 (ファイルなし・不正ファイル・不正メッシュ)
 *       - 全角文字を含むパスでの往復と例外 (親ディレクトリの自動作成を含む)
 * @note TODO: ビッグエンディアン環境は対象外 (実装がリトルエンディアン前提).
 *       他ツールが出力したSTLファイルとの相互運用は手動確認とする.
 */
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/numerics/meshes/algorithms.h"
#include "igesio/extensions/stl/stl_io.h"
#include "igesio/utils/path_encoding.h"
#include "unicode_test_path.h"

namespace {

namespace fs = std::filesystem;
namespace i_ext = igesio::extensions;
namespace i_num = igesio::numerics;

/// @brief テストによる出力フォルダ
const std::string kOutputDirPath =
        fs::path(__FILE__).parent_path().parent_path()
            .append("test_data").append("output").string();

/// @brief 浮動小数点比較の許容誤差 (float経由の往復のため単精度相当)
constexpr float kTol = 1e-6f;

/// @brief 出力フォルダ内のファイルパスを生成する
std::string OutputPath(const std::string& filename) {
    return (fs::path(kOutputDirPath) / filename).string();
}

/// @brief z=0平面上の単位正方形メッシュ (4頂点・2三角形; 共有頂点あり) を生成する
i_num::TriangleMeshf MakeUnitQuad() {
    i_num::TriangleMeshf mesh;
    mesh.positions.resize(3, 4);
    mesh.positions << 0.0f, 1.0f, 1.0f, 0.0f,
                      0.0f, 0.0f, 1.0f, 1.0f,
                      0.0f, 0.0f, 0.0f, 0.0f;
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

/// @brief 読み戻したメッシュが単位正方形と一致するか検証する
/// @param mesh 検証対象 (溶接読み込みの結果)
void ExpectUnitQuad(const i_num::TriangleMeshf& mesh) {
    EXPECT_EQ(mesh.VertexCount(), 4u);
    EXPECT_EQ(mesh.TriangleCount(), 2u);
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);
    EXPECT_FALSE(mesh.HasNormals());  // 溶接読み込みは法線を持たない

    // 成分min/maxが単位正方形と一致する
    EXPECT_NEAR(mesh.positions.row(0).minCoeff(), 0.0f, kTol);
    EXPECT_NEAR(mesh.positions.row(0).maxCoeff(), 1.0f, kTol);
    EXPECT_NEAR(mesh.positions.row(1).minCoeff(), 0.0f, kTol);
    EXPECT_NEAR(mesh.positions.row(1).maxCoeff(), 1.0f, kTol);
    EXPECT_NEAR(mesh.positions.row(2).minCoeff(), 0.0f, kTol);
    EXPECT_NEAR(mesh.positions.row(2).maxCoeff(), 0.0f, kTol);
}

/// @brief 対角頂点の一方にfloatの丸め誤差程度のずれを持たせた2三角形の
///        スープを生成する (一辺300の正方形; 完全一致では5頂点になる)
/// @return 生成したポリゴンスープ
/// @note 他ツールが出力したSTLで、幾何的には同一だが末尾ビットが異なる頂点が
///       現れる状況を模した入力. バウンディングボックスの対角長は約424のため、
///       既定の相対許容距離 (1e-6) では約4.2e-4となりずれが吸収される
i_num::TriangleMeshf MakeQuadSoupWithRoundingNoise() {
    // 座標0付近に現れる丸め誤差 (実際のSTLで観測される桁)
    constexpr float kNoise = 2.744137e-14f;
    i_num::TriangleMeshf soup;
    soup.positions.resize(3, 6);
    soup.positions << 0.0f, 300.0f, 300.0f,   0.0f, 300.0f, 0.0f,
                      0.0f, 0.0f,   300.0f,   0.0f, 300.0f, 300.0f,
                      0.0f, 0.0f,   0.0f,     0.0f, kNoise, 0.0f;
    soup.indices = {0, 1, 2, 3, 4, 5};
    return soup;
}

/// @brief 文字列を改行コードの変換なしでファイルへ書き出す
void WriteBytes(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << content;
}

/// @brief 1つ目の頂点のx座標の語を差し替えた、三角形1枚のASCII STLを生成する
/// @param x_token 1つ目の頂点のx座標として書く語
/// @return 面法線 (0, 0, 1)、頂点 (x_token, 0, 0)/(1, 0, 0)/(0, 1, 0) のテキスト
std::string MakeAsciiTriangleWithX(const std::string& x_token) {
    return "solid t\n"
           "facet normal 0 0 1\n"
           "outer loop\n"
           "vertex " + x_token + " 0 0\n"
           "vertex 1 0 0\n"
           "vertex 0 1 0\n"
           "endloop\n"
           "endfacet\n"
           "endsolid t\n";
}

/// @brief 共有頂点の多い格子と、頂点を共有しない散在三角形を混ぜたスープを生成する
/// @return 格子 (40×40頂点; 共有あり) の三角形の後に、乱数座標の三角形
///         (3000枚; 座標は1/8刻みのため一部は偶然一致する) を並べたスープ
/// @note 溶接後の頂点数がスープの頂点数の1/4を大きく超えるため、溶接用の
///       ハッシュ表が読み込み中に拡張される
i_num::TriangleMeshf MakeMixedSoupForWelding() {
    constexpr int kGrid = 40;
    constexpr int kScattered = 3000;
    std::vector<std::array<float, 3>> corners;
    for (int y = 0; y + 1 < kGrid; ++y) {
        for (int x = 0; x + 1 < kGrid; ++x) {
            const auto p = [](int i, int j) {
                return std::array<float, 3>{float(i), float(j), 0.0f};
            };
            for (const auto& c : {p(x, y), p(x + 1, y), p(x + 1, y + 1),
                                  p(x, y), p(x + 1, y + 1), p(x, y + 1)}) {
                corners.push_back(c);
            }
        }
    }
    std::mt19937 engine(7);
    std::uniform_int_distribution<int> coordinate(-800, 800);
    for (int t = 0; t < 3 * kScattered; ++t) {
        corners.push_back({coordinate(engine) / 8.0f, coordinate(engine) / 8.0f,
                           coordinate(engine) / 8.0f + 50.0f});
    }

    i_num::TriangleMeshf soup;
    soup.positions.resize(3, static_cast<Eigen::Index>(corners.size()));
    for (std::size_t c = 0; c < corners.size(); ++c) {
        soup.positions.col(static_cast<Eigen::Index>(c))
                << corners[c][0], corners[c][1], corners[c][2];
        soup.indices.push_back(static_cast<std::uint32_t>(c));
    }
    return soup;
}

/// @brief ビット単位の完全一致で溶接した頂点番号を素直な方法で計算する
/// @param soup ポリゴンスープ (indicesが0,1,2,...の順に並ぶこと)
/// @return 各スープ頂点の溶接後の頂点番号 (初出順に番号を付ける)
std::vector<std::uint32_t> WeldBitExactByMap(const i_num::TriangleMeshf& soup) {
    std::map<std::array<std::uint32_t, 3>, std::uint32_t> index_of;
    std::vector<std::uint32_t> indices;
    for (Eigen::Index c = 0; c < soup.positions.cols(); ++c) {
        std::array<std::uint32_t, 3> bits;
        std::memcpy(bits.data(), soup.positions.col(c).data(), sizeof(bits));
        const auto next = static_cast<std::uint32_t>(index_of.size());
        indices.push_back(index_of.emplace(bits, next).first->second);
    }
    return indices;
}

}  // namespace



/**
 * 正常系: 往復
 */

// バイナリ形式の書き出し→読み戻しで形状が保持される
TEST(StlIOTest, BinaryRoundTrip) {
    const auto path = OutputPath("stl_roundtrip_binary.stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeUnitQuad(), path, /*binary=*/true));
    ASSERT_TRUE(fs::exists(path));

    ExpectUnitQuad(i_ext::ReadStl(path));
}

// ASCII形式の書き出し→読み戻しで形状が保持される ("solid"で始まる)
TEST(StlIOTest, AsciiRoundTrip) {
    const auto path = OutputPath("stl_roundtrip_ascii.stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeUnitQuad(), path, /*binary=*/false));

    std::ifstream file(path);
    std::string first_word;
    file >> first_word;
    EXPECT_EQ(first_word, "solid");
    file.close();

    ExpectUnitQuad(i_ext::ReadStl(path));
}



/**
 * 正常系: 溶接
 */

// weld_vertices=falseでは3頂点/三角形のまま、面法線が頂点へ展開される
TEST(StlIOTest, NoWeldExpandsFacetNormals) {
    const auto path = OutputPath("stl_no_weld.stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeUnitQuad(), path));

    i_ext::StlReadParams params;
    params.weld_vertices = false;
    const auto mesh = i_ext::ReadStl(path, params);

    EXPECT_EQ(mesh.VertexCount(), 6u);  // 2三角形 × 3頂点 (共有なし)
    EXPECT_EQ(mesh.TriangleCount(), 2u);
    ASSERT_TRUE(mesh.HasNormals());
    // z=0平面のCCW三角形のため面法線は+z (全頂点へ展開済み)
    for (int c = 0; c < 6; ++c) {
        EXPECT_NEAR(mesh.normals(0, c), 0.0f, kTol);
        EXPECT_NEAR(mesh.normals(1, c), 0.0f, kTol);
        EXPECT_NEAR(mesh.normals(2, c), 1.0f, kTol);
    }
}

// weld_toleranceで近接頂点が溶接される (境界: 0では別頂点のまま)
TEST(StlIOTest, WeldToleranceMergesNearbyVertices) {
    // 共有すべき対角頂点を1e-6だけずらした2三角形のスープを作る
    i_num::TriangleMeshf soup;
    soup.positions.resize(3, 6);
    soup.positions << 0.0f, 1.0f, 1.0f, 0.0f,      1.0f + 1e-6f, 0.0f,
                      0.0f, 0.0f, 1.0f, 0.0f,      1.0f + 1e-6f, 1.0f,
                      0.0f, 0.0f, 0.0f, 0.0f,      0.0f,         0.0f;
    soup.indices = {0, 1, 2, 3, 4, 5};
    const auto path = OutputPath("stl_weld_tolerance.stl");
    ASSERT_TRUE(i_ext::WriteStl(soup, path));

    // 完全一致のみ (絶対・相対とも0): ずれた頂点は溶接されない
    i_ext::StlReadParams exact;
    exact.weld_tolerance = 0.0;
    exact.weld_relative_tolerance = 0.0;
    EXPECT_EQ(i_ext::ReadStl(path, exact).VertexCount(), 5u);  // (0,0,0)のみ共有

    // tolerance=1e-3: ずれた頂点も溶接される
    i_ext::StlReadParams tolerant;
    tolerant.weld_tolerance = 1e-3;
    EXPECT_EQ(i_ext::ReadStl(path, tolerant).VertexCount(), 4u);
}

// 既定の相対許容距離は、幾何的に同一だが末尾ビットが異なる頂点を溶接する
TEST(StlIOTest, RelativeToleranceMergesRoundingNoise) {
    const auto path = OutputPath("stl_weld_relative_tolerance.stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeQuadSoupWithRoundingNoise(), path));

    // 既定 (相対1e-6; バウンディングボックスの対角長約424に対し約4.2e-4) では溶接される
    EXPECT_EQ(i_ext::ReadStl(path).VertexCount(), 4u);
}

// 相対許容距離を0にすると、従来どおりビット単位の完全一致のみを溶接する
TEST(StlIOTest, ZeroRelativeToleranceKeepsBitExactWelding) {
    const auto path = OutputPath("stl_weld_relative_zero.stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeQuadSoupWithRoundingNoise(), path));

    i_ext::StlReadParams exact;
    exact.weld_relative_tolerance = 0.0;
    EXPECT_EQ(i_ext::ReadStl(path, exact).VertexCount(), 5u);
}

// 絶対許容距離 (weld_tolerance) の指定は相対許容距離より優先される
TEST(StlIOTest, AbsoluteToleranceTakesPrecedenceOverRelative) {
    const auto path = OutputPath("stl_weld_absolute_precedence.stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeQuadSoupWithRoundingNoise(), path));

    // 相対を0 (完全一致) にしても、絶対値の指定が使われて全頂点が溶接される
    i_ext::StlReadParams coarse;
    coarse.weld_tolerance = 1000.0;  // 一辺300より粗い量子化
    coarse.weld_relative_tolerance = 0.0;
    EXPECT_EQ(i_ext::ReadStl(path, coarse).VertexCount(), 1u);
}

// 溶接用ハッシュ表が拡張される規模でも、初出順の頂点番号が素直な実装と一致する
TEST(StlIOTest, WeldMatchesMapBasedWeldingWhenTableGrows) {
    const auto soup = MakeMixedSoupForWelding();
    const auto path = OutputPath("stl_weld_table_growth.stl");
    ASSERT_TRUE(i_ext::WriteStl(soup, path));

    i_ext::StlReadParams exact;
    exact.weld_relative_tolerance = 0.0;
    const auto mesh = i_ext::ReadStl(path, exact);

    const auto expected = WeldBitExactByMap(soup);
    ASSERT_EQ(mesh.indices.size(), expected.size());
    EXPECT_EQ(mesh.indices, expected);
    for (std::size_t s = 0; s < expected.size(); ++s) {
        const auto source = static_cast<Eigen::Index>(s);
        const auto welded = static_cast<Eigen::Index>(mesh.indices[s]);
        EXPECT_NEAR((mesh.positions.col(welded) - soup.positions.col(source))
                            .cwiseAbs().maxCoeff(),
                    0.0f, kTol);
    }
}



/**
 * 正常系: ASCII形式の書式
 */

// istreamと同じ書式 (先頭の+、小数点で始まる/終わる値、大文字の指数) と、
// CRLF/タブの区切り、空白を含むsolid名を受け付ける
TEST(StlIOTest, AsciiAcceptsIstreamCompatibleFormats) {
    const auto path = OutputPath("stl_ascii_formats.stl");
    WriteBytes(path,
        "solid my part\r\n"
        "facet normal 0 0 +1\r\n"
        "\touter loop\r\n"
        "\t\tvertex 0 0 0\r\n"
        "\t\tvertex 1E0\t0 0\r\n"
        "\t\tvertex .0 1. 0\r\n"
        "\tendloop\r\n"
        "endfacet\r\n"
        "endsolid my part\r\n");

    i_ext::StlReadParams params;
    params.weld_vertices = false;
    const auto mesh = i_ext::ReadStl(path, params);
    ASSERT_EQ(mesh.VertexCount(), 3u);
    EXPECT_NEAR(mesh.positions(0, 1), 1.0f, kTol);
    EXPECT_NEAR(mesh.positions(1, 2), 1.0f, kTol);
    EXPECT_NEAR(mesh.normals(2, 0), 1.0f, kTol);
}

// 単精度の最大値 (オーバーフローの境界の内側) は読み取れる
TEST(StlIOTest, AsciiAcceptsFloatMaximum) {
    const auto path = OutputPath("stl_ascii_float_max.stl");
    WriteBytes(path, MakeAsciiTriangleWithX("3.4028234e38"));

    i_ext::StlReadParams params;
    params.weld_vertices = false;
    const auto mesh = i_ext::ReadStl(path, params);
    ASSERT_EQ(mesh.VertexCount(), 3u);
    EXPECT_NEAR(mesh.positions(0, 0) / std::numeric_limits<float>::max(), 1.0f,
                kTol);
}



/**
 * 正常系: エンティティ直結
 */

// ReadStlAsEntityがMeshEntity (kNonIges) を返す
TEST(StlIOTest, ReadStlAsEntityBuildsMeshEntity) {
    const auto path = OutputPath("stl_as_entity.stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeUnitQuad(), path));

    const auto entity = i_ext::ReadStlAsEntity(path);
    ASSERT_NE(entity, nullptr);
    EXPECT_EQ(entity->GetType(), igesio::entities::EntityType::kNonIges);
    EXPECT_EQ(entity->Mesh().TriangleCount(), 2u);
    EXPECT_TRUE(entity->GetDefinedBoundingBox().Contains({0.5, 0.5, 0.0}));
}



/**
 * 異常系
 */

// 存在しないファイルはFileOpenError
TEST(StlIOTest, ReadThrowsFileOpenErrorWhenFileMissing) {
    EXPECT_THROW(i_ext::ReadStl(OutputPath("stl_not_exist.stl")),
                 igesio::FileOpenError);
}

// STLでないテキストファイルはParseError
TEST(StlIOTest, ReadThrowsParseErrorWhenNotStl) {
    const auto path = OutputPath("stl_garbage.txt");
    {
        std::ofstream file(path, std::ios::trunc);
        file << "This is not an STL file.\n";
    }
    EXPECT_THROW(i_ext::ReadStl(path), igesio::ParseError);
}

/// @brief istreamが受け付けない数値の書式の読み取りテスト (ASCII STL)
class StlInvalidNumberTest : public ::testing::TestWithParam<std::string> {};

// 受け付けない書式はParseError
TEST_P(StlInvalidNumberTest, ReadThrowsParseErrorWhenNumberIsInvalid) {
    const auto path = OutputPath("stl_ascii_invalid_number.stl");
    WriteBytes(path, MakeAsciiTriangleWithX(GetParam()));
    EXPECT_THROW(i_ext::ReadStl(path), igesio::ParseError);
}

INSTANTIATE_TEST_SUITE_P(
        IstreamIncompatible, StlInvalidNumberTest,
        ::testing::Values(
                "inf", "nan",  // 無限大/非数の綴り
                "3.5e38",      // 単精度のオーバーフロー (境界の外側)
                "1e",          // 不完全な指数
                "1,5"));       // 小数点がカンマ (後続の",5"が不正)

// 不正なメッシュ (範囲外インデックス) の書き出しはstd::invalid_argument
TEST(StlIOTest, WriteThrowsInvalidArgumentWhenMeshInvalid) {
    auto mesh = MakeUnitQuad();
    mesh.indices[0] = 100;
    EXPECT_THROW(
        i_ext::WriteStl(mesh, OutputPath("stl_invalid.stl")),
        std::invalid_argument);
}



/**
 * 全角パス
 */

// 全角名のディレクトリ (未作成の親ディレクトリを含む) で、
// バイナリ形式の書き出し→読み戻しができる
TEST(StlIOTest, UnicodePath_BinaryRoundTrip) {
    using igesio::tests::kUnicodeName;
    const igesio::tests::UnicodeTempDir dir("stl_io_binary");
    const std::string path =
            dir.JoinUtf8(kUnicodeName + "/" + kUnicodeName + ".stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeUnitQuad(), path, /*binary=*/true));
    EXPECT_TRUE(fs::is_regular_file(igesio::utils::PathFromUtf8(path)));

    ExpectUnitQuad(i_ext::ReadStl(path));
    EXPECT_NE(i_ext::ReadStlAsEntity(path), nullptr);
}

// 全角名のディレクトリ (未作成の親ディレクトリを含む) で、
// ASCII形式の書き出し→読み戻しができる
TEST(StlIOTest, UnicodePath_AsciiRoundTrip) {
    using igesio::tests::kUnicodeName;
    const igesio::tests::UnicodeTempDir dir("stl_io_ascii");
    const std::string path =
            dir.JoinUtf8(kUnicodeName + "/" + kUnicodeName + ".stl");
    ASSERT_TRUE(i_ext::WriteStl(MakeUnitQuad(), path, /*binary=*/false));
    EXPECT_TRUE(fs::is_regular_file(igesio::utils::PathFromUtf8(path)));

    ExpectUnitQuad(i_ext::ReadStl(path));
}

// 存在しない全角パスはFileOpenErrorで、文言にUTF-8のパスを含む
TEST(StlIOTest, UnicodePath_ReadThrowsFileOpenErrorWhenFileMissing) {
    const igesio::tests::UnicodeTempDir dir("stl_io_missing");
    const std::string path =
            dir.JoinUtf8(igesio::tests::kUnicodeName + ".stl");
    try {
        i_ext::ReadStl(path);
        FAIL() << "FileOpenError was not thrown";
    } catch (const igesio::FileOpenError& e) {
        EXPECT_NE(std::string(e.what()).find(path), std::string::npos);
    }
}
