/**
 * @file tests/extensions/machines/machine/test_forward_kinematics.cpp
 * @brief 順運動学 (machine/forward_kinematics) のテスト
 * @author Yayoi Habami
 * @date 2026-09-10
 * @copyright 2026 Yayoi Habami
 * @note 対象: InitialJoints / JointsFromNc / NcFromJoints / Forward (2つの多重定義)
 *       - 正常系 (実例): `t-ZYX-b-AC-w.toml`について、各コンポーネントの剛体変換行列が
 *         閉形式と一致すること、NC↔軸変位量の往復とワーク側軸の符号反転、
 *         戻り値版と出力引数版の一致 (2回目で再確保しないこと)
 *       - 正常系 (退化): ゼロポーズ (既定姿勢) で全コンポーネントの剛体変換行列が単位行列になること
 *       - 異常系: 軸数不一致・出力先が`nullptr`・未知の軸名で`std::invalid_argument`
 * @note 構造の検証中に順運動学を使うテスト (派生構成の従動軸・チェーン外の軸等) は
 *       `test_machine_model.cpp`側にある.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "./machines_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;
using machines_test::kFixturePath;

/// @brief 行列・ベクトル比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 実例TOML (工具側ZYX・ワーク側AC) から運動学モデルを作る
mc::MachineModel FixtureModel() {
    return mc::MachineModel(mc::ReadMachineDefinition(kFixturePath));
}

/// @brief 名前でコンポーネントの剛体変換行列を探す
/// @throw std::logic_error 見つからない場合
const Matrix4d& Placement(const mc::MachineModel& model,
                          const std::vector<Matrix4d>& placements,
                          const std::string& name) {
    const auto index = model.FindComponent(name);
    if (!index.has_value()) throw std::logic_error("component not found: " + name);
    return placements[*index];
}

}  // namespace



// ---- 正常系: 実例TOML ----

TEST(ForwardKinematicsTest, Fixture_ForwardMatchesClosedForm) {
    const mc::MachineModel model = FixtureModel();
    const double a = ToRadians(30.0), c = ToRadians(45.0);
    const Vector3d xyz(10.0, 20.0, -5.0);
    const mc::JointVector q = mc::JointsFromNc(
            model,
            {{"A", a}, {"C", c}, {"X", xyz.x()}, {"Y", xyz.y()}, {"Z", xyz.z()}},
            mc::InitialJoints(model));
    const std::vector<Matrix4d> f = mc::Forward(model, q);
    ASSERT_EQ(f.size(), model.ComponentCount());
    EXPECT_TRUE(Placement(model, f, "base").isIdentity(kTol));
    EXPECT_TRUE(Placement(model, f, "cradle-frame").isIdentity(kTol));
    EXPECT_TRUE(Placement(model, f, "X").isApprox(
            mc::Translation(Vector3d(xyz.x(), 0.0, 0.0)), kTol));
    EXPECT_TRUE(Placement(model, f, "Z").isApprox(mc::Translation(xyz), kTol));
    // フレーム原点(0,-180,250.5)はMountPlacement側にあり、F_Toolには含まれない
    EXPECT_TRUE(Placement(model, f, "Tool").isApprox(mc::Translation(xyz), kTol));
    const Matrix4d attach =
            mc::RotationAboutLine(Vector3d::UnitX(), Vector3d(0.0, 0.0, 60.0), -a)
            * mc::RotationAboutLine(Vector3d::UnitZ(), Vector3d::Zero(), -c);
    EXPECT_TRUE(Placement(model, f, "Attach").isApprox(attach, kTol));
    EXPECT_TRUE(Placement(model, f, "C").isApprox(attach, kTol));
}

TEST(ForwardKinematicsTest, Fixture_JointsFromNcFlipsWorkSideSign) {
    const mc::MachineModel model = FixtureModel();
    mc::JointVector base = mc::InitialJoints(model);
    base[*model.FindAxis("X")] = 7.0;
    const mc::NcValues nc = {{"A", ToRadians(30.0)}, {"C", ToRadians(45.0)}, {"Y", 10.0}};
    const mc::JointVector q = mc::JointsFromNc(model, nc, base);
    EXPECT_NEAR(q[*model.FindAxis("A")], -ToRadians(30.0), kTol);
    EXPECT_NEAR(q[*model.FindAxis("C")], -ToRadians(45.0), kTol);
    EXPECT_NEAR(q[*model.FindAxis("Y")], 10.0, kTol);
    EXPECT_NEAR(q[*model.FindAxis("X")], 7.0, kTol);   // 未指定軸はbaseの値
    const mc::NcValues back = mc::NcFromJoints(model, q);
    EXPECT_NEAR(back.At("A"), ToRadians(30.0), kTol);
    EXPECT_NEAR(back.At("C"), ToRadians(45.0), kTol);
    EXPECT_NEAR(back.At("X"), 7.0, kTol);
    EXPECT_NEAR(back.At("Z"), 0.0, kTol);
    // 反復順は軸順 (Axes()の順) であり、文字列順ではない
    ASSERT_EQ(back.Size(), model.Axes().size());
    for (std::size_t i = 0; i < model.Axes().size(); ++i) {
        EXPECT_EQ(back.Entries()[i].register_name, model.Axes()[i].register_name);
    }
}

TEST(ForwardKinematicsTest, Fixture_ForwardOverloadsAgree) {
    const mc::MachineModel model = FixtureModel();
    const mc::JointVector q = mc::JointsFromNc(
            model, {{"A", ToRadians(10.0)}, {"X", 3.0}}, mc::InitialJoints(model));
    const std::vector<Matrix4d> returned = mc::Forward(model, q);
    std::vector<Matrix4d> out;
    mc::Forward(model, q, &out);
    const Matrix4d* buffer = out.data();
    mc::Forward(model, q, &out);   // 2回目は再確保しない
    EXPECT_EQ(out.data(), buffer);
    ASSERT_EQ(returned.size(), out.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        EXPECT_TRUE(returned[i].isApprox(out[i], kTol));
    }
}

// ---- 正常系: 退化 ----

TEST(ForwardKinematicsTest, Fixture_ZeroPoseIsIdentity) {
    const mc::MachineModel model = FixtureModel();
    for (const double value : mc::InitialJoints(model).Values()) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }
    for (const Matrix4d& placement : mc::Forward(model, mc::InitialJoints(model))) {
        EXPECT_TRUE(placement.isIdentity(kTol));
    }
}

// ---- 異常系 ----

TEST(ForwardKinematicsTest, Forward_ThrowsInvalidArgumentWhenJointCountMismatches) {
    const mc::MachineModel model = FixtureModel();
    const mc::JointVector too_short(4, 0.0);
    EXPECT_THROW(mc::Forward(model, too_short), std::invalid_argument);
    std::vector<Matrix4d> out;
    EXPECT_THROW(mc::Forward(model, too_short, &out), std::invalid_argument);
    EXPECT_THROW(mc::Forward(model, mc::InitialJoints(model), nullptr),
                 std::invalid_argument);
    EXPECT_THROW(mc::NcFromJoints(model, too_short), std::invalid_argument);
    EXPECT_THROW(mc::JointsFromNc(model, {}, too_short), std::invalid_argument);
    EXPECT_NO_THROW(mc::Forward(model, mc::InitialJoints(model)));
}

TEST(ForwardKinematicsTest, JointsFromNc_ThrowsInvalidArgumentWhenRegisterUnknown) {
    const mc::MachineModel model = FixtureModel();
    EXPECT_THROW(mc::JointsFromNc(model, {{"B", 1.0}}, mc::InitialJoints(model)),
                 std::invalid_argument);
    EXPECT_NO_THROW(mc::JointsFromNc(model, {{"A", 1.0}}, mc::InitialJoints(model)));
}
