/**
 * @file tests/extensions/machines/project/test_setup.cpp
 * @brief 加工セットアップ (project/setup) のテスト
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 対象: MachiningSetup (BaseQ / Tools / WorkFrames / Models / Geometries /
 *       ToolOffsets / InitialWorkOffset / ResolveAttach)
 *       - 正常系: 初期姿勢の重ね合わせ (σを含む)、簡易工具の解決とゲージ長、
 *         ライブラリ参照工具のコールバック解決、ワーク座標系の登録値形式
 *         (直進のみ・回転軸あり・`from = "machine"`) と幾何形式 (取り付け先・
 *         モデル経由)、暗黙のG54、モデルの同次変換と所属、形状の同次変換一覧 (機械部品→
 *         モデルの順・`local_frame`の合成・干渉専用形状の保持)、工具オフセットの
 *         既定値、干渉ペア規則の正常系、取り付け先の解決
 *       - 正常系 (退化): ワークオフセット・モデルが無い定義
 *       - 異常系: ワーク座標系の取り付け先が`work_mount`に至らない、チェーン外の
 *         `values`、取り付け先の不在・閉路、ペア規則の各違反
 *       TODO: `MachineModel`構築失敗の`invalid_argument`は読込済みの定義では
 *             起きないため検証しない
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/project_io.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "../machine/machines_for_testing.h"
#include "./projects_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;
using machines_test::MinimalXyzAc;
using projects_test::DefaultOptions;
using projects_test::kProjectsDir;
using projects_test::MinimalProject;
using projects_test::ReadProjectText;
using projects_test::ReadProjectWithMachine;
using projects_test::Replace;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 実例機の工具取り付け点の原点 (ゼロポーズ機械座標)
const Vector3d kToolOrigin(0.0, -180.0, 250.5);

/// @brief 最小構成のプロジェクトからセットアップを作る
mc::MachiningSetup MakeSetup(const std::string& toml = MinimalProject(),
                             const mc::SetupOptions& options = {}) {
    return mc::MachiningSetup(ReadProjectText(toml), options);
}

/// @brief `DataFormatError`が投げられ、メッセージに指定語を含むことを検証する
void ExpectDataFormatError(const mc::ProjectDefinition& project, const std::string& keyword) {
    try {
        mc::MachiningSetup setup(project);
        FAIL() << "DataFormatError was not thrown (expected: " << keyword << ")";
    } catch (const igesio::DataFormatError& e) {
        EXPECT_NE(std::string(e.what()).find(keyword), std::string::npos)
                << "message: " << e.what();
    }
}

/// @brief 最小構成のプロジェクトにセクションを追記する
std::string WithSection(const std::string& section) {
    return MinimalProject() + "\n" + section;
}

/// @brief 簡易ボール工具の輪郭をライブラリ由来に見立てて返すコールバック
/// @note ゲージラインを未設定にし、`GaugeLength`のフォールバック (ホルダ上端) が
///       セットアップで確定されることを検証する
std::optional<mc::ToolAssemblySpec> FakeResolver(const mc::ToolLibrarySpec& library,
                                                 const mc::LibraryToolRef& ref) {
    mc::SimpleToolSpec simple;
    simple.diameter = 8.0;
    simple.cutting_length = 16.0;
    simple.tool_length = 50.0;
    simple.overhang = 30.0;
    simple.holder_diameter = 30.0;
    simple.holder_length = 40.0;
    mc::ToolAssemblySpec spec;
    spec.name = library.alias + "#" + std::to_string(ref.assembly);
    spec.profile = mc::MakeSimpleToolProfile(simple, nullptr);
    spec.profile.gauge_line_z.reset();
    return spec;
}

/// @brief 登録値形式のW_0を閉形式 F_wm(q*)⁻¹·T(p_from) で計算する
Matrix4d ExpectedRegisteredFrame(const mc::MachiningSetup& setup, const mc::NcValues& values,
                                 const bool from_tool_mount) {
    const mc::MachineModel& model = setup.Model();
    mc::NcValues nc;
    for (const std::string& name : model.ChainRegisters()) nc.Set(name, values.GetOr(name, 0.0));
    const auto frames = mc::Forward(model, mc::JointsFromNc(model, nc, setup.BaseQ()));
    Vector3d p_from = Vector3d::Zero();
    if (from_tool_mount) {
        p_from = mc::TranslationPart(frames[model.ToolMountIndex()]
                                     * model.MountPlacement(mc::MountKind::kToolMount));
    }
    return mc::RigidInverse(frames[model.WorkMountIndex()]) * mc::Translation(p_from);
}

}  // namespace



/**
 * ---- 初期姿勢・工具 ----
 */

TEST(SetupTest, BaseQ_OverlaysInitialAxes) {
    const auto setup = MakeSetup(Replace(MinimalProject(), "Z = 100.0", "Z = 100.0\nA = 30.0"));
    const mc::MachineModel& model = setup.Model();
    ASSERT_EQ(setup.BaseQ().Size(), model.Axes().size());
    // 工具側のZはσ=+1、ワーク側のAはσ=-1で軸変位量になる
    EXPECT_NEAR(setup.BaseQ()[*model.FindAxis("Z")], 100.0, kTol);
    EXPECT_NEAR(setup.BaseQ()[*model.FindAxis("A")], -ToRadians(30.0), kTol);
    EXPECT_NEAR(setup.BaseQ()[*model.FindAxis("X")], 0.0, kTol);
    const mc::NcValues nc = mc::NcFromJoints(model, setup.BaseQ());
    EXPECT_NEAR(nc.At("A"), ToRadians(30.0), kTol);
    EXPECT_EQ(setup.InitialTool(), 1);
    EXPECT_EQ(setup.Project().name, "minimal");
    EXPECT_TRUE(setup.Warnings().empty());
}

TEST(SetupTest, Tools_SimpleIsResolved) {
    const auto setup = MakeSetup();
    ASSERT_EQ(setup.Tools().size(), 1u);
    const mc::ToolAssemblySpec& tool = setup.Tools().at(1);
    EXPECT_EQ(tool.number, 1);
    EXPECT_EQ(tool.name, "ball");
    EXPECT_EQ(tool.control_point, mc::ControlPoint::kTip);
    EXPECT_NEAR(tool.profile.gauge_line_z.value_or(0.0), 40.0 + 50.0, kTol);
    EXPECT_NEAR(tool.profile.command_point_z, 0.0, kTol);
    EXPECT_EQ(tool.profile.elements.size(), 3u);

    const auto center = MakeSetup(Replace(
            Replace(MinimalProject(), "cutter = \"ball\"",
                    "cutter = \"ball\"\ncommand_point = \"center\""),
            "number = 1", "number = 1\ncontrol_point = \"gauge\""));
    EXPECT_NEAR(center.Tools().at(1).profile.command_point_z, 5.0, kTol);
    EXPECT_EQ(center.Tools().at(1).control_point, mc::ControlPoint::kGauge);
    const auto named = MakeSetup(Replace(MinimalProject(), "number = 1",
                                         "number = 1\nname = \"R5 ball\""));
    EXPECT_EQ(named.Tools().at(1).name, "R5 ball");
}

TEST(SetupTest, Tools_GaugeLengthOverride) {
    const auto setup = MakeSetup(Replace(MinimalProject(), "number = 1",
                                         "number = 1\ngauge_length = 80.0"));
    EXPECT_NEAR(setup.Tools().at(1).profile.gauge_line_z.value_or(0.0), 80.0, kTol);
    EXPECT_NEAR(setup.Tools().at(1).profile.GaugeLength(nullptr), 80.0, kTol);
}

TEST(SetupTest, Tools_LibraryRefUnresolved) {
    const auto project = mc::ReadProject(kProjectsDir / "library_ref.toml", DefaultOptions());
    const mc::MachiningSetup setup(project);
    ASSERT_EQ(setup.Tools().size(), 1u);
    EXPECT_NE(setup.Tools().find(3), setup.Tools().end());
    ASSERT_EQ(setup.Warnings().size(), 2u);
    EXPECT_NE(setup.Warnings()[0].message.find("library tool #1 is unresolved"),
              std::string::npos);
    EXPECT_EQ(setup.Warnings()[1].context, "[[tool]](#2)");
}

TEST(SetupTest, Tools_LibraryRefResolvedByCallback) {
    const auto project = mc::ReadProject(kProjectsDir / "library_ref.toml", DefaultOptions());
    mc::SetupOptions options;
    options.tool_resolver = FakeResolver;
    const mc::MachiningSetup setup(project, options);
    ASSERT_EQ(setup.Tools().size(), 3u);
    // #1: 名前はコールバック、ゲージ長は省略なので輪郭のホルダ上端で確定 (警告)
    const mc::ToolAssemblySpec& first = setup.Tools().at(1);
    EXPECT_EQ(first.name, "std#2");
    EXPECT_EQ(first.number, 1);
    EXPECT_NEAR(first.profile.gauge_line_z.value_or(0.0), 30.0 + 40.0, kTol);
    ASSERT_EQ(setup.Warnings().size(), 1u);
    EXPECT_EQ(setup.Warnings()[0].context, "[[tool]](#1)");
    EXPECT_NE(setup.Warnings()[0].message.find("gauge line not specified"), std::string::npos);
    // #2: エントリの名前・ゲージ長・制御点が優先される
    const mc::ToolAssemblySpec& second = setup.Tools().at(2);
    EXPECT_EQ(second.name, "Special 7");
    EXPECT_NEAR(second.profile.gauge_line_z.value_or(0.0), 150.0, kTol);
    EXPECT_EQ(second.control_point, mc::ControlPoint::kGauge);
    EXPECT_EQ(setup.Tools().at(3).name, "square");
}



/**
 * ---- ワーク座標系 ----
 */

TEST(SetupTest, WorkFrame_RegisteredValues) {
    const auto setup = MakeSetup();
    ASSERT_EQ(setup.WorkFrames().size(), 2u);
    const mc::WorkFrame& g54 = setup.WorkFrames()[0];
    EXPECT_EQ(g54.id, "G54");
    EXPECT_EQ(g54.carrier, setup.Model().WorkMountIndex());
    // ゲージ点 (0,-180,250.5) をテーブル中心へ置く機械位置なので原点は機械原点
    EXPECT_TRUE(mc::TranslationPart(g54.w0).isZero(kTol)) << g54.w0;
    EXPECT_TRUE(mc::RotationPart(g54.w0).isIdentity(kTol));
    EXPECT_EQ(setup.InitialWorkOffset(), "G54");
    EXPECT_EQ(setup.FindWorkFrame("G55"), &setup.WorkFrames()[1]);
    EXPECT_EQ(setup.FindWorkFrame("G59"), nullptr);
}

TEST(SetupTest, WorkFrame_RegisteredWithRotary) {
    const auto setup = MakeSetup(Replace(MinimalProject(),
                                         "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
                                         "values = { C = 90.0 }"));
    const Matrix4d& w0 = setup.WorkFrames()[0].w0;
    const mc::NcValues values{{"C", ToRadians(90.0)}};
    EXPECT_TRUE(w0.isApprox(ExpectedRegisteredFrame(setup, values, true), kTol)) << w0;
    // ワーク側σ=-1: C=90°の軸変位量は-90°なので、F_wm⁻¹の回転は+90°
    EXPECT_TRUE(mc::RotationPart(w0).isApprox(
            mc::RotationAboutAxis(Vector3d::UnitZ(), ToRadians(90.0)), kTol)) << w0;
    EXPECT_TRUE(mc::TranslationPart(w0).isApprox(
            mc::RotationAboutAxis(Vector3d::UnitZ(), ToRadians(90.0)) * kToolOrigin, kTol));
}

TEST(SetupTest, WorkFrame_FromMachine) {
    const auto setup = MakeSetup(Replace(MinimalProject(),
                                         "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
                                         "values = { A = 30.0 }\nfrom = \"machine\""));
    const Matrix4d& w0 = setup.WorkFrames()[0].w0;
    const mc::NcValues values{{"A", ToRadians(30.0)}};
    EXPECT_TRUE(w0.isApprox(ExpectedRegisteredFrame(setup, values, false), kTol)) << w0;
    // p_fromは機械原点なので、W_0はF_wm(q*)⁻¹そのもの (A軸は(0,0,60)を通る)
    const Matrix4d f_wm = mc::RotationAboutLine(Vector3d::UnitX(), Vector3d(0.0, 0.0, 60.0),
                                                -ToRadians(30.0));
    EXPECT_TRUE(w0.isApprox(mc::RigidInverse(f_wm), kTol)) << w0;
}

TEST(SetupTest, WorkFrame_Geometric) {
    const auto setup = MakeSetup(Replace(
            MinimalProject(), "attach = \"stock\"\norigin = [0.0, 0.0, 20.0]",
            "origin = [1.0, 2.0, 3.0]\nrotation_axis_angle = { axis = [0, 0, 1], angle = 90 }"));
    const mc::WorkFrame& g55 = setup.WorkFrames()[1];
    const Matrix4d expected = setup.Model().MountPlacement(mc::MountKind::kWorkMount)
            * mc::MakeRigid(mc::RotationAboutAxis(Vector3d::UnitZ(), ToRadians(90.0)),
                            Vector3d(1.0, 2.0, 3.0));
    EXPECT_TRUE(g55.w0.isApprox(expected, kTol)) << g55.w0;
    EXPECT_EQ(g55.carrier, setup.Model().WorkMountIndex());
}

TEST(SetupTest, WorkFrame_GeometricChainedThroughModel) {
    const auto setup = MakeSetup();
    const mc::WorkFrame& g55 = setup.WorkFrames()[1];
    // stockの同次変換T(0,0,20)の上にorigin (0,0,20) → ストック上面中心 (0,0,40)
    EXPECT_TRUE(mc::TranslationPart(g55.w0).isApprox(Vector3d(0.0, 0.0, 40.0), kTol)) << g55.w0;
    EXPECT_TRUE(mc::RotationPart(g55.w0).isIdentity(kTol));
    EXPECT_EQ(g55.carrier, setup.Model().WorkMountIndex());
}

TEST(SetupTest, WorkFrame_ImplicitG54) {
    const std::string none = Replace(
            Replace(MinimalProject(), "[[work_offset]]\nid = \"G54\"\n"
                    "values = { X = 0.0, Y = 180.0, Z = -250.5 }\n", ""),
            "[[work_offset]]\nid = \"G55\"\nattach = \"stock\"\norigin = [0.0, 0.0, 20.0]\n", "");
    const auto setup = MakeSetup(none);
    ASSERT_EQ(setup.WorkFrames().size(), 1u);
    EXPECT_EQ(setup.WorkFrames()[0].id, "G54");
    EXPECT_EQ(setup.InitialWorkOffset(), "G54");
    // 全軸0 (初期姿勢のZ=100は工具側なのでp_fromに効く) のゲージ点がワーク原点
    const Matrix4d expected = ExpectedRegisteredFrame(setup, mc::NcValues{}, true);
    EXPECT_TRUE(setup.WorkFrames()[0].w0.isApprox(expected, kTol));
    EXPECT_TRUE(mc::TranslationPart(setup.WorkFrames()[0].w0).isApprox(kToolOrigin, kTol));
    EXPECT_TRUE(setup.Project().work_offsets.empty());
}

TEST(SetupTest, WorkFrame_ThrowsDataFormatErrorWhenAttachIsNotUnderWorkMount) {
    ExpectDataFormatError(ReadProjectText(Replace(MinimalProject(), "attach = \"stock\"",
                                                  "attach = \"tool_mount\"")),
                          "attach does not lead to work_mount");
    ExpectDataFormatError(ReadProjectText(Replace(MinimalProject(), "attach = \"stock\"",
                                                  "attach = \"Z\"")),
                          "attach does not lead to work_mount");
    ExpectDataFormatError(ReadProjectText(Replace(MinimalProject(), "id = \"G54\"",
                                                  "id = \"G54\"\nattach = \"Spindle\"")),
                          "[[work_offset]](G54): attach does not lead to work_mount");
}

TEST(SetupTest, WorkFrame_ThrowsDataFormatErrorWhenRegisterIsOffChain) {
    const std::string machine = MinimalXyzAc() + R"(
[[component]]
name = "Door"
type = "linear"
parent = "base"

[component.axis]
register = "U"
direction = [0, 1, 0]
limits = [0, 500]
)";
    const auto project = ReadProjectWithMachine(
            machine,
            Replace(Replace(MinimalProject(), "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\"\n", ""),
                    "values = { X = 0.0, Y = 180.0, Z = -250.5 }", "values = { U = 10.0 }"),
            "igesio_setup_door");
    ExpectDataFormatError(project, "U is not on the kinematic chain");
}



/**
 * ---- モデル・取り付け先 ----
 */

TEST(SetupTest, Models_PlacementAndCarrier) {
    const auto setup = MakeSetup(WithSection(
            "[[model]]\nname = \"part\"\nrole = \"design\"\nprimitive = \"box\"\nsize = [1, 1, 1]\n"
            "attach = \"stock\"\norigin = [1, 2, 3]\n"
            "rotation_axis_angle = { axis = [1, 0, 0], angle = 90 }\n"
            "[[model]]\nname = \"cover\"\nrole = \"display\"\nprimitive = \"box\"\n"
            "size = [1, 1, 1]\nattach = \"Z\"\norigin = [0, 0, 5]\n"));
    ASSERT_EQ(setup.Models().size(), 3u);
    const mc::PlacedModel& stock = setup.Models()[0];
    EXPECT_EQ(stock.spec.name, "stock");
    EXPECT_EQ(stock.carrier, setup.Model().WorkMountIndex());
    EXPECT_TRUE(stock.placement.isApprox(mc::Translation(Vector3d(0.0, 0.0, 20.0)), kTol));
    const mc::PlacedModel& part = setup.Models()[1];
    const Matrix4d expected = mc::Translation(Vector3d(0.0, 0.0, 20.0))
            * mc::MakeRigid(mc::RotationAboutAxis(Vector3d::UnitX(), ToRadians(90.0)),
                            Vector3d(1.0, 2.0, 3.0));
    EXPECT_TRUE(part.placement.isApprox(expected, kTol)) << part.placement;
    EXPECT_EQ(part.carrier, setup.Model().WorkMountIndex());
    const mc::PlacedModel& cover = setup.Models()[2];
    EXPECT_EQ(cover.carrier, *setup.Model().FindComponent("Z"));
    EXPECT_TRUE(cover.placement.isApprox(mc::Translation(Vector3d(0.0, 0.0, 5.0)), kTol));
    EXPECT_TRUE(setup.Warnings().empty());   // displayは機械側でも警告しない
}

TEST(SetupTest, Models_WarnsWhenStockIsNotUnderWorkMount) {
    // ストックを工具側のXに取り付ける (G55はストック経由でなくwork_mount直付けにする)
    const auto setup = MakeSetup(Replace(
            Replace(MinimalProject(), "attach = \"stock\"\norigin", "origin"),
            "role = \"stock\"\n", "role = \"stock\"\nattach = \"X\"\n"));
    ASSERT_EQ(setup.Warnings().size(), 1u);
    EXPECT_EQ(setup.Warnings()[0].context, "[[model]](stock)");
    EXPECT_NE(setup.Warnings()[0].message.find("stock model is not attached under work_mount"),
              std::string::npos);
    EXPECT_EQ(setup.Models()[0].carrier, *setup.Model().FindComponent("X"));
}

TEST(SetupTest, Geometries_MachinePartsThenModels) {
    const auto setup = MakeSetup(WithSection(
            "[[model]]\nname = \"cover\"\nrole = \"display\"\nprimitive = \"box\"\n"
            "size = [1, 1, 1]\nattach = \"Z\"\norigin = [0, 0, 5]\nvisible = false\n"));
    // 実例機の機械部品7つ (Component()の順. 先頭はbase) → モデル (stock・cover) の順
    constexpr std::size_t kPartCount = 7;
    ASSERT_EQ(setup.Geometries().size(), kPartCount + 2);
    for (std::size_t i = 0; i < kPartCount; ++i) {
        const mc::GeometryInstance& part = setup.Geometries()[i];
        EXPECT_EQ(part.kind, mc::GeometryInstance::Kind::kMachinePart);
        EXPECT_EQ(part.carrier, *setup.Model().FindComponent(part.owner));
        EXPECT_FALSE(part.role.has_value());
        EXPECT_TRUE(part.collision);
        EXPECT_TRUE(part.visible);
    }
    const mc::GeometryInstance& first = setup.Geometries()[0];
    EXPECT_EQ(first.owner, "base");
    EXPECT_EQ(first.carrier, 0u);
    EXPECT_TRUE(first.placement.isIdentity(kTol));   // local_frameもT(origin)·Rも単位

    const mc::GeometryInstance& stock = setup.Geometries()[kPartCount];
    EXPECT_EQ(stock.kind, mc::GeometryInstance::Kind::kModel);
    EXPECT_EQ(stock.owner, "stock");
    EXPECT_EQ(stock.carrier, setup.Model().WorkMountIndex());
    EXPECT_EQ(stock.role.value_or(mc::ModelRole::kDisplay), mc::ModelRole::kStock);
    EXPECT_TRUE(stock.placement.isApprox(setup.Models()[0].placement, kTol));
    EXPECT_TRUE(stock.collision);
    EXPECT_TRUE(stock.visible);

    // 干渉専用 (visible = false) のモデルも並びに含まれる
    const mc::GeometryInstance& cover = setup.Geometries()[kPartCount + 1];
    EXPECT_EQ(cover.owner, "cover");
    EXPECT_EQ(cover.carrier, *setup.Model().FindComponent("Z"));
    EXPECT_EQ(cover.role.value_or(mc::ModelRole::kStock), mc::ModelRole::kDisplay);
    EXPECT_TRUE(cover.placement.isApprox(mc::Translation(Vector3d(0.0, 0.0, 5.0)), kTol));
    EXPECT_FALSE(cover.collision);
    EXPECT_FALSE(cover.visible);
}

TEST(SetupTest, Geometries_MachinePartPlacementComposesLocalFrame) {
    // Xにlocal_frame (原点(0, 0, -50)) を与え、形状はコンポーネント座標でorigin = [1, 0, 0]
    const std::string machine = Replace(
            MinimalXyzAc(),
            "[[component.geometry]]\nname = \"x-box\"\nprimitive = \"box\"\n"
            "size = [10, 20, 30]\n",
            "[component.local_frame]\norigin = [0, 0, -50]\n\n"
            "[[component.geometry]]\nname = \"x-box\"\nprimitive = \"box\"\n"
            "size = [10, 20, 30]\norigin = [1, 0, 0]\n");
    const auto project = ReadProjectWithMachine(
            machine,
            Replace(MinimalProject(), "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\"\n", ""),
            "igesio_setup_geometry_local_frame");
    const mc::MachiningSetup setup(project);
    // 定義には書かれた値だけが残り、C_cとの合成は`Geometries()`で行われる
    const auto& entry = setup.Model().Spec(*setup.Model().FindComponent("X")).geometries[0];
    EXPECT_TRUE(entry.placement.origin.isApprox(Vector3d(1.0, 0.0, 0.0), kTol));
    ASSERT_EQ(setup.Geometries().size(), 2u);
    EXPECT_TRUE(setup.Geometries()[0].placement.isApprox(
            mc::Translation(Vector3d(1.0, 0.0, -50.0)), kTol));
}

TEST(SetupTest, Attach_ThrowsDataFormatErrorOnCycleAndMissing) {
    ExpectDataFormatError(ReadProjectText(WithSection(
            "[[model]]\nname = \"a\"\nrole = \"display\"\nprimitive = \"box\"\nsize = [1, 1, 1]\n"
            "attach = \"b\"\n"
            "[[model]]\nname = \"b\"\nrole = \"display\"\nprimitive = \"box\"\nsize = [1, 1, 1]\n"
            "attach = \"a\"\n")),
            "attach forms a cycle");
    ExpectDataFormatError(ReadProjectText(Replace(MinimalProject(), "attach = \"stock\"",
                                                  "attach = \"nowhere\"")),
                          "attach does not exist: nowhere");
    // ワークオフセットとモデルの相互参照も閉路
    ExpectDataFormatError(ReadProjectText(Replace(
            MinimalProject(), "origin = [0.0, 0.0, 20.0]\n\n[[model]]",
            "origin = [0.0, 0.0, 20.0]\n\n[[model]]\nattach = \"G55\"")),
            "attach forms a cycle");
}



/**
 * ---- 工具オフセット・干渉ペア・取り付け先の解決 ----
 */

TEST(SetupTest, ToolOffsets_DefaultsFromTool) {
    const auto setup = MakeSetup(WithSection(
            "[[tool_offset]]\nnumber = 1\ntool = 1\nlength_wear = -0.02\n"
            "[[tool_offset]]\nnumber = 2\n"
            "[[tool_offset]]\nnumber = 3\ntool = 1\nlength = 95.0\nradius = 4.0\n"));
    ASSERT_EQ(setup.ToolOffsets().size(), 3u);
    const mc::ResolvedToolOffset& first = setup.ToolOffsets().at(1);
    EXPECT_NEAR(first.length, 90.0, kTol);   // ゲージ長 = overhang + holder_length
    EXPECT_NEAR(first.radius, 20.0, kTol);   // 最大半径 = ホルダ半径
    EXPECT_NEAR(first.length_wear, -0.02, kTol);
    EXPECT_EQ(first.tool.value_or(0), 1);
    const mc::ResolvedToolOffset& second = setup.ToolOffsets().at(2);
    EXPECT_NEAR(second.length, 0.0, kTol);
    EXPECT_NEAR(second.radius, 0.0, kTol);
    EXPECT_FALSE(second.tool.has_value());
    const mc::ResolvedToolOffset& third = setup.ToolOffsets().at(3);
    EXPECT_NEAR(third.length, 95.0, kTol);
    EXPECT_NEAR(third.radius, 4.0, kTol);
    EXPECT_TRUE(setup.Warnings().empty());
}

TEST(SetupTest, ToolOffsets_WarnsWhenToolIsUnresolved) {
    auto project = mc::ReadProject(kProjectsDir / "library_ref.toml", DefaultOptions());
    mc::ToolOffsetEntry entry;
    entry.number = 1;
    entry.tool = 1;   // 未解決のライブラリ参照工具
    project.tool_offsets.push_back(entry);
    const mc::MachiningSetup setup(project);
    EXPECT_NEAR(setup.ToolOffsets().at(1).length, 0.0, kTol);
    EXPECT_NEAR(setup.ToolOffsets().at(1).radius, 0.0, kTol);
    bool found = false;
    for (const auto& warning : setup.Warnings()) {
        if (warning.context == "[[tool_offset]](#1)") {
            found = true;
            EXPECT_NE(warning.message.find("length defaults to 0"), std::string::npos);
        }
    }
    EXPECT_TRUE(found);
}

TEST(SetupTest, MachinePairs_RulesChecked) {
    const std::string head = "[collision]\n[[collision.machine_pair]]\n";
    const std::string next = "[[collision.machine_pair]]\n";
    EXPECT_NO_THROW(MakeSetup(WithSection(
            head + "targets = [\"Z\", \"A\"]\nsubtree = [true, true]\n"
            + next + "targets = [\"tool\", \"base\"]\nenabled = false\n")));
    ExpectDataFormatError(ReadProjectText(WithSection(
            head + "targets = [\"tool\", \"base\"]\nsubtree = [true, false]\n")),
            "reserved name tool");
    ExpectDataFormatError(ReadProjectText(WithSection(
            head + "targets = [\"Z\", \"A\"]\n" + next + "targets = [\"A\", \"Z\"]\n")),
            "duplicate collision pair");
    ExpectDataFormatError(ReadProjectText(WithSection(
            head + "targets = [\"Z\", \"Spindle\"]\nsubtree = [true, false]\n")),
            "intersect");
    // 工具側のマウントを含む部分木と予約名"tool"も交差する
    ExpectDataFormatError(ReadProjectText(WithSection(
            head + "targets = [\"Z\", \"tool\"]\nsubtree = [true, false]\n")),
            "intersect");
}

TEST(SetupTest, ResolveAttach_ReservedAndNames) {
    const auto setup = MakeSetup();
    const auto [work_index, work_frame] = setup.ResolveAttach("work_mount");
    const auto [named_index, named_frame] = setup.ResolveAttach("Attach");
    EXPECT_EQ(work_index, setup.Model().WorkMountIndex());
    EXPECT_EQ(named_index, work_index);
    EXPECT_TRUE(work_frame.isApprox(named_frame, kTol));
    EXPECT_TRUE(work_frame.isApprox(setup.Model().MountPlacement(mc::MountKind::kWorkMount), kTol));
    const auto [tool_index, tool_frame] = setup.ResolveAttach("tool_mount");
    EXPECT_EQ(tool_index, setup.Model().ToolMountIndex());
    EXPECT_TRUE(mc::TranslationPart(tool_frame).isApprox(kToolOrigin, kTol));
    const auto [z_index, z_frame] = setup.ResolveAttach("Z");
    EXPECT_EQ(z_index, *setup.Model().FindComponent("Z"));
    EXPECT_TRUE(z_frame.isIdentity(kTol));   // local_frameは単位行列
    const auto [stock_index, stock_frame] = setup.ResolveAttach("stock");
    EXPECT_EQ(stock_index, setup.Model().WorkMountIndex());
    EXPECT_TRUE(stock_frame.isApprox(mc::Translation(Vector3d(0.0, 0.0, 20.0)), kTol));
    const auto [g55_index, g55_frame] = setup.ResolveAttach("G55");
    EXPECT_TRUE(g55_frame.isApprox(setup.WorkFrames()[1].w0, kTol));
    EXPECT_EQ(g55_index, setup.Model().WorkMountIndex());
    EXPECT_THROW(setup.ResolveAttach("nowhere"), std::invalid_argument);
}
