/**
 * @file tests/extensions/test_coordinate_frame_group.cpp
 * @brief CoordinateFrameGroup (extensions/inspection) のテスト
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note 対象は CoordinateFrameGroup の以下の振る舞い:
 *       - 正常系 (代表値): コンストラクタの格納・既定値、型/フォーム/次元、
 *         アクセサの往復、GetDefinedBoundingBox の原点・軸先端の包含
 *       - 正常系 (境界値): 空の座標系群でのバウンディングボックス (0次元)
 *       - 正常系 (退化): 形状リビジョンのバンプ有無
 *         (frames/axis_sizeはバンプ、色・点径はバンプしない)
 *
 * TODO: 描画クラス (CoordinateFrameGroupGraphics) はGL文脈を要するため本テストの
 *       対象外 (GUI/手動検証で確認)。
 */
#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_type.h"
#include "igesio/extensions/inspection/coordinate_frame_group.h"

namespace {

namespace inspection = igesio::extensions::inspection;
namespace i_ent = igesio::entities;
using igesio::Vector3d;

/// @brief バウンディングボックス包含判定の許容に用いる微小量
constexpr double kEps = 1e-9;

/// @brief 原点と3軸方向から座標系を1つ作成する
inspection::CoordinateFrame MakeFrame(
        const Vector3d& origin, const Vector3d& x,
        const Vector3d& y, const Vector3d& z) {
    inspection::CoordinateFrame frame;
    frame.origin = origin;
    frame.x_axis = x;
    frame.y_axis = y;
    frame.z_axis = z;
    return frame;
}

/// @brief 軸が単位の標準的な座標系を原点指定で作成する
inspection::CoordinateFrame MakeStandardFrame(const Vector3d& origin) {
    return MakeFrame(origin, Vector3d::UnitX(), Vector3d::UnitY(),
                     Vector3d::UnitZ());
}

/// @brief 2つのベクトルが各成分でtol以内に一致することを検査する
void ExpectVecNear(const Vector3d& a, const Vector3d& b,
                   const double tol = kEps) {
    EXPECT_NEAR(a.x(), b.x(), tol);
    EXPECT_NEAR(a.y(), b.y(), tol);
    EXPECT_NEAR(a.z(), b.z(), tol);
}

/// @brief 2つのRGBA色が各成分で一致することを検査する
void ExpectColorNear(const std::array<float, 4>& a,
                     const std::array<float, 4>& b) {
    for (std::size_t i = 0; i < 4; ++i) EXPECT_NEAR(a[i], b[i], 1e-6f);
}

}  // namespace



/**
 * 正常系 (代表値)
 */

TEST(CoordinateFrameGroup, Constructor_StoresFramesAndDefaults) {
    inspection::CoordinateFrameGroup group(
            {MakeStandardFrame({1.0, 2.0, 3.0}),
             MakeStandardFrame({4.0, 5.0, 6.0})});

    ASSERT_EQ(group.Frames().size(), 2u);
    ExpectVecNear(group.Frames()[0].origin, Vector3d(1.0, 2.0, 3.0));
    ExpectVecNear(group.Frames()[1].origin, Vector3d(4.0, 5.0, 6.0));

    // 既定の表示属性
    EXPECT_NEAR(group.PointSize(), 6.0, kEps);
    EXPECT_NEAR(group.AxisSize(), 1.0, kEps);
    EXPECT_NEAR(group.PointColor()[3], 1.0f, 1e-6f);  // 既定は不透明

    // 非IGESエンティティとしての基本属性
    EXPECT_EQ(group.GetType(), i_ent::EntityType::kNonIges);
    EXPECT_EQ(group.GetFormNumber(), 0);
    EXPECT_EQ(group.NDim(), 3u);
    // コンストラクタは形状リビジョンをバンプしない (初期値0)
    EXPECT_EQ(group.GeometryRevision(), 0u);
}

TEST(CoordinateFrameGroup, Accessors_RoundTrip) {
    inspection::CoordinateFrameGroup group;
    const std::array<float, 4> pc = {0.1f, 0.2f, 0.3f, 0.4f};
    const std::array<float, 4> xc = {0.5f, 0.6f, 0.7f, 0.8f};

    group.SetPointColor(pc);
    group.SetXColor(xc);
    group.SetPointSize(12.0);

    ExpectColorNear(group.PointColor(), pc);
    ExpectColorNear(group.XColor(), xc);
    EXPECT_NEAR(group.PointSize(), 12.0, kEps);
}

TEST(CoordinateFrameGroup, GetDefinedBoundingBox_ContainsOriginsAndAxisTips) {
    inspection::CoordinateFrameGroup group({MakeStandardFrame({1.0, 2.0, 3.0})});
    group.SetAxisSize(2.0);  // 先端は原点 + 軸·2

    const auto bb = group.GetDefinedBoundingBox();
    // 原点と各軸先端 ((3,2,3),(1,4,3),(1,2,5)) を包含する
    EXPECT_TRUE(bb.Contains(Vector3d(1.0, 2.0, 3.0)));
    EXPECT_TRUE(bb.Contains(Vector3d(3.0, 2.0, 3.0)));
    EXPECT_TRUE(bb.Contains(Vector3d(1.0, 4.0, 3.0)));
    EXPECT_TRUE(bb.Contains(Vector3d(1.0, 2.0, 5.0)));
    EXPECT_TRUE(bb.Contains(Vector3d(2.0, 3.0, 4.0)));  // 内部
    EXPECT_EQ(bb.Dimension(), 3u);
    // 領域外の点は包含しない
    EXPECT_FALSE(bb.Contains(Vector3d(0.0, 0.0, 0.0)));
    EXPECT_FALSE(bb.Contains(Vector3d(3.0 + kEps * 1e3, 2.0, 3.0)));
}



/**
 * 正常系 (境界値)
 */

TEST(CoordinateFrameGroup, GetDefinedBoundingBox_EmptyFramesIsZeroDim) {
    const inspection::CoordinateFrameGroup group;
    EXPECT_TRUE(group.GetDefinedBoundingBox().IsEmpty());
}



/**
 * 正常系 (退化): 形状リビジョンのバンプ有無
 */

TEST(CoordinateFrameGroup, SetFrames_BumpsGeometryRevision) {
    inspection::CoordinateFrameGroup group;
    const auto rev0 = group.GeometryRevision();
    group.SetFrames({MakeStandardFrame({0.0, 0.0, 0.0})});
    EXPECT_GT(group.GeometryRevision(), rev0);
}

TEST(CoordinateFrameGroup, AddFrame_BumpsGeometryRevision) {
    inspection::CoordinateFrameGroup group;
    const auto rev0 = group.GeometryRevision();
    group.AddFrame(MakeStandardFrame({0.0, 0.0, 0.0}));
    EXPECT_GT(group.GeometryRevision(), rev0);
}

TEST(CoordinateFrameGroup, SetAxisSize_BumpsGeometryRevision) {
    inspection::CoordinateFrameGroup group({MakeStandardFrame({0.0, 0.0, 0.0})});
    const auto rev0 = group.GeometryRevision();
    group.SetAxisSize(3.0);
    EXPECT_GT(group.GeometryRevision(), rev0);
}

TEST(CoordinateFrameGroup, ColorAndPointSizeSetters_DoNotBumpGeometryRevision) {
    // 色・点径は描画時に参照される属性であり、形状 (テッセレーション) には影響しない
    inspection::CoordinateFrameGroup group({MakeStandardFrame({0.0, 0.0, 0.0})});
    const auto rev0 = group.GeometryRevision();
    group.SetPointColor({0.0f, 0.0f, 0.0f, 1.0f});
    group.SetXColor({0.0f, 0.0f, 0.0f, 1.0f});
    group.SetYColor({0.0f, 0.0f, 0.0f, 1.0f});
    group.SetZColor({0.0f, 0.0f, 0.0f, 1.0f});
    group.SetPointSize(20.0);
    EXPECT_EQ(group.GeometryRevision(), rev0);
}
