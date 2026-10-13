/**
 * @file extensions/test_obj_io.cpp
 * @brief extensions/obj/obj_io.h (OBJ入出力) のテスト
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `WriteObj` / `ReadObj` の往復 (位置のみ / 法線・UV付き / グループ付き)
 *       - 多角形フェイスのファン三角形分割
 *       - v/vt/vn の多重インデックスのコーナー重複排除
 *       - 負 (末尾相対) インデックスの解決
 *       - 未対応キーワード (mtllib / s / コメント) の読み飛ばし
 *       - 数値の書式 (`std::istream`の`>>`と同じ成否と値) と区切り (CRLF/タブ)
 *       - 読み込みブロック (1 MiB) をまたぐ行・ブロックより長い行・末尾の改行なし
 *       - g / o / usemtl による面グループの切り替え
 *       - 途中から参照されるUVチャンネル (それ以前の頂点は0)
 *       - `ReadObjAsEntity`
 *       - 異常系 (ファイルなし・範囲外/不足インデックス・不正メッシュ・不正な数値)
 *       - 全角文字を含むパスでの往復と例外 (親ディレクトリの自動作成を含む)
 * @note TODO: マテリアルライブラリ (.mtl) は対象外 (usemtlの参照名のみ保持).
 *       マテリアルが設定された後に未設定へ戻すグループ境界は、標準OBJでは
 *       表現できないため往復対象外 (usemtlは次の指定まで持続する).
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "igesio/common/errors.h"
#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/numerics/meshes/algorithms.h"
#include "igesio/extensions/obj/obj_io.h"
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

/// @brief 浮動小数点比較の許容誤差 (倍精度の往復)
constexpr double kTol = 1e-9;

/// @brief 出力フォルダ内のファイルパスを生成する
std::string OutputPath(const std::string& filename) {
    return (fs::path(kOutputDirPath) / filename).string();
}

/// @brief 文字列をファイルへ書き出す (生のOBJテキストを用意する用)
void WriteText(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::trunc);
    file << content;
}

/// @brief 文字列を改行コードの変換なしでファイルへ書き出す
void WriteBytes(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << content;
}

/// @brief 1つ目の頂点のx座標の語を差し替えた、三角形1枚のOBJテキストを生成する
/// @param x_token 1つ目の頂点のx座標として書く語
/// @return "v <x_token> 0 0" に続けて2頂点と面1枚を並べたテキスト
std::string MakeTriangleObjWithX(const std::string& x_token) {
    return "v " + x_token + " 0 0\n"
           "v 1 0 0\n"
           "v 0 1 0\n"
           "f 1 2 3\n";
}

/// @brief 数値の語と、読み取りで期待する値の組
struct NumberCase {
    /// @brief ファイルに書く語
    std::string token;
    /// @brief 期待する値
    double expected;
};

/// @brief 読み込みブロック (1 MiB) をまたぐ行を含むOBJテキストを生成する
/// @param vertex_count 頂点行の数 (i番目の頂点は (i, i/4, -i))
/// @return 1.5 MiBのコメント行の後に頂点行と、全頂点を使う扇形の面行を並べ、
///         末尾に改行を置かないテキスト (約3.5 MiB)
std::string MakeObjSpanningReadBlocks(const int vertex_count) {
    std::string text = "#" + std::string(1536 * 1024, 'x') + "\n";
    for (int i = 0; i < vertex_count; ++i) {
        text += "v " + std::to_string(i) + " " + std::to_string(i / 4.0) +
                " " + std::to_string(-i) + "\n";
    }
    text += "f";
    for (int i = 1; i <= vertex_count; ++i) text += " " + std::to_string(i);
    return text;
}

/// @brief z=0平面上の単位正方形メッシュ (4頂点・2三角形; 共有頂点あり) を生成する
i_num::TriangleMeshd MakeUnitQuad() {
    i_num::TriangleMeshd mesh;
    mesh.positions.resize(3, 4);
    mesh.positions << 0.0, 1.0, 1.0, 0.0,
                      0.0, 0.0, 1.0, 1.0,
                      0.0, 0.0, 0.0, 0.0;
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

/// @brief 法線 (+z) とUVを備えた単位正方形メッシュを生成する
i_num::TriangleMeshd MakeUnitQuadWithChannels() {
    auto mesh = MakeUnitQuad();
    mesh.normals.resize(3, 4);
    mesh.normals << 0.0, 0.0, 0.0, 0.0,
                    0.0, 0.0, 0.0, 0.0,
                    1.0, 1.0, 1.0, 1.0;
    mesh.uvs.resize(2, 4);
    mesh.uvs << 0.0, 1.0, 1.0, 0.0,
                0.0, 0.0, 1.0, 1.0;
    return mesh;
}

/// @brief 三角形ごとに解決した頂点属性が一致するか検証する
/// @note 頂点列の順序は往復で変わりうるため、(三角形, コーナー) 単位で比較する
void ExpectSameGeometry(const i_num::TriangleMeshd& a,
                        const i_num::TriangleMeshd& b) {
    ASSERT_EQ(a.TriangleCount(), b.TriangleCount());
    ASSERT_EQ(a.HasNormals(), b.HasNormals());
    ASSERT_EQ(a.HasUVs(), b.HasUVs());
    for (std::size_t t = 0; t < a.TriangleCount(); ++t) {
        for (int i = 0; i < 3; ++i) {
            const auto ia = a.indices[t * 3 + i];
            const auto ib = b.indices[t * 3 + i];
            for (int r = 0; r < 3; ++r) {
                EXPECT_NEAR(a.positions(r, ia), b.positions(r, ib), kTol);
            }
            if (a.HasNormals()) {
                for (int r = 0; r < 3; ++r) {
                    EXPECT_NEAR(a.normals(r, ia), b.normals(r, ib), kTol);
                }
            }
            if (a.HasUVs()) {
                for (int r = 0; r < 2; ++r) {
                    EXPECT_NEAR(a.uvs(r, ia), b.uvs(r, ib), kTol);
                }
            }
        }
    }
}

}  // namespace



/**
 * 正常系: 往復
 */

// 位置のみのメッシュが往復で保持される
TEST(ObjIOTest, RoundTrip_PositionsOnly) {
    const auto path = OutputPath("obj_positions_only.obj");
    ASSERT_TRUE(i_ext::WriteObj(MakeUnitQuad(), path));
    ASSERT_TRUE(fs::exists(path));

    const auto read = i_ext::ReadObj(path);
    EXPECT_EQ(read.VertexCount(), 4u);
    EXPECT_EQ(read.TriangleCount(), 2u);
    EXPECT_FALSE(read.HasNormals());
    EXPECT_FALSE(read.HasUVs());
    ExpectSameGeometry(MakeUnitQuad(), read);
}

// 法線・UV付きのメッシュが往復で保持される
TEST(ObjIOTest, RoundTrip_WithNormalsAndUVs) {
    const auto path = OutputPath("obj_with_channels.obj");
    const auto mesh = MakeUnitQuadWithChannels();
    ASSERT_TRUE(i_ext::WriteObj(mesh, path));

    const auto read = i_ext::ReadObj(path);
    EXPECT_TRUE(read.HasNormals());
    EXPECT_TRUE(read.HasUVs());
    ExpectSameGeometry(mesh, read);
}

// 面グループ (名前+マテリアル参照) が往復で保持される
TEST(ObjIOTest, RoundTrip_WithGroups) {
    auto mesh = MakeUnitQuad();
    // 2三角形をそれぞれ別グループ・別マテリアルへ割り当てる
    mesh.groups = {
        i_num::MeshGroup{"front", "red", 0, 1},
        i_num::MeshGroup{"back", "blue", 1, 1},
    };
    const auto path = OutputPath("obj_with_groups.obj");
    ASSERT_TRUE(i_ext::WriteObj(mesh, path));

    const auto read = i_ext::ReadObj(path);
    ASSERT_EQ(read.groups.size(), 2u);
    EXPECT_EQ(read.groups[0].name, "front");
    EXPECT_EQ(read.groups[0].material_name, "red");
    EXPECT_EQ(read.groups[0].first_triangle, 0u);
    EXPECT_EQ(read.groups[0].triangle_count, 1u);
    EXPECT_EQ(read.groups[1].name, "back");
    EXPECT_EQ(read.groups[1].material_name, "blue");
    EXPECT_EQ(read.groups[1].first_triangle, 1u);
    EXPECT_EQ(read.groups[1].triangle_count, 1u);
}



/**
 * 正常系: パース仕様
 */

// 多角形フェイスは先頭頂点を共有する三角形に分割される (四角形1枚 → 2三角形)
TEST(ObjIOTest, Read_PolygonFanTriangulates) {
    const auto path = OutputPath("obj_polygon.obj");
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "f 1 2 3 4\n");

    const auto mesh = i_ext::ReadObj(path);
    EXPECT_EQ(mesh.VertexCount(), 4u);
    EXPECT_EQ(mesh.TriangleCount(), 2u);
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);
}

// UVが異なるコーナーは別頂点に分かれる (位置共有・UV相違)
TEST(ObjIOTest, Read_MultiIndexSplitsVerticesByCorner) {
    const auto path = OutputPath("obj_multi_index.obj");
    // v=1 は (vt=1) と (vt=2) の2通りで参照され、別頂点に分かれる.
    // v=3 は両三角形で (vt=1) のため共有される.
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "vt 0 0\n"
        "vt 1 1\n"
        "f 1/1 2/1 3/1\n"
        "f 1/2 3/1 4/1\n");

    const auto mesh = i_ext::ReadObj(path);
    // コーナー: (1,1)(2,1)(3,1)(1,2)(4,1) の5通り
    EXPECT_EQ(mesh.VertexCount(), 5u);
    EXPECT_EQ(mesh.TriangleCount(), 2u);
    EXPECT_TRUE(mesh.HasUVs());
    EXPECT_FALSE(mesh.HasNormals());
}

// 負 (末尾相対) インデックスが解決される
TEST(ObjIOTest, Read_NegativeIndices) {
    const auto path = OutputPath("obj_negative.obj");
    // -3/-2/-1 は直前に定義した3頂点を指す
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f -3 -2 -1\n");

    const auto mesh = i_ext::ReadObj(path);
    EXPECT_EQ(mesh.VertexCount(), 3u);
    EXPECT_EQ(mesh.TriangleCount(), 1u);
    // 三角形の頂点が3点を正しく指している
    EXPECT_NEAR(mesh.positions(0, mesh.indices[1]), 1.0, kTol);  // 2番目=(1,0,0)
    EXPECT_NEAR(mesh.positions(1, mesh.indices[2]), 1.0, kTol);  // 3番目=(0,1,0)
}

// 未対応キーワード (mtllib / s / コメント) は読み飛ばす
TEST(ObjIOTest, Read_SkipsUnknownKeywords) {
    const auto path = OutputPath("obj_unknown_keywords.obj");
    WriteText(path,
        "# comment line\n"
        "mtllib materials.mtl\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "s 1\n"
        "f 1 2 3\n");

    const auto mesh = i_ext::ReadObj(path);
    EXPECT_EQ(mesh.VertexCount(), 3u);
    EXPECT_EQ(mesh.TriangleCount(), 1u);
}

/// @brief istreamが受け付ける数値の書式の読み取りテスト
class ObjNumberFormatTest : public ::testing::TestWithParam<NumberCase> {};

// 受け付ける書式は、istreamの`>>`と同じ値で読み取られる
TEST_P(ObjNumberFormatTest, Read_AcceptedNumberFormat) {
    const auto path = OutputPath("obj_number_format.obj");
    WriteText(path, MakeTriangleObjWithX(GetParam().token));

    const auto mesh = i_ext::ReadObj(path);
    ASSERT_EQ(mesh.VertexCount(), 3u);
    EXPECT_NEAR(mesh.positions(0, mesh.indices[0]), GetParam().expected,
                kTol * std::max(1.0, std::abs(GetParam().expected)));
}

INSTANTIATE_TEST_SUITE_P(
        IstreamCompatible, ObjNumberFormatTest,
        ::testing::Values(
                NumberCase{"+1.5", 1.5},       // 先頭の+
                NumberCase{".5", 0.5},         // 小数点で始まる
                NumberCase{"5.", 5.0},         // 小数点で終わる
                NumberCase{"1E+05", 1e5},      // 大文字の指数と符号
                NumberCase{"00012.50", 12.5},  // 先頭の0
                NumberCase{"-0", 0.0},         // 負のゼロ
                NumberCase{"1e-400", 0.0},     // アンダーフローは0
                // 倍精度の最大値 (オーバーフローの境界の内側)
                NumberCase{"1.7976931348623157e308", 1.7976931348623157e308}));

/// @brief istreamが受け付けない数値の書式の読み取りテスト
class ObjInvalidNumberTest : public ::testing::TestWithParam<std::string> {};

// 受け付けない書式はParseError
TEST_P(ObjInvalidNumberTest, Read_ThrowsParseErrorWhenNumberIsInvalid) {
    const auto path = OutputPath("obj_invalid_number.obj");
    WriteText(path, MakeTriangleObjWithX(GetParam()));
    EXPECT_THROW(i_ext::ReadObj(path), igesio::ParseError);
}

INSTANTIATE_TEST_SUITE_P(
        IstreamIncompatible, ObjInvalidNumberTest,
        ::testing::Values(
                "inf", "nan", "-INF",  // 無限大/非数の綴り
                "1.8e308",             // オーバーフロー (境界の外側)
                "1e",                  // 不完全な指数
                "+-1",                 // 符号の重複
                "1,5",                 // 小数点がカンマ (後続の",5"が不正)
                "1.5abc",              // 数値の直後の文字 (後続の"abc"が不正)
                "abc"));

// 3つ目の座標の直後の文字は、istreamと同様に読み残して無視する
TEST(ObjIOTest, Read_IgnoresCharactersAfterThirdCoordinate) {
    const auto path = OutputPath("obj_trailing_chars.obj");
    WriteText(path,
        "v 0 0 0abc\n"
        "v 1 0 0 1.0 0.5 0.25\n"  // 頂点カラー等の余剰成分
        "v 0 1 0\n"
        "f 1 2 3\n");
    EXPECT_EQ(i_ext::ReadObj(path).VertexCount(), 3u);
}

// vtのv成分は省略時、および読み取れない場合も0になる (エラーにしない)
TEST(ObjIOTest, Read_TexcoordSecondComponentDefaultsToZero) {
    const auto path = OutputPath("obj_vt_optional_v.obj");
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vt 0.5\n"
        "vt 0.25 x\n"
        "f 1/1 2/2 3/1\n");

    const auto mesh = i_ext::ReadObj(path);
    ASSERT_TRUE(mesh.HasUVs());
    EXPECT_NEAR(mesh.uvs(0, mesh.indices[0]), 0.5, kTol);
    EXPECT_NEAR(mesh.uvs(1, mesh.indices[0]), 0.0, kTol);
    EXPECT_NEAR(mesh.uvs(0, mesh.indices[1]), 0.25, kTol);
    EXPECT_NEAR(mesh.uvs(1, mesh.indices[1]), 0.0, kTol);
}

// CRLFの改行とタブ区切りを受け付ける (グループ名の末尾のCRは除く)
TEST(ObjIOTest, Read_AcceptsCrlfAndTabs) {
    const auto path = OutputPath("obj_crlf_tabs.obj");
    WriteBytes(path,
        "g\tpart A\r\n"
        "v\t0 \t0\t0\r\n"
        "v 1\t\t0 0\r\n"
        "v 0 1 0\r\n"
        "f\t1/// 2 3\r\n");
    // "1///"は位置1 (UV/法線の要素は空)

    const auto mesh = i_ext::ReadObj(path);
    EXPECT_EQ(mesh.VertexCount(), 3u);
    EXPECT_EQ(mesh.TriangleCount(), 1u);
    ASSERT_EQ(mesh.groups.size(), 1u);
    EXPECT_EQ(mesh.groups[0].name, "part A");
}

// 読み込みブロックをまたぐ行、ブロックより長い行、末尾に改行のない行を読み取れる
TEST(ObjIOTest, Read_LinesAcrossReadBlocks) {
    constexpr int kVertexCount = 80000;
    const auto path = OutputPath("obj_read_blocks.obj");
    WriteBytes(path, MakeObjSpanningReadBlocks(kVertexCount));

    const auto mesh = i_ext::ReadObj(path);
    ASSERT_EQ(mesh.VertexCount(), std::size_t(kVertexCount));
    ASSERT_EQ(mesh.TriangleCount(), std::size_t(kVertexCount - 2));
    for (int i = 0; i < kVertexCount; i += 997) {
        // 面は頂点を定義順に参照するため、出力頂点の順は定義順と同じ
        const auto c = static_cast<Eigen::Index>(i);
        EXPECT_NEAR(mesh.positions(0, c), double(i), kTol);
        EXPECT_NEAR(mesh.positions(1, c), i / 4.0, kTol);
        EXPECT_NEAR(mesh.positions(2, c), -double(i), kTol);
    }
}

// グループは名前/マテリアルが変わった時だけ切り替わる (同じ文脈の再指定は継続)
TEST(ObjIOTest, Read_GroupsSplitOnlyWhenContextChanges) {
    const auto path = OutputPath("obj_group_context.obj");
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "g A\n"
        "f 1 2 3\n"
        "g A\n"           // 同じ名前の再指定: グループは継続
        "f 1 2 3\n"
        "usemtl m\n"      // マテリアルの変更: 新しいグループ
        "f 1 2 3\n"
        "g B\n"
        "g A\n"           // 面の無いまま元の文脈に戻る: 直前のグループを継続
        "f 1 2 3\n");

    const auto mesh = i_ext::ReadObj(path);
    ASSERT_EQ(mesh.groups.size(), 2u);
    EXPECT_EQ(mesh.groups[0].name, "A");
    EXPECT_EQ(mesh.groups[0].material_name, "");
    EXPECT_EQ(mesh.groups[0].first_triangle, 0u);
    EXPECT_EQ(mesh.groups[0].triangle_count, 2u);
    EXPECT_EQ(mesh.groups[1].name, "A");
    EXPECT_EQ(mesh.groups[1].material_name, "m");
    EXPECT_EQ(mesh.groups[1].first_triangle, 2u);
    EXPECT_EQ(mesh.groups[1].triangle_count, 2u);
}

// 途中の面から初めてUVが参照された場合、それ以前の頂点のUVは0になる
TEST(ObjIOTest, Read_LateUvReferenceFillsEarlierVerticesWithZero) {
    const auto path = OutputPath("obj_late_uv.obj");
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "v 1 1 0\n"
        "vt 0.5 0.75\n"
        "f 1 2 3\n"
        "f 2/1 4/1 3/1\n");

    const auto mesh = i_ext::ReadObj(path);
    // 1つ目の面の3頂点 + UV付きの3頂点 (位置2と3はUVの有無で別頂点)
    ASSERT_EQ(mesh.VertexCount(), 6u);
    ASSERT_TRUE(mesh.HasUVs());
    for (int i = 0; i < 3; ++i) {
        EXPECT_NEAR(mesh.uvs.col(mesh.indices[i]).norm(), 0.0, kTol);
        EXPECT_NEAR(mesh.uvs(0, mesh.indices[3 + i]), 0.5, kTol);
        EXPECT_NEAR(mesh.uvs(1, mesh.indices[3 + i]), 0.75, kTol);
    }
}



/**
 * 正常系: エンティティ直結
 */

// ReadObjAsEntityがMeshEntity (kNonIges) を返す
TEST(ObjIOTest, ReadObjAsEntity_BuildsMeshEntity) {
    const auto path = OutputPath("obj_as_entity.obj");
    ASSERT_TRUE(i_ext::WriteObj(MakeUnitQuad(), path));

    const auto entity = i_ext::ReadObjAsEntity(path);
    ASSERT_NE(entity, nullptr);
    EXPECT_EQ(entity->GetType(), igesio::entities::EntityType::kNonIges);
    EXPECT_EQ(entity->Mesh().TriangleCount(), 2u);
    EXPECT_TRUE(entity->GetDefinedBoundingBox().Contains({0.5, 0.5, 0.0}));
}



/**
 * 異常系
 */

// 存在しないファイルはFileOpenError
TEST(ObjIOTest, Read_ThrowsFileOpenErrorWhenFileMissing) {
    EXPECT_THROW(i_ext::ReadObj(OutputPath("obj_not_exist.obj")),
                 igesio::FileOpenError);
}

// 範囲外の頂点インデックスはParseError (境界: 範囲内は通る)
TEST(ObjIOTest, Read_ThrowsParseErrorWhenIndexOutOfRange) {
    const auto valid = OutputPath("obj_index_in_range.obj");
    WriteText(valid,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n");
    EXPECT_NO_THROW(i_ext::ReadObj(valid));  // 範囲内 (1..3) は通る

    const auto invalid = OutputPath("obj_index_out_of_range.obj");
    WriteText(invalid,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 4\n");  // 頂点は3個しかない
    EXPECT_THROW(i_ext::ReadObj(invalid), igesio::ParseError);
}

// 面インデックスは先頭の+を受け付ける (std::stollと同じ)
TEST(ObjIOTest, Read_FaceIndexWithLeadingPlus) {
    const auto path = OutputPath("obj_index_plus.obj");
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f +1 +2 3\n");
    EXPECT_EQ(i_ext::ReadObj(path).TriangleCount(), 1u);
}

/// @brief 整数として不正な面インデックスの読み取りテスト
class ObjInvalidFaceIndexTest : public ::testing::TestWithParam<std::string> {};

// 整数でない面インデックス、64bitの範囲外の面インデックスはParseError
TEST_P(ObjInvalidFaceIndexTest, Read_ThrowsParseErrorWhenFaceIndexIsInvalid) {
    const auto path = OutputPath("obj_index_not_integer.obj");
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 " + GetParam() + "\n");
    EXPECT_THROW(i_ext::ReadObj(path), igesio::ParseError);
}

INSTANTIATE_TEST_SUITE_P(
        NotInteger, ObjInvalidFaceIndexTest,
        ::testing::Values(
                "1x",                     // 数値の後の文字
                "1.0",                    // 小数
                "+-1",                    // 符号の重複
                "x",                      // 数値でない
                "99999999999999999999"));  // 64bitの範囲外

// 3頂点未満の面はParseError
TEST(ObjIOTest, Read_ThrowsParseErrorWhenFaceHasTwoVertices) {
    const auto path = OutputPath("obj_degenerate_face.obj");
    WriteText(path,
        "v 0 0 0\n"
        "v 1 0 0\n"
        "f 1 2\n");
    EXPECT_THROW(i_ext::ReadObj(path), igesio::ParseError);
}

// 不正なメッシュ (範囲外インデックス) の書き出しはstd::invalid_argument
TEST(ObjIOTest, Write_ThrowsInvalidArgumentWhenMeshInvalid) {
    auto mesh = MakeUnitQuad();
    mesh.indices[0] = 100;
    EXPECT_THROW(
        i_ext::WriteObj(mesh, OutputPath("obj_invalid.obj")),
        std::invalid_argument);
}



/**
 * 全角パス
 */

// 全角名のディレクトリ (未作成の親ディレクトリを含む) で書き出し→読み戻しができる
TEST(ObjIOTest, UnicodePath_RoundTrip) {
    using igesio::tests::kUnicodeName;
    const igesio::tests::UnicodeTempDir dir("obj_io");
    const std::string path =
            dir.JoinUtf8(kUnicodeName + "/" + kUnicodeName + ".obj");
    ASSERT_TRUE(i_ext::WriteObj(MakeUnitQuadWithChannels(), path));
    EXPECT_TRUE(fs::is_regular_file(igesio::utils::PathFromUtf8(path)));

    ExpectSameGeometry(MakeUnitQuadWithChannels(), i_ext::ReadObj(path));
    EXPECT_NE(i_ext::ReadObjAsEntity(path), nullptr);
}

// 存在しない全角パスはFileOpenErrorで、文言にUTF-8のパスを含む
TEST(ObjIOTest, UnicodePath_Read_ThrowsFileOpenErrorWhenFileMissing) {
    const igesio::tests::UnicodeTempDir dir("obj_io_missing");
    const std::string path =
            dir.JoinUtf8(igesio::tests::kUnicodeName + ".obj");
    try {
        i_ext::ReadObj(path);
        FAIL() << "FileOpenError was not thrown";
    } catch (const igesio::FileOpenError& e) {
        EXPECT_NE(std::string(e.what()).find(path), std::string::npos);
    }
}
