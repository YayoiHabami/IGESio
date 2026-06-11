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
 *       - `ReadObjAsEntity`
 *       - 異常系 (ファイルなし・範囲外/不足インデックス・不正メッシュ)
 * @note TODO: マテリアルライブラリ (.mtl) は対象外 (usemtlの参照名のみ保持).
 *       マテリアルが設定された後に未設定へ戻すグループ境界は、標準OBJでは
 *       表現できないため往復対象外 (usemtlは次の指定まで持続する).
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "igesio/common/errors.h"
#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/numerics/mesh/algorithms.h"
#include "igesio/extensions/obj/obj_io.h"

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

// 多角形フェイスはファン分割される (四角形1枚 → 2三角形)
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
