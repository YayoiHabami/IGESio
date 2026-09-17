/**
 * @file tests/extensions/machines/scene/test_geometry_loader.cpp
 * @brief 形状読込 (scene/geometry_loader) のテスト
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 対象: LoadGeometry / LoadGeometryMesh / GeometrySpec::DisplayName
 *       - 正常系: STL (inch換算・溶接・折り目法線)、OBJ、プリミティブ (box・cylinder)、
 *         IGES (ルートAssemblyが子になる)、色・不透明度の反映と変換・名前を
 *         持たないこと (単位行列・空・可視)
 *       - 正常系 (退化): 名前の無い形状の表示名 (ファイル名・プリミティブ種別)
 *       - スキップ: STEP (info)・inchのIGES (警告) は`nullptr`. 診断の文脈は
 *         呼び出し側が与えたもの
 *       - 異常系: 存在しないSTLの`FileOpenError`、寸法が正でないプリミティブの
 *         `invalid_argument`
 *       TODO: OBJの法線付きファイル (法線をそのまま使う経路) はテストデータに無い
 */
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "igesio/common/color.h"
#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/entity_type.h"
#include "igesio/models/assembly.h"
#include "igesio/models/global_param.h"
#include "igesio/models/iges_data.h"
#include "igesio/reader.h"
#include "igesio/writer.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/scene/geometry_loader.h"
#include "../machine/machines_for_testing.h"

namespace {

namespace fs = std::filesystem;
namespace mc = igesio::extensions::machines;
namespace i_ent = igesio::entities;
using igesio::Vector3d;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 診断の文脈 (呼び出し側が与える)
constexpr std::string_view kContext = "component[X].geometry[0]";

/// @brief テストデータのディレクトリ (tests/test_data)
const fs::path kTestDataDir = machines_test::kFixturePath.parent_path().parent_path();

/// @brief モデルファイルのディレクトリ (tests/test_data/machines/projects/models)
const fs::path kModelsDir = machines_test::kFixturePath.parent_path() / "projects" / "models";

/// @brief ファイル参照形式の形状を作る
mc::GeometrySpec FileSpec(const fs::path& path, const double unit_scale = 1.0) {
    mc::GeometrySpec spec;
    spec.source = path;
    spec.raw_path = path.filename().generic_string();
    spec.file_unit_scale = unit_scale;
    return spec;
}

/// @brief 10×20×30の直方体プリミティブ
mc::GeometrySpec BoxSpec() {
    mc::GeometrySpec spec;
    mc::PrimitiveSpec primitive;
    primitive.kind = mc::PrimitiveSpec::Kind::kBox;
    primitive.size = Vector3d(10.0, 20.0, 30.0);
    spec.source = primitive;
    spec.name = "box";
    return spec;
}

/// @brief 各頂点の法線が単位の軸方向ベクトルであるか (面法線と一致するか)
bool NormalsAreAxisAligned(const igesio::numerics::TriangleMeshd& mesh) {
    for (Eigen::Index i = 0; i < mesh.normals.cols(); ++i) {
        const Vector3d n = mesh.normals.col(i);
        if (std::abs(n.norm() - 1.0) > kTol) return false;
        if (std::abs(n.cwiseAbs().maxCoeff() - 1.0) > kTol) return false;
    }
    return true;
}

/// @brief inch単位のIGESファイル (線分1本) を一時ディレクトリに作る
fs::path MakeInchIges() {
    const fs::path dir = fs::temp_directory_path() / "igesio_geometry_loader_test";
    fs::create_directories(dir);
    const fs::path path = dir / "inch_line.iges";
    igesio::models::IgesData data;
    data.global_section.units_flag = igesio::models::UnitFlag::kInch;
    data.Root().AddEntity(i_ent::MakeLine(Vector3d::Zero(), Vector3d(1.0, 0.0, 0.0)));
    igesio::WriteIges(data, path.string());
    return path;
}

}  // namespace



TEST(GeometryLoaderTest, Stl_ScaledAndWelded) {
    std::vector<mc::Diagnostic> warnings;
    const auto mesh = mc::LoadGeometryMesh(FileSpec(kModelsDir / "cube.stl", 25.4),
                                           mc::GeometryLoadOptions{}, kContext, &warnings);
    ASSERT_TRUE(mesh.has_value());
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(mesh->TriangleCount(), 12u);
    // 溶接で8頂点になった後、折り目 (90°) で3面分に分かれて24頂点になる
    EXPECT_EQ(mesh->VertexCount(), 24u);
    ASSERT_TRUE(mesh->HasNormals());
    EXPECT_TRUE(NormalsAreAxisAligned(*mesh));
    const Vector3d min = mesh->positions.rowwise().minCoeff();
    const Vector3d max = mesh->positions.rowwise().maxCoeff();
    EXPECT_TRUE(min.isZero(kTol));
    EXPECT_TRUE(max.isApprox(Vector3d(25.4, 25.4, 25.4), kTol));

    // 溶接しなければ36頂点のままファイルの法線を使う
    mc::GeometryLoadOptions options;
    options.stl.weld_vertices = false;
    const auto raw = mc::LoadGeometryMesh(FileSpec(kModelsDir / "cube.stl"),
                                          options, kContext, &warnings);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(raw->VertexCount(), 36u);
    EXPECT_TRUE(NormalsAreAxisAligned(*raw));
}

TEST(GeometryLoaderTest, Obj_Reads) {
    std::vector<mc::Diagnostic> warnings;
    const auto mesh = mc::LoadGeometryMesh(FileSpec(kModelsDir / "cube.obj"),
                                           mc::GeometryLoadOptions{}, kContext, &warnings);
    ASSERT_TRUE(mesh.has_value());
    EXPECT_EQ(mesh->TriangleCount(), 12u);
    EXPECT_EQ(mesh->VertexCount(), 24u);   // 法線が無いので折り目で分かれる
    EXPECT_TRUE(NormalsAreAxisAligned(*mesh));

    const auto assembly = mc::LoadGeometry(FileSpec(kModelsDir / "cube.obj"),
                                           mc::GeometryLoadOptions{}, kContext, &warnings);
    ASSERT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->GetEntityCount(), 1u);
    EXPECT_EQ(assembly->FindEntitiesByType(i_ent::EntityType::kNonIges).size(), 1u);
    EXPECT_TRUE(assembly->GetChildAssemblies().empty());
    EXPECT_EQ(FileSpec(kModelsDir / "cube.obj").DisplayName(), "cube.obj");
    EXPECT_TRUE(warnings.empty());
}

TEST(GeometryLoaderTest, Primitive_BoxAndCylinder) {
    std::vector<mc::Diagnostic> warnings;
    const auto box = mc::LoadGeometryMesh(BoxSpec(), mc::GeometryLoadOptions{},
                                          kContext, &warnings);
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(box->TriangleCount(), 12u);
    EXPECT_EQ(box->VertexCount(), 24u);
    EXPECT_TRUE(box->positions.rowwise().maxCoeff().isApprox(Vector3d(5.0, 10.0, 15.0), kTol));

    mc::GeometrySpec spec;
    mc::PrimitiveSpec cylinder;
    cylinder.kind = mc::PrimitiveSpec::Kind::kCylinder;
    cylinder.radius = 4.0;
    cylinder.height = 6.0;
    spec.source = cylinder;
    const auto assembly = mc::LoadGeometry(spec, mc::GeometryLoadOptions{}, kContext,
                                           &warnings);
    ASSERT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->GetEntityCount(), 1u);
    // 名前の無いプリミティブの表示名は種別名、名前があればその名前
    EXPECT_EQ(spec.DisplayName(), "cylinder");
    EXPECT_EQ(BoxSpec().DisplayName(), "box");
    EXPECT_TRUE(warnings.empty());
}

TEST(GeometryLoaderTest, Iges_RootIsChild) {
    const fs::path path = kTestDataDir / "single_rounded_cube.iges";
    const mc::GeometrySpec spec = FileSpec(path);
    std::vector<mc::Diagnostic> warnings;
    const auto assembly = mc::LoadGeometry(spec, mc::GeometryLoadOptions{}, kContext,
                                           &warnings);
    ASSERT_NE(assembly, nullptr);
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(assembly->GetEntityCount(), 0u);
    ASSERT_EQ(assembly->GetChildAssemblies().size(), 1u);
    EXPECT_GT(assembly->GetChildAssemblies()[0]->GetEntityCount(), 0u);

    // 変換は単位行列なので、ワールドBBは直接読んだものと一致する
    EXPECT_TRUE(assembly->GetGlobalTransform().isIdentity(kTol));
    const auto direct = igesio::ReadIges(path.string());
    const auto original = direct.Root().GetWorldBoundingBox();
    const auto loaded = assembly->GetWorldBoundingBox();
    ASSERT_TRUE(original.has_value());
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->GetControl().isApprox(original->GetControl(), 1e-6));

    // メッシュとしては読めない (info)
    const auto mesh = mc::LoadGeometryMesh(spec, mc::GeometryLoadOptions{}, kContext,
                                           &warnings);
    EXPECT_FALSE(mesh.has_value());
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0].severity, mc::Severity::kInfo);
}

TEST(GeometryLoaderTest, Assembly_OverridesAppliedAndUnplaced) {
    mc::GeometrySpec spec = FileSpec(kModelsDir / "cube.stl");
    spec.name = "vise";
    spec.color = igesio::Color{0.5, 0.25, 0.0};
    spec.opacity = 0.5f;
    spec.line = 12;
    const auto assembly = mc::LoadGeometry(spec, mc::GeometryLoadOptions{}, kContext,
                                           nullptr);
    ASSERT_NE(assembly, nullptr);
    ASSERT_TRUE(assembly->Display().color_override.has_value());
    EXPECT_NEAR(assembly->Display().color_override->g, 0.25, kTol);
    ASSERT_TRUE(assembly->Display().opacity_override.has_value());
    EXPECT_NEAR(static_cast<double>(*assembly->Display().opacity_override), 0.5, 1e-6);
    // 変換・名前・可視性は呼び出し側が決めるので、読込器は付けない
    EXPECT_TRUE(assembly->GetGlobalTransform().isIdentity(kTol));
    EXPECT_TRUE(assembly->Metadata().name.empty());
    EXPECT_TRUE(assembly->Display().visible);
    EXPECT_EQ(spec.DisplayName(), "vise");

    // 色・不透明度を指定しなければオーバーライドは付かない
    const auto plain = mc::LoadGeometry(FileSpec(kModelsDir / "cube.stl"),
                                        mc::GeometryLoadOptions{}, kContext, nullptr);
    EXPECT_FALSE(plain->Display().color_override.has_value());
    EXPECT_FALSE(plain->Display().opacity_override.has_value());
}

TEST(GeometryLoaderTest, Skips_StepAndInch) {
    std::vector<mc::Diagnostic> warnings;
    mc::GeometrySpec step = FileSpec(kModelsDir / "missing_part.stp");
    step.line = 7;
    EXPECT_EQ(mc::LoadGeometry(step, mc::GeometryLoadOptions{}, kContext, &warnings),
              nullptr);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0].severity, mc::Severity::kInfo);
    EXPECT_EQ(warnings[0].context, kContext);   // 文脈は呼び出し側が与えたもの
    EXPECT_EQ(warnings[0].line, 7);
    EXPECT_NE(warnings[0].message.find("unsupported"), std::string::npos);

    warnings.clear();
    EXPECT_EQ(mc::LoadGeometry(FileSpec(kModelsDir / "unknown.xyz"),
                               mc::GeometryLoadOptions{}, kContext, &warnings), nullptr);
    EXPECT_EQ(warnings.size(), 1u);

    warnings.clear();
    const fs::path inch = MakeInchIges();
    EXPECT_EQ(mc::LoadGeometry(FileSpec(inch), mc::GeometryLoadOptions{}, kContext,
                               &warnings), nullptr);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0].severity, mc::Severity::kWarning);
    EXPECT_EQ(warnings[0].context, kContext);
    EXPECT_NE(warnings[0].message.find("not millimeters"), std::string::npos);
    fs::remove_all(inch.parent_path());
}

TEST(GeometryLoaderTest, Throws_FileOpenErrorAndInvalidArgument) {
    EXPECT_THROW(mc::LoadGeometry(FileSpec(kModelsDir / "missing.stl"),
                                  mc::GeometryLoadOptions{}, kContext, nullptr),
                 igesio::FileOpenError);
    EXPECT_THROW(mc::LoadGeometryMesh(FileSpec(kModelsDir / "missing.obj"),
                                      mc::GeometryLoadOptions{}, kContext, nullptr),
                 igesio::FileOpenError);
    mc::GeometrySpec spec = BoxSpec();
    std::get<mc::PrimitiveSpec>(spec.source).size = Vector3d(10.0, 0.0, 30.0);
    EXPECT_THROW(mc::LoadGeometry(spec, mc::GeometryLoadOptions{}, kContext, nullptr),
                 std::invalid_argument);
}
