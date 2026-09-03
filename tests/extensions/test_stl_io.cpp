/**
 * @file extensions/test_stl_io.cpp
 * @brief extensions/stl/stl_io.h (STL入出力) のテスト
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `WriteStl` / `ReadStl` の往復 (バイナリ・ASCII)
 *       - バイナリ/ASCIIの自動判別
 *       - 溶接 (weld_vertices / weld_tolerance / weld_relative_tolerance) の挙動
 *       - `ReadStlAsEntity`
 *       - 異常系 (ファイルなし・不正ファイル・不正メッシュ)
 * @note TODO: ビッグエンディアン環境は対象外 (実装がリトルエンディアン前提).
 *       他ツールが出力したSTLファイルとの相互運用は手動確認とする.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "igesio/common/errors.h"
#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/numerics/meshes/algorithms.h"
#include "igesio/extensions/stl/stl_io.h"

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

// 不正なメッシュ (範囲外インデックス) の書き出しはstd::invalid_argument
TEST(StlIOTest, WriteThrowsInvalidArgumentWhenMeshInvalid) {
    auto mesh = MakeUnitQuad();
    mesh.indices[0] = 100;
    EXPECT_THROW(
        i_ext::WriteStl(mesh, OutputPath("stl_invalid.stl")),
        std::invalid_argument);
}
