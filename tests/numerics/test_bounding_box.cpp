/**
 * @file entities/interfaces/test_bounding_box.cpp
 * @brief entities/interfaces/bounding_box.hのテスト
 * @author Yayoi Habami
 * @date 2025-10-31
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

#include "igesio/numerics/tolerance.h"
#include "igesio/numerics/bounding_box.h"

namespace igesio::numerics {

std::ostream&
operator<<(std::ostream& os, const igesio::numerics::BoundingBox::DirectionType& dt) {
    switch (dt) {
        case igesio::numerics::BoundingBox::DirectionType::kSegment:
            os << "kSegment";
            break;
        case igesio::numerics::BoundingBox::DirectionType::kRay:
            os << "kRay";
            break;
        case igesio::numerics::BoundingBox::DirectionType::kLine:
            os << "kLine";
            break;
    }
    return os;
}

}  // namespace igesio::numerics

namespace {

namespace i_num = igesio::numerics;
using igesio::Vector3d;
using igesio::Matrix3d;
using i_num::BoundingBox;
using DT = BoundingBox::DirectionType;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kTol = 1e-8;

template<typename T>
void ExpectArray3ApproxEqual(
        const std::array<T, 3>& actual, const std::array<T, 3>& expected,
        const double tol = kTol) {
    for (size_t i = 0; i < 3; ++i) {
        if constexpr (std::is_floating_point_v<T>) {
            auto inf = std::numeric_limits<T>::infinity();
            if ((actual[i] == inf && expected[i] == inf) ||
                (actual[i] == -inf && expected[i] == -inf)) {
                continue;  // 両方が無限大または負の無限大なら等しいとみなす
            }
            EXPECT_NEAR(actual[i], expected[i], tol) << " at index " << i
                << ": actual=(" << actual[0] << "," << actual[1] << "," << actual[2]
                << "), expected=(" << expected[0] << "," << expected[1] << ","
                << expected[2] << ")";
        } else if constexpr (std::is_same_v<T, Vector3d>) {
            // Vector3dの場合
            EXPECT_TRUE(i_num::IsApproxEqual(actual[i], expected[i], tol));
        } else {
            EXPECT_EQ(actual[i], expected[i]) << " at index " << i
                << ": actual=" << actual[i] << ", expected=" << expected[i];
        }
    }
}

void ExpectInf(const double value, const bool is_positive = true) {
    if (is_positive) {
        EXPECT_EQ(value, kInf);
    } else {
        EXPECT_EQ(value, -kInf);
    }
}

bool ContainsVertex(const std::vector<Vector3d>& vertices, const Vector3d& vertex) {
    for (const auto& v : vertices) {
        if (i_num::IsApproxEqual(v, vertex)) {
            return true;
        }
    }
    return false;
}

// bbox生成の補助関数
std::tuple<BoundingBox, Vector3d, std::array<Vector3d, 3>, std::array<double, 3>>
MakeBBox3DRotatedByZ() {
    Vector3d p0(1.0, 2.0, 3.0);
    auto rot = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    std::array<Vector3d, 3> dirs = {
        rot * Vector3d::UnitX(),
        rot * Vector3d::UnitY(),
        Vector3d::UnitZ()
    };
    std::array<double, 3> sizes = {10.0, 20.0, 30.0};

    return std::make_tuple(BoundingBox(p0, dirs, sizes), p0, dirs, sizes);
}

BoundingBox MakeBox2D(
    const Vector3d& p0,
    const std::array<Vector3d, 2>& dirs,
    const std::array<double, 2>& sizes) {
    return BoundingBox(p0, dirs, sizes);
}

}  // namespace



/**
 * コンストラクタのテスト
 */

TEST(BoundingBoxTest, DefaultConstructor) {
    BoundingBox bbox;

    EXPECT_TRUE(i_num::IsApproxEqual(bbox.GetControl(), Vector3d::Zero()));

    auto sizes = bbox.GetSizes();
    ExpectArray3ApproxEqual(sizes, {0.0, 0.0, 0.0});

    // デフォルトのdirectionsが軸平行であることを確認
    auto dirs = bbox.GetDirections();
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[0], Vector3d::UnitX()));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[1], Vector3d::UnitY()));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[2], Vector3d::UnitZ()));

    auto types = bbox.GetDirectionTypes();
    ExpectArray3ApproxEqual(
            types, {DT::kSegment, DT::kSegment, DT::kSegment});
}

TEST(BoundingBoxTest, Constructor3DGeneral_Valid) {

    auto [box, p0, dirs, sizes] = MakeBBox3DRotatedByZ();

    EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p0));
    auto box_dirs = box.GetDirections();
    EXPECT_TRUE(i_num::IsApproxEqual(box_dirs[0], dirs[0]));
    EXPECT_TRUE(i_num::IsApproxEqual(box_dirs[1], dirs[1]));
    EXPECT_TRUE(i_num::IsApproxEqual(box_dirs[2], dirs[2]));
    ExpectArray3ApproxEqual(box.GetSizes(), sizes);
    ExpectArray3ApproxEqual(
        box.GetDirectionTypes(),
        {DT::kSegment, DT::kSegment, DT::kSegment});
}

TEST(BoundingBoxTest, Constructor3DGeneral_InvalidDirections) {
    auto [box, p0, dirs, sizes] = MakeBBox3DRotatedByZ();

    // 単位ベクトルでない
    auto non_unit = dirs;
    non_unit[0] *= 2.0;
    EXPECT_THROW(BoundingBox(p0, non_unit, sizes), std::invalid_argument);

    // 直交しない
    auto non_ortho = dirs;
    non_ortho[1] = dirs[0];
    EXPECT_THROW(BoundingBox(p0, non_ortho, sizes), std::invalid_argument);

    // 右手系でない
    auto left_hand = dirs;
    left_hand[2] = -left_hand[2];
    EXPECT_THROW(BoundingBox(p0, left_hand, sizes), std::invalid_argument);
}

TEST(BoundingBoxTest, Constructor3DGeneral_InvalidSizes) {
    auto [box, p0, dirs, sizes] = MakeBBox3DRotatedByZ();

    // s0 = 0
    auto invalid_sizes = sizes;
    invalid_sizes[0] = 0.0;
    EXPECT_THROW(BoundingBox(p0, dirs, invalid_sizes), std::invalid_argument);

    // s1 = 0
    invalid_sizes = sizes;
    invalid_sizes[1] = 0.0;
    EXPECT_THROW(BoundingBox(p0, dirs, invalid_sizes), std::invalid_argument);

    // s2 = 0 は許可される
    invalid_sizes = sizes;
    invalid_sizes[2] = 0.0;
    EXPECT_NO_THROW(BoundingBox(p0, dirs, invalid_sizes));
}

// AABB (軸平行ボックス) のコンストラクタテスト
TEST(BoundingBoxTest, Constructor3DAABB_Valid) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes = {10.0, 20.0, 30.0};
    auto box = BoundingBox(p0, sizes);

    EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p0));
    auto dirs = box.GetDirections();
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[0], Vector3d::UnitX()));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[1], Vector3d::UnitY()));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[2], Vector3d::UnitZ()));
    ExpectArray3ApproxEqual(box.GetSizes(), sizes);
}

TEST(BoundingBoxTest, Constructor3DAABB_InvalidSizes) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes = {10.0, 20.0, 30.0};

    // 負の値
    auto invalid_sizes = sizes;
    invalid_sizes[0] = -1.0;
    EXPECT_THROW(BoundingBox(p0, invalid_sizes), std::invalid_argument);

    // s0 = 0
    invalid_sizes = sizes;
    invalid_sizes[0] = 0.0;
    EXPECT_THROW(BoundingBox(p0, invalid_sizes), std::invalid_argument);
}

TEST(BoundingBoxTest, Constructor2DGeneral_Valid) {
    Vector3d p0(1.0, 2.0, 3.0);
    auto rot = igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitZ());
    std::array<Vector3d, 2> dirs_2d = {
        rot * Vector3d::UnitX(),
        rot * Vector3d::UnitY()
    };
    std::array<double, 2> sizes_2d = {15.0, 25.0};

    BoundingBox box(p0, dirs_2d, sizes_2d);

    EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p0));

    auto dirs = box.GetDirections();
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[0], dirs_2d[0]));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[1], dirs_2d[1]));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[2], dirs_2d[0].cross(dirs_2d[1])));  // D2 = D0 x D1

    auto sizes = box.GetSizes();
    ExpectArray3ApproxEqual(sizes, {sizes_2d[0], sizes_2d[1], 0.0});  // s2 = 0
}

TEST(BoundingBoxTest, Constructor2DGeneral_InvalidSizes) {
    Vector3d p0(1.0, 2.0, 3.0);
    auto rot = igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitZ());
    std::array<Vector3d, 2> dirs_2d = {
        rot * Vector3d::UnitX(),
        rot * Vector3d::UnitY()
    };
    std::array<double, 2> sizes_2d = {15.0, 25.0};

    auto invalid_sizes = sizes_2d;

    // s0 = 0
    invalid_sizes[0] = 0.0;
    EXPECT_THROW(BoundingBox(p0, dirs_2d, invalid_sizes), std::invalid_argument);

    // s1 = -1
    invalid_sizes = sizes_2d;
    invalid_sizes[1] = -1.0;
    EXPECT_THROW(BoundingBox(p0, dirs_2d, invalid_sizes), std::invalid_argument);
}

TEST(BoundingBoxTest, Constructor2DAABB_Valid) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 2> sizes_2d = {15.0, 25.0};

    BoundingBox box(p0, sizes_2d);

    EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p0));
    EXPECT_TRUE(i_num::IsApproxEqual(box.GetDirections()[0], Vector3d::UnitX()));
    auto sizes = box.GetSizes();
    ExpectArray3ApproxEqual(sizes, {sizes_2d[0], sizes_2d[1], 0.0});
}

TEST(BoundingBoxTest, Constructor_InfiniteTypes) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<Vector3d, 3> dirs_aabb = {
        Vector3d::UnitX(),
        Vector3d::UnitY(),
        Vector3d::UnitZ()
    };
    std::array<double, 3> sizes_inf = {10.0, kInf, kInf};
    std::array<bool, 3> is_line_inf = {false, false, true};

    BoundingBox box(p0, dirs_aabb, sizes_inf, is_line_inf);

    auto types = box.GetDirectionTypes();
    ExpectArray3ApproxEqual(types, {DT::kSegment, DT::kRay, DT::kLine});

    // GetSizes() は kRay も kLine も +∞ を返す
    auto retrieved_sizes = box.GetSizes();
    ExpectArray3ApproxEqual(retrieved_sizes, {10.0, kInf, kInf});
}

TEST(BoundingBoxTest, ConstructorFromTwoPoints_Valid) {
    // 3Dケース
    {
        SCOPED_TRACE("3D case");
        Vector3d p1(0, 0, 0);
        Vector3d p2(10, 20, 30);
        BoundingBox box(p1, p2);

        EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p1));
        ExpectArray3ApproxEqual(box.GetSizes(), {10.0, 20.0, 30.0});
        auto dirs = box.GetDirections();
        ExpectArray3ApproxEqual(dirs, {Vector3d::UnitX(), Vector3d::UnitY(), Vector3d::UnitZ()});
        EXPECT_TRUE(box.Is3D());
    }

    // 3Dケース (point1 > point2)
    {
        SCOPED_TRACE("3D case, reversed points");
        Vector3d p1(10, 20, 30);
        Vector3d p2(0, 0, 0);
        BoundingBox box(p1, p2);

        EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p2));
        ExpectArray3ApproxEqual(box.GetSizes(), {10.0, 20.0, 30.0});
        auto dirs = box.GetDirections();
        ExpectArray3ApproxEqual(dirs, {Vector3d::UnitX(), Vector3d::UnitY(), Vector3d::UnitZ()});
    }

    // 2Dケース (zが同じ)
    {
        SCOPED_TRACE("2D case, z-plane");
        Vector3d p1(1, 2, 5);
        Vector3d p2(11, 12, 5);
        BoundingBox box(p1, p2);

        EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p1));
        ExpectArray3ApproxEqual(box.GetSizes(), {10.0, 10.0, 0.0});
        auto dirs = box.GetDirections();
        ExpectArray3ApproxEqual(dirs, {Vector3d::UnitX(), Vector3d::UnitY(), Vector3d::UnitZ()});
        EXPECT_TRUE(box.Is2D());
    }

    // 2Dケース (yが同じ)
    {
        SCOPED_TRACE("2D case, y-plane");
        Vector3d p1(1, 5, 2);
        Vector3d p2(11, 5, 12);
        BoundingBox box(p1, p2);

        EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p1));
        ExpectArray3ApproxEqual(box.GetSizes(), {10.0, 10.0, 0.0});
        auto dirs = box.GetDirections();
        // D0=z, D1=x, D2=y
        ExpectArray3ApproxEqual(dirs, {Vector3d::UnitZ(), Vector3d::UnitX(), Vector3d::UnitY()});
        EXPECT_TRUE(box.Is2D());
    }

    // 2Dケース (xが同じ)
    {
        SCOPED_TRACE("2D case, x-plane");
        Vector3d p1(5, 1, 2);
        Vector3d p2(5, 11, 12);
        BoundingBox box(p1, p2);

        EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), p1));
        ExpectArray3ApproxEqual(box.GetSizes(), {10.0, 10.0, 0.0});
        auto dirs = box.GetDirections();
        // D0=y, D1=z, D2=x
        ExpectArray3ApproxEqual(dirs, {Vector3d::UnitY(), Vector3d::UnitZ(), Vector3d::UnitX()});
        EXPECT_TRUE(box.Is2D());
    }
}

TEST(BoundingBoxTest, ConstructorFromTwoPoints_Invalid) {
    // point1とpoint2が同じ
    EXPECT_THROW(BoundingBox(Vector3d(1, 2, 3), Vector3d(1, 2, 3)), std::invalid_argument);

    // 2つ以上の座標値が同じ (1Dになるケース)
    EXPECT_THROW(BoundingBox(Vector3d(1, 2, 3), Vector3d(1, 2, 4)), std::invalid_argument);
    EXPECT_THROW(BoundingBox(Vector3d(1, 2, 3), Vector3d(1, 4, 3)), std::invalid_argument);
    EXPECT_THROW(BoundingBox(Vector3d(1, 2, 3), Vector3d(4, 2, 3)), std::invalid_argument);

    // 無限大の成分を持つ
    EXPECT_THROW(BoundingBox(Vector3d(kInf, 2, 3), Vector3d(4, 5, 6)), std::invalid_argument);
    EXPECT_THROW(BoundingBox(Vector3d(1, 2, 3), Vector3d(4, -kInf, 6)), std::invalid_argument);
    EXPECT_THROW(BoundingBox(Vector3d(1, 2, kInf), Vector3d(4, 5, -kInf)), std::invalid_argument);
}



/**
 * ゲッター・セッターのテスト
 */

TEST(BoundingBoxTest, GetSetControl) {
    BoundingBox box;
    Vector3d new_p0(100, 200, 300);

    box.SetControl(new_p0);
    EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), new_p0));

    // 無限大の成分を持つ場合は例外
    Vector3d inf_p0(kInf, 0, 0);
    EXPECT_THROW(box.SetControl(inf_p0), std::invalid_argument);
}

TEST(BoundingBoxTest, GetSetDirections) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes = {10.0, 20.0, 30.0};
    BoundingBox box(p0, sizes);  // AABBで初期化

    auto rot = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    std::array<Vector3d, 3> dirs_rotated = {
        rot * Vector3d::UnitX(),
        rot * Vector3d::UnitY(),
        Vector3d::UnitZ()
    };

    // 有効な回転した方向を設定
    EXPECT_NO_THROW(box.SetDirections(dirs_rotated));
    auto dirs = box.GetDirections();
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[0], dirs_rotated[0]));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[1], dirs_rotated[1]));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[2], dirs_rotated[2]));

    // 無効な方向（直交しない）を設定
    auto non_ortho = dirs_rotated;
    non_ortho[1] = non_ortho[0];
    EXPECT_THROW(box.SetDirections(non_ortho), std::invalid_argument);
}

TEST(BoundingBoxTest, SetDirections2D) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes = {10.0, 20.0, 30.0};
    BoundingBox box(p0, sizes);  // 3Dボックスで初期化

    auto rot = igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitZ());
    std::array<Vector3d, 2> dirs_2d_rotated = {
        rot * Vector3d::UnitX(),
        rot * Vector3d::UnitY()
    };

    EXPECT_NO_THROW(box.SetDirections2D(dirs_2d_rotated));

    auto dirs = box.GetDirections();
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[0], dirs_2d_rotated[0]));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[1], dirs_2d_rotated[1]));
    EXPECT_TRUE(i_num::IsApproxEqual(dirs[2], dirs_2d_rotated[0].cross(dirs_2d_rotated[1])));

    // s2 が 0 に設定されることを確認
    auto box_sizes = box.GetSizes();
    EXPECT_NEAR(box_sizes[2], 0.0, kTol);
    // s0, s1 は維持される
    EXPECT_NEAR(box_sizes[0], sizes[0], kTol);
    EXPECT_NEAR(box_sizes[1], sizes[1], kTol);

    // 無効な方向（直交しない）を設定
    auto non_ortho_2d = dirs_2d_rotated;
    non_ortho_2d[1] = non_ortho_2d[0];
    EXPECT_THROW(box.SetDirections2D(non_ortho_2d), std::invalid_argument);
}

TEST(BoundingBoxTest, GetSetSizes) {
    BoundingBox box;
    std::array<double, 3> sizes_3d = {10.0, 20.0, 30.0};

    // 3D Segment
    EXPECT_NO_THROW(box.SetSizes(sizes_3d));
    auto sizes = box.GetSizes();
    ExpectArray3ApproxEqual(sizes, sizes_3d);
    auto types = box.GetDirectionTypes();
    ExpectArray3ApproxEqual(types, {DT::kSegment, DT::kSegment, DT::kSegment});

    // 3D Infinite (Ray, Line)
    std::array<double, 3> sizes_inf = {10.0, kInf, kInf};
    std::array<bool, 3> is_line_inf = {false, false, true};
    EXPECT_NO_THROW(box.SetSizes(sizes_inf, is_line_inf));

    sizes = box.GetSizes();
    ExpectArray3ApproxEqual(sizes, {sizes_inf[0], kInf, kInf});  // GetSizesはkLineも+Inf

    types = box.GetDirectionTypes();
    ExpectArray3ApproxEqual(types, {DT::kSegment, DT::kRay, DT::kLine});

    // 無効なサイズ (s0 = 0)
    auto invalid_sizes = sizes_3d;
    invalid_sizes[0] = 0.0;
    EXPECT_THROW(box.SetSizes(invalid_sizes), std::invalid_argument);

    // 無効なサイズ (s1 < 0)
    invalid_sizes = sizes_3d;
    invalid_sizes[1] = -1.0;
    EXPECT_THROW(box.SetSizes(invalid_sizes), std::invalid_argument);
}

TEST(BoundingBoxTest, SetSizes2D) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes_3d = {10.0, 20.0, 30.0};
    BoundingBox box(p0, sizes_3d);  // 3Dで初期化

    // 2D Segment
    std::array<double, 2> sizes_2d = {15.0, 25.0};
    EXPECT_NO_THROW(box.SetSizes2D(sizes_2d));
    auto sizes = box.GetSizes();
    ExpectArray3ApproxEqual(sizes, {sizes_2d[0], sizes_2d[1], 0.0});
    auto types = box.GetDirectionTypes();
    ExpectArray3ApproxEqual(types, {DT::kSegment, DT::kSegment, DT::kSegment});

    // 2D Infinite (Ray, Line)
    std::array<double, 2> sizes_inf_2d = {kInf, 10.0};
    std::array<bool, 2> is_line_2d = {true, false};  // {Line, Segment}

    EXPECT_NO_THROW(box.SetSizes2D(sizes_inf_2d, is_line_2d));
    sizes = box.GetSizes();
    ExpectArray3ApproxEqual(sizes, {kInf, 10.0, 0.0});  // GetSizesはkLineも+Inf

    types = box.GetDirectionTypes();
    ExpectArray3ApproxEqual(types, {DT::kLine, DT::kSegment, DT::kSegment});

    // 無効なサイズ (s0 = 0)
    auto invalid_sizes = sizes_2d;
    invalid_sizes[0] = 0.0;
    EXPECT_THROW(box.SetSizes2D(invalid_sizes), std::invalid_argument);
}

TEST(BoundingBoxTest, SetSize) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes_3d = {10.0, 20.0, 30.0};
    BoundingBox box(p0, sizes_3d);

    // s0 を Ray に変更
    EXPECT_NO_THROW(box.SetSize(0, kInf, false));
    ExpectInf(box.GetSizes()[0]);
    EXPECT_EQ(box.GetDirectionTypes()[0], DT::kRay);
    EXPECT_NEAR(box.GetSizes()[1], sizes_3d[1], kTol);  // 他は影響を受けない

    // s1 を Line に変更
    EXPECT_NO_THROW(box.SetSize(1, 123.45, true));  // サイズ値はkLineの場合無視される
    ExpectInf(box.GetSizes()[1]);  // GetSizesはkLineも+Inf
    EXPECT_EQ(box.GetDirectionTypes()[1], DT::kLine);

    // s1 を Segment に戻す
    EXPECT_NO_THROW(box.SetSize(1, 50.0, false));
    EXPECT_NEAR(box.GetSizes()[1], 50.0, kTol);
    EXPECT_EQ(box.GetDirectionTypes()[1], DT::kSegment);

    // s2 を 0 に設定 (許可)
    EXPECT_NO_THROW(box.SetSize(2, 0.0, false));
    EXPECT_NEAR(box.GetSizes()[2], 0.0, kTol);

    // --- 異常系 ---
    // i が範囲外
    EXPECT_THROW(box.SetSize(3, 10.0, false), std::out_of_range);

    // s0 = 0
    EXPECT_THROW(box.SetSize(0, 0.0, false), std::invalid_argument);

    // s1 = 0
    EXPECT_THROW(box.SetSize(1, 0.0, false), std::invalid_argument);

    // s0 < 0
    EXPECT_THROW(box.SetSize(0, -1.0, false), std::invalid_argument);
}

TEST(BoundingBoxTest, GetDirectionTypes) {
    // この関数は他のテスト (コンストラクタ, SetSizes) で十分にテストされていますが、
    // 明示的な確認テストを追加します。
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<Vector3d, 3> dirs_aabb = {
        Vector3d::UnitX(),
        Vector3d::UnitY(),
        Vector3d::UnitZ()
    };

    std::array<double, 3> sizes = {10.0, kInf, 10.0};
    std::array<bool, 3> is_line = {false, false, true};  // {Seg, Ray, Line}

    BoundingBox box(p0, dirs_aabb, sizes, is_line);

    auto types = box.GetDirectionTypes();
    EXPECT_EQ(types[0], DT::kSegment);
    EXPECT_EQ(types[1], DT::kRay);
    EXPECT_EQ(types[2], DT::kLine);

    // すべて Line
    std::array<bool, 3> all_line = {true, true, true};
    box.SetSizes(sizes, all_line);  // サイズ値は +Inf でなくても kLine になるはず
    types = box.GetDirectionTypes();
    EXPECT_EQ(types[0], DT::kLine);
    EXPECT_EQ(types[1], DT::kLine);
    EXPECT_EQ(types[2], DT::kLine);
}



/**
 * 変換系関数のテスト
 */

TEST(BoundingBoxTest, Translate) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes = {10.0, 20.0, 30.0};
    BoundingBox box(p0, sizes); // AABB
    auto original_dirs = box.GetDirections();
    auto original_sizes = box.GetSizes();

    Vector3d vec(100.0, -50.0, 10.0);
    EXPECT_NO_THROW(box.Translate(vec));

    Vector3d expected_p0 = p0 + vec;
    EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), expected_p0));

    // 方向とサイズは変わらない
    auto dirs = box.GetDirections();
    ExpectArray3ApproxEqual(dirs, original_dirs);
    auto new_sizes = box.GetSizes();
    ExpectArray3ApproxEqual(new_sizes, original_sizes);

    // 無限大のベクトルで例外
    Vector3d inf_vec(0, kInf, 0);
    EXPECT_THROW(box.Translate(inf_vec), std::invalid_argument);
}

TEST(BoundingBoxTest, Rotate) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes = {10.0, 20.0, 30.0};
    BoundingBox box(p0, sizes); // AABB
    auto original_sizes = box.GetSizes();

    // Z軸周りに90度回転
    auto rot = igesio::AngleAxisd(igesio::kPi / 2.0, Vector3d::UnitZ());

    EXPECT_NO_THROW(box.Rotate(rot));

    std::array<Vector3d, 3> expected_dirs = {
        Vector3d::UnitY(),   // UnitX が回転
        -Vector3d::UnitX(),  // UnitY が回転
        Vector3d::UnitZ()    // UnitZ はそのまま
    };
    auto dirs = box.GetDirections();
    ExpectArray3ApproxEqual(dirs, expected_dirs);

    // 基準点とサイズは変わらないことを確認
    auto new_p0 = box.GetControl();
    EXPECT_TRUE(i_num::IsApproxEqual(new_p0, p0));
    auto new_sizes = box.GetSizes();
    ExpectArray3ApproxEqual(new_sizes, original_sizes);

    // 非直交行列で例外
    Matrix3d non_ortho = Matrix3d::Identity() * 2.0;
    EXPECT_THROW(box.Rotate(non_ortho), std::invalid_argument);
}

TEST(BoundingBoxTest, Transform) {
    Vector3d p0(1.0, 2.0, 3.0);
    std::array<double, 3> sizes = {10.0, 20.0, 30.0};
    BoundingBox box(p0, sizes);  // AABB
    auto original_sizes = box.GetSizes();

    // Z軸周りに90度回転
    auto rot = igesio::AngleAxisd(igesio::kPi / 2.0, Vector3d::UnitZ());
    Vector3d vec(100.0, -50.0, 10.0);

    EXPECT_NO_THROW(box.Transform(rot, vec));

    Vector3d expected_p0 = p0 + vec;
    std::array<Vector3d, 3> expected_dirs = {
        Vector3d::UnitY(),
        -Vector3d::UnitX(),
        Vector3d::UnitZ()
    };

    EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), expected_p0));
    auto dirs = box.GetDirections();
    ExpectArray3ApproxEqual(dirs, expected_dirs);

    // サイズは変わらない
    auto new_sizes = box.GetSizes();
    ExpectArray3ApproxEqual(new_sizes, original_sizes);

    // 異常系
    Matrix3d non_ortho = Matrix3d::Identity() * 2.0;
    Vector3d inf_vec(0, kInf, 0);
    EXPECT_THROW(box.Transform(non_ortho, vec), std::invalid_argument);
    EXPECT_THROW(box.Transform(rot, inf_vec), std::invalid_argument);
}

// 拡張
TEST(BoundingBoxTest, ExpandToInclude) {
    std::string description = "Box: X:[10,15], Y:[10,20], Z:[10,30]";
    Vector3d p0(10.0, 10.0, 10.0);
    std::array<double, 3> sizes = {5.0, 10.0, 20.0};
    BoundingBox box(p0, sizes);

    struct Case {
        std::string description;
        Vector3d other_p0;
        std::array<double, 3> other_sizes;
        Vector3d expected_p0;
        std::array<double, 3> expected_sizes;
    };
    std::vector<Case> cases = {
        { "BがAに内包される",
          Vector3d(11.0, 11.0, 11.0), {1.0, 1.0, 1.0},
          Vector3d(10.0, 10.0, 10.0), {5.0, 10.0, 20.0} },
        { "BがAを正方向(+s_i)に拡大する",
          Vector3d(12.0, 12.0, 12.0), {10.0, 10.0, 20.0},
          Vector3d(10.0, 10.0, 10.0), {12.0, 12.0, 22.0} },
        { "BがAを負方向(-D_i)に拡大する",
          Vector3d(5.0, 5.0, 5.0), {10.0, 10.0, 10.0},
          Vector3d(5.0, 5.0, 5.0), {10.0, 15.0, 25.0} },
        { "BがAを両方向に拡大する (AがBに内包される)",
          Vector3d(0.0, 0.0, 0.0), {100.0, 100.0, 100.0},
          Vector3d(0.0, 0.0, 0.0), {100.0, 100.0, 100.0} },
        { "BがAと離れている",
          Vector3d(0.0, 0.0, 0.0), {1.0, 1.0, 1.0},
          Vector3d(0.0, 0.0, 0.0), {15.0, 20.0, 30.0} },
        { "Bが一部の軸でのみ拡大 (X方向のみ)",
          Vector3d(8.0, 12.0, 15.0), {10.0, 5.0, 10.0},
          Vector3d(8.0, 10.0, 10.0), {10.0, 10.0, 20.0} },
        { "Bが2D (s2=0)、Aを拡大",
          Vector3d(5.0, 5.0, 10.0), {15.0, 20.0, 0.0},
          Vector3d(5.0, 5.0, 10.0), {15.0, 20.0, 20.0} },
        { "Bが境界に接触 (正方向)",
          Vector3d(15.0, 20.0, 30.0), {5.0, 5.0, 5.0},
          Vector3d(10.0, 10.0, 10.0), {10.0, 15.0, 25.0} },
        { "Bが境界に接触 (負方向)",
          Vector3d(5.0, 5.0, 5.0), {5.0, 5.0, 5.0},
          Vector3d(5.0, 5.0, 5.0), {10.0, 15.0, 25.0} }
    };

    // AABB
    for (const auto& c : cases) {
        SCOPED_TRACE(description + "; " + c.description);

        // boxをリセット
        BoundingBox test_box(p0, sizes);
        BoundingBox other_box(c.other_p0, c.other_sizes);

        EXPECT_NO_THROW(test_box.ExpandToInclude(other_box));

        EXPECT_TRUE(i_num::IsApproxEqual(test_box.GetControl(), c.expected_p0))
            << "Expected p0: " << c.expected_p0.transpose()
            << ", Got: " << test_box.GetControl().transpose();

        auto result_sizes = test_box.GetSizes();
        ExpectArray3ApproxEqual(result_sizes, c.expected_sizes);

        // 拡大後のboxは両方のboxを包含する
        EXPECT_TRUE(test_box.Contains(other_box))
            << "Expanded box should contain other_box: other_p0="
            << other_box.GetControl().transpose() << ", sizes=("
            << other_box.GetSizes()[0] << "," << other_box.GetSizes()[1]
            << "," << other_box.GetSizes()[2] << ")";
        EXPECT_TRUE(test_box.Contains(BoundingBox(p0, sizes)))
            << "Expanded box should contain original box: p0="
            << test_box.GetControl().transpose() << ", sizes=("
            << test_box.GetSizes()[0] << "," << test_box.GetSizes()[1] << ","
            << test_box.GetSizes()[2] << ")";
    }

    // 回転したboxについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    for (const auto& c : cases) {
        SCOPED_TRACE("回転後: " + description + "; " + c.description);

        // boxをリセット
        BoundingBox test_box(p0, sizes);
        BoundingBox other_box(c.other_p0, c.other_sizes);
        test_box.Rotate(rotation, Vector3d::Zero());
        other_box.Rotate(rotation, Vector3d::Zero());

        EXPECT_NO_THROW(test_box.ExpandToInclude(other_box));

        // 拡大後のboxの位置・サイズを検証
        auto expected_p0 = rotation * c.expected_p0;
        EXPECT_TRUE(i_num::IsApproxEqual(test_box.GetControl(), expected_p0))
            << "Expected p0: " << expected_p0.transpose()
            << ", Got: " << test_box.GetControl().transpose();
        auto result_sizes = test_box.GetSizes();
        ExpectArray3ApproxEqual(result_sizes, c.expected_sizes);

        // 拡大後のboxは両方のboxを包含する
        EXPECT_TRUE(test_box.Contains(other_box))
            << "Expanded box should contain other_box: other_p0="
            << other_box.GetControl().transpose() << ", sizes=("
            << other_box.GetSizes()[0] << "," << other_box.GetSizes()[1]
            << "," << other_box.GetSizes()[2] << ")";
        auto original_box_rotated = BoundingBox(p0, sizes);
        original_box_rotated.Rotate(rotation, Vector3d::Zero());
        EXPECT_TRUE(test_box.Contains(original_box_rotated))
            << "Expanded box should contain original box: p0="
            << test_box.GetControl().transpose() << ", sizes=("
            << test_box.GetSizes()[0] << "," << test_box.GetSizes()[1] << ","
            << test_box.GetSizes()[2] << ")";
    }
}

// サイズがゼロのバウンディングボックスの/による拡張
TEST(BoundingBoxTest, ExpandToInclude_ZeroSizeBox) {
    // サイズがゼロのボックス2つ
    {
        auto box1 = BoundingBox();
        auto box2 = BoundingBox();
        EXPECT_NO_THROW(box1.ExpandToInclude(box2));
        EXPECT_TRUE(box1.IsEmpty());
        EXPECT_TRUE(i_num::IsApproxEqual(box1.GetControl(), Vector3d::Zero()));

        // p_{0,1} = (0,0,0), p_{0,2} = (1,1,1) -> サイズが(1,1,1)のボックスに拡大
        box2.SetControl(Vector3d(1.0, 1.0, 1.0));
        EXPECT_NO_THROW(box1.ExpandToInclude(box2));
        EXPECT_FALSE(box1.IsEmpty());
        EXPECT_TRUE(i_num::IsApproxEqual(box1.GetControl(), Vector3d::Zero()));
        ExpectArray3ApproxEqual(box1.GetSizes(), {1.0, 1.0, 1.0});
    }

    // 通常のボックスをサイズがゼロのボックスで拡張
    {
        // サイズが非ゼロのボックス内にゼロサイズボックスがある場合
        auto null_box = BoundingBox();
        auto sizes = std::array<double, 3>{10.0, 20.0, 30.0};
        auto box = BoundingBox(Vector3d::Zero(), sizes);
        EXPECT_NO_THROW(box.ExpandToInclude(null_box));
        EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), Vector3d::Zero()));
        ExpectArray3ApproxEqual(box.GetSizes(), sizes);

        // サイズが非ゼロのボックス外にゼロサイズボックスがある場合
        auto new_control = Vector3d(-1, -1, -1);
        null_box.SetControl(new_control);
        EXPECT_NO_THROW(box.ExpandToInclude(null_box));
        EXPECT_TRUE(i_num::IsApproxEqual(box.GetControl(), new_control));
        ExpectArray3ApproxEqual(box.GetSizes(), std::array<double, 3>{11.0, 21.0, 31.0});
    }

    // ゼロサイズボックスを通常のボックスで拡張
    {
        auto null_box = BoundingBox();
        auto sizes = std::array<double, 3>{10.0, 20.0, 30.0};
        auto box = BoundingBox(Vector3d(5.0, 5.0, 5.0), sizes);
        EXPECT_NO_THROW(null_box.ExpandToInclude(box));
        EXPECT_TRUE(i_num::IsApproxEqual(null_box.GetControl(), Vector3d::Zero()));
        ExpectArray3ApproxEqual(null_box.GetSizes(),
                                std::array<double, 3>{15.0, 25.0, 35.0});
    }
}



/**
 * 状態取得関数のテスト
 */

TEST(BoundingBoxTest, IsEmpty) {
    BoundingBox box_empty;
    EXPECT_TRUE(box_empty.IsEmpty());

    Vector3d p0(1.0, 2.0, 3.0);
    BoundingBox box_3d(p0, std::array<double, 3>{10.0, 20.0, 30.0});
    EXPECT_FALSE(box_3d.IsEmpty());

    BoundingBox box_2d(p0, std::array<double, 2>{10.0, 20.0});
    EXPECT_FALSE(box_2d.IsEmpty());

    BoundingBox box_ray(p0, {kInf, 20.0, 30.0}, {false, false, false});
    EXPECT_FALSE(box_ray.IsEmpty());

    // サイズをセットし直すと Empty ではなくなる
    box_empty.SetSizes({1.0, 1.0, 1.0});
    EXPECT_FALSE(box_empty.IsEmpty());
}

TEST(BoundingBoxTest, Is2DIs3D) {
    Vector3d p0(1.0, 2.0, 3.0);

    // Empty (s0=s1=s2=0) は 2D 扱い
    BoundingBox box_empty;
    EXPECT_TRUE(box_empty.Is2D());
    EXPECT_FALSE(box_empty.Is3D());

    // 3D (s2 > 0)
    BoundingBox box_3d(p0, std::array<double, 3>{10.0, 20.0, 30.0});
    EXPECT_FALSE(box_3d.Is2D());
    EXPECT_TRUE(box_3d.Is3D());

    // 2D (s2 = 0)
    BoundingBox box_2d_s3(p0, std::array<double, 3>{10.0, 20.0, 0.0});
    EXPECT_TRUE(box_2d_s3.Is2D());
    EXPECT_FALSE(box_2d_s3.Is3D());

    // 2D (コンストラクタ)
    BoundingBox box_2d(p0, std::array<double, 2>{10.0, 20.0});
    EXPECT_TRUE(box_2d.Is2D());
    EXPECT_FALSE(box_2d.Is3D());

    // 3D 無限 (s2 = +Inf)
    BoundingBox box_inf_3d(p0, std::array<double, 3>{10.0, 20.0, kInf});
    EXPECT_FALSE(box_inf_3d.Is2D());
    EXPECT_TRUE(box_inf_3d.Is3D());

    // 2D 無限 (s0 = +Inf, s2 = 0)
    box_inf_3d.SetSize(0, kInf, false);  // s0 = Ray
    box_inf_3d.SetSize(2, 0.0, false);   // s2 = 0
    EXPECT_TRUE(box_inf_3d.Is2D());
    EXPECT_FALSE(box_inf_3d.Is3D());

    // 3D -> 2D
    box_3d.SetSize(2, 0.0, false);
    EXPECT_TRUE(box_3d.Is2D());

    // 2D -> 3D
    box_2d.SetSize(2, 10.0, false);
    EXPECT_FALSE(box_2d.Is2D());
}

TEST(BoundingBoxTest, IsOnZPlane) {
    // Z平面上の2Dボックス (p0.z=0, d2//UnitZ)
    Vector3d p0_on_z(1.0, 2.0, 0.0);
    std::array<double, 2> sizes_2d = {10.0, 20.0};
    BoundingBox box_on_z(p0_on_z, sizes_2d);
    EXPECT_TRUE(box_on_z.Is2D());
    EXPECT_TRUE(box_on_z.IsOnZPlane());

    // Z平面上の2Dボックス (基準点が原点)
    BoundingBox box_origin(Vector3d::Zero(), sizes_2d);
    EXPECT_TRUE(box_origin.IsOnZPlane());

    // 3Dボックス (s2 > 0)
    Vector3d p0_3d(1.0, 2.0, 0.0);
    std::array<double, 3> sizes_3d = {10.0, 20.0, 30.0};
    BoundingBox box_3d(p0_3d, sizes_3d);
    EXPECT_FALSE(box_3d.Is2D());
    EXPECT_FALSE(box_3d.IsOnZPlane());

    // 2DだがZ平面からずれている (p0.z != 0)
    Vector3d p0_offset(1.0, 2.0, 5.0);
    BoundingBox box_offset(p0_offset, sizes_2d);
    EXPECT_TRUE(box_offset.Is2D());
    EXPECT_FALSE(box_offset.IsOnZPlane());

    // 2DだがZ軸に平行でない (XY平面に対して傾いている)
    Vector3d p0_tilted(1.0, 2.0, 0.0);
    auto rot = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitX());
    std::array<Vector3d, 2> dirs_tilted = {
        Vector3d::UnitX(),
        rot * Vector3d::UnitY()
    };
    BoundingBox box_tilted(p0_tilted, dirs_tilted, sizes_2d);
    EXPECT_TRUE(box_tilted.Is2D());
    EXPECT_FALSE(box_tilted.IsOnZPlane());

    // Z軸周りに回転した2Dボックス (p0.z=0, d2//UnitZ)
    auto rot_z = igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitZ());
    std::array<Vector3d, 2> dirs_rotated_z = {
        rot_z * Vector3d::UnitX(),
        rot_z * Vector3d::UnitY()
    };
    BoundingBox box_rotated_z(p0_on_z, dirs_rotated_z, sizes_2d);
    EXPECT_TRUE(box_rotated_z.Is2D());
    EXPECT_TRUE(box_rotated_z.IsOnZPlane());

    // Empty box (デフォルトコンストラクタ)
    BoundingBox box_empty;
    EXPECT_TRUE(box_empty.Is2D());
    EXPECT_TRUE(box_empty.IsOnZPlane());

    // 2D無限ボックス (s0=Inf, p0.z=0, d2//UnitZ)
    std::array<bool, 2> is_line_2d = {false, false};
    BoundingBox box_inf_2d(p0_on_z, {kInf, 20.0}, is_line_2d);
    EXPECT_TRUE(box_inf_2d.Is2D());
    EXPECT_TRUE(box_inf_2d.IsOnZPlane());

    // 負のZ座標
    Vector3d p0_neg_z(1.0, 2.0, -3.0);
    BoundingBox box_neg_z(p0_neg_z, sizes_2d);
    EXPECT_TRUE(box_neg_z.Is2D());
    EXPECT_FALSE(box_neg_z.IsOnZPlane());

    // d2が-UnitZ (Z軸に平行だが向きが逆)
    std::array<Vector3d, 2> dirs_neg_z = {
        Vector3d::UnitX(),
        Vector3d::UnitY()
    };
    BoundingBox box_dir_neg_z(p0_on_z, dirs_neg_z, sizes_2d);
    // SetDirections2Dでd2 = d0 x d1 = UnitZ となる
    EXPECT_TRUE(box_dir_neg_z.IsOnZPlane());

    // d2を明示的に-UnitZに設定
    std::array<Vector3d, 3> dirs_3d_neg_z = {
        Vector3d::UnitY(),
        Vector3d::UnitX(),
        -Vector3d::UnitZ()
    };
    std::array<double, 3> sizes_flat = {10.0, 20.0, 0.0};
    BoundingBox box_explicit_neg_z(p0_on_z, dirs_3d_neg_z, sizes_flat);
    EXPECT_TRUE(box_explicit_neg_z.Is2D());
    EXPECT_TRUE(box_explicit_neg_z.IsOnZPlane());
}

TEST(BoundingBoxTest, IsFinite) {
    Vector3d p0(1.0, 2.0, 3.0);

    // Empty (s0=s1=s2=0) は有限
    BoundingBox box_empty;
    EXPECT_TRUE(box_empty.IsFinite());

    // 3D Segment
    BoundingBox box_3d(p0, std::array<double, 3>{10.0, 20.0, 30.0});
    EXPECT_TRUE(box_3d.IsFinite());

    // 2D Segment
    BoundingBox box_2d(p0, std::array<double, 2>{10.0, 20.0});
    EXPECT_TRUE(box_2d.IsFinite());

    // Ray
    BoundingBox box_ray(p0, {kInf, 20.0, 30.0}, {false, false, false});
    EXPECT_FALSE(box_ray.IsFinite());

    // Line
    BoundingBox box_line(p0, {10.0, 20.0, 30.0}, {false, true, false});
    EXPECT_FALSE(box_line.IsFinite());

    // 複数の無限 -- Ray, Seg, Line
    BoundingBox box_inf(p0, {kInf, 20.0, kInf}, {false, false, true});
    EXPECT_FALSE(box_inf.IsFinite());
}



/**
 * 頂点取得関数のテスト
 */

TEST(BoundingBoxTest, GetVerticesFinite) {
    Vector3d p0(1.0, 2.0, 3.0);
    double s0 = 10.0, s1 = 20.0, s2 = 30.0;
    Vector3d d0 = Vector3d::UnitX();
    Vector3d d1 = Vector3d::UnitY();
    Vector3d d2 = Vector3d::UnitZ();

    // --- 3D AABB ---
    BoundingBox box_3d(p0, std::array<double, 3>{s0, s1, s2});
    auto vertices_3d = box_3d.GetVertices();
    EXPECT_EQ(vertices_3d.size(), 8);

    // 8頂点の期待値
    EXPECT_TRUE(ContainsVertex(vertices_3d, p0));
    EXPECT_TRUE(ContainsVertex(vertices_3d, p0 + s0 * d0));
    EXPECT_TRUE(ContainsVertex(vertices_3d, p0 + s1 * d1));
    EXPECT_TRUE(ContainsVertex(vertices_3d, p0 + s2 * d2));
    EXPECT_TRUE(ContainsVertex(vertices_3d, p0 + s0 * d0 + s1 * d1));
    EXPECT_TRUE(ContainsVertex(vertices_3d, p0 + s0 * d0 + s2 * d2));
    EXPECT_TRUE(ContainsVertex(vertices_3d, p0 + s1 * d1 + s2 * d2));
    EXPECT_TRUE(ContainsVertex(vertices_3d, p0 + s0 * d0 + s1 * d1 + s2 * d2));

    // --- 2D AABB (s2=0) ---
    BoundingBox box_2d(p0, std::array<double, 2>{s0, s1});
    auto vertices_2d = box_2d.GetVertices();
    EXPECT_EQ(vertices_2d.size(), 4);

    // 4頂点の期待値 (d2方向の移動がない)
    EXPECT_TRUE(ContainsVertex(vertices_2d, p0));
    EXPECT_TRUE(ContainsVertex(vertices_2d, p0 + s0 * d0));
    EXPECT_TRUE(ContainsVertex(vertices_2d, p0 + s1 * d1));
    EXPECT_TRUE(ContainsVertex(vertices_2d, p0 + s0 * d0 + s1 * d1));

    // --- Empty ---
    BoundingBox box_empty;
    auto vertices_empty = box_empty.GetVertices();
    EXPECT_EQ(vertices_empty.size(), 4);  // Is2D() が true のため
    EXPECT_TRUE(ContainsVertex(vertices_empty, Vector3d::Zero()));
    EXPECT_TRUE(i_num::IsApproxEqual(vertices_empty[0], Vector3d::Zero()));
    EXPECT_TRUE(i_num::IsApproxEqual(vertices_empty[1], Vector3d::Zero()));
    EXPECT_TRUE(i_num::IsApproxEqual(vertices_empty[2], Vector3d::Zero()));
    EXPECT_TRUE(i_num::IsApproxEqual(vertices_empty[3], Vector3d::Zero()));
}

TEST(BoundingBoxTest, GetVerticesInfinite) {
    Vector3d p0(1.0, 2.0, 3.0);
    double s1 = 20.0, s2 = 30.0;

    // --- Ray (s0 = +Inf) ---
    BoundingBox box_ray(p0, {kInf, s1, s2}, {false, false, false});
    auto vertices_ray = box_ray.GetVertices();
    EXPECT_EQ(vertices_ray.size(), 8);

    int inf_count = 0;
    int finite_count = 0;
    for (const auto& v : vertices_ray) {
        // AABBをであり、s0はx座標にのみ伝播 (d0 = UnitX)
        if (std::isinf(v[0]) && v[0] > 0) {
            inf_count++;
            // 他の成分は0
            EXPECT_TRUE(i_num::IsApproxEqual(v[1], 0.0)) << v;
            EXPECT_TRUE(i_num::IsApproxEqual(v[2], 0.0)) << v;
        } else {
            finite_count++;
            EXPECT_TRUE(std::isfinite(v[0])) << v;
        }
    }
    EXPECT_EQ(inf_count, 4);
    EXPECT_EQ(finite_count, 4);

    // --- Line (s1 = +/-Inf) ---
    BoundingBox box_line(p0, {10.0, kInf, s2}, {false, true, false});
    auto vertices_line = box_line.GetVertices();
    EXPECT_EQ(vertices_line.size(), 8);

    int pos_inf_count = 0;
    int neg_inf_count = 0;
    for (const auto& v : vertices_line) {
        // AABBであり、s1はy座標にのみ伝播 (d1 = UnitY)
        if (std::isinf(v[1])) {
            if (v[1] > 0) pos_inf_count++;
            else neg_inf_count++;
        }
        // 他の成分は0
        EXPECT_TRUE(i_num::IsApproxEqual(v[0], 0.0)) << v;
        EXPECT_TRUE(i_num::IsApproxEqual(v[2], 0.0)) << v;
    }
    EXPECT_EQ(pos_inf_count, 4);
    EXPECT_EQ(neg_inf_count, 4);
}

TEST(BoundingBoxTest, GetFiniteVertices) {
    Vector3d p0(1.0, 2.0, 3.0);

    // --- 3D Finite ---
    BoundingBox box_3d(p0, std::array<double, 3>{10.0, 20.0, 30.0});
    EXPECT_TRUE(box_3d.IsFinite());
    auto vertices_3d = box_3d.GetFiniteVertices();
    auto vertices_3d_all = box_3d.GetVertices();
    EXPECT_EQ(vertices_3d.size(), 8);
    EXPECT_EQ(vertices_3d.size(), vertices_3d_all.size());  // GetVertices と同じ結果

    // --- 2D Finite ---
    BoundingBox box_2d(p0, std::array<double, 2>{10.0, 20.0});
    EXPECT_TRUE(box_2d.IsFinite());
    auto vertices_2d = box_2d.GetFiniteVertices();
    auto vertices_2d_all = box_2d.GetVertices();
    EXPECT_EQ(vertices_2d.size(), 4);
    EXPECT_EQ(vertices_2d.size(), vertices_2d_all.size());  // GetVertices と同じ結果

    // --- Empty ---
    BoundingBox box_empty;
    EXPECT_TRUE(box_empty.IsFinite());
    auto vertices_empty = box_empty.GetFiniteVertices();
    EXPECT_EQ(vertices_empty.size(), 4);
    EXPECT_TRUE(i_num::IsApproxEqual(vertices_empty[0], Vector3d::Zero()));

    // --- Infinite (Ray) ---
    BoundingBox box_ray(p0, {kInf, 20.0, 30.0}, {false, false, false});
    EXPECT_FALSE(box_ray.IsFinite());
    auto vertices_ray = box_ray.GetFiniteVertices();
    EXPECT_TRUE(vertices_ray.empty());

    // --- Infinite (Line) ---
    BoundingBox box_line(p0, {10.0, 20.0, 30.0}, {false, true, false});
    EXPECT_FALSE(box_line.IsFinite());
    auto vertices_line = box_line.GetFiniteVertices();
    EXPECT_TRUE(vertices_line.empty());
}



/**
 * 包含判定のテスト
 */

// 点の包含判定: Finite, 3D
TEST(BoundingBoxTest, ContainsPoint_Finite3D) {
    Vector3d p0(10, 10, 10);
    std::array<double, 3> sizes = {5.0, 10.0, 20.0};
    BoundingBox box(p0, sizes);

    std::vector<std::tuple<Vector3d, bool, std::string>> test_cases = {
        {Vector3d(12, 15, 20), true, "内部"},
        {Vector3d(10, 10, 10), true, "境界 (頂点 P0)"},
        {Vector3d(15, 20, 30), true, "境界 (頂点 P0 + s0*D0 + s1*D1 + s2*D2)"},
        {Vector3d(10, 15, 20), true, "境界 (面 D0 側)"},
        {Vector3d(15, 15, 20), true, "境界 (面 s0 側)"},
        {Vector3d(12, 10, 20), true, "境界 (面 D1 側)"},
        {Vector3d(12, 20, 20), true, "境界 (面 s1 側)"},
        {Vector3d(12, 15, 10), true, "境界 (面 D2 側)"},
        {Vector3d(12, 15, 30), true, "境界 (面 s2 側)"},
        {Vector3d(10, 10, 20), true, "境界 (辺)"},
        {Vector3d(9, 15, 20), false, "外部 (D0 負方向)"},
        {Vector3d(16, 15, 20), false, "外部 (s0 正方向)"},
        {Vector3d(12, 9, 20), false, "外部 (D1 負方向)"},
        {Vector3d(12, 21, 20), false, "外部 (s1 正方向)"},
        {Vector3d(12, 15, 9), false, "外部 (D2 負方向)"},
        {Vector3d(12, 15, 31), false, "外部 (s2 正方向)"},
        {Vector3d(kInf, 15, 20), false, "外部 (s0 = +Inf)"},
        {Vector3d(-kInf, 15, 20), false, "外部 (s0 = -Inf)"},
        {Vector3d(12, kInf, 20), false, "外部 (s1 = +Inf)"},
        {Vector3d(12, -kInf, 20), false, "外部 (s1 = -Inf)"},
        {Vector3d(12, 15, kInf), false, "外部 (s2 = +Inf)"},
        {Vector3d(12, 15, -kInf), false, "外部 (s2 = -Inf)"}
    };

    // AABBなboxについて検証
    for (const auto& [point, expected, description] : test_cases) {
        SCOPED_TRACE(description);
        EXPECT_EQ(box.Contains(point), expected)
            << "point: " << point.transpose();
    }

    // 回転したボックスについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    box.Rotate(rotation);
    p0 = box.GetControl();
    for (const auto& [point, expected, description] : test_cases) {
        SCOPED_TRACE("回転後: " + description);
        auto rotated_point = rotation * (point - p0) + p0;
        EXPECT_EQ(box.Contains(rotated_point), expected)
            << "point: " << rotated_point.transpose();
    }
}

// 点の包含判定: Finite, 2D
TEST(BoundingBoxTest, ContainsPoint_Finite2D) {
    Vector3d p0(10, 10, 10);
    std::array<double, 2> sizes_2d = {5.0, 10.0};
    BoundingBox box(p0, sizes_2d);

    std::vector<std::tuple<Vector3d, bool, std::string>> test_cases = {
        {Vector3d(12, 15, 10), true, "内部"},
        {Vector3d(10, 10, 10), true, "境界 (頂点 P0)"},
        {Vector3d(15, 20, 10), true, "境界 (頂点 P0 + s0*D0 + s1*D1)"},
        {Vector3d(10, 15, 10), true, "境界 (辺 D0 側)"},
        {Vector3d(15, 15, 10), true, "境界 (辺 s0 側)"},
        {Vector3d(12, 10, 10), true, "境界 (辺 D1 側)"},
        {Vector3d(12, 20, 10), true, "境界 (辺 s1 側)"},
        {Vector3d(9, 15, 10), false, "外部 (平面上, D0 負方向)"},
        {Vector3d(16, 15, 10), false, "外部 (平面上, s0 正方向)"},
        {Vector3d(12, 9, 10), false, "外部 (平面上, D1 負方向)"},
        {Vector3d(12, 21, 10), false, "外部 (平面上, s1 正方向)"},
        {Vector3d(12, 15, 11), false, "外部 (Z軸方向, 正)"},
        {Vector3d(12, 15, 9), false, "外部 (Z軸方向, 負)"},
        {Vector3d(kInf, 15, 10), false, "外部 (s0 = +Inf)"},
        {Vector3d(-kInf, 15, 10), false, "外部 (s0 = -Inf)"},
        {Vector3d(12, kInf, 10), false, "外部 (s1 = +Inf)"},
        {Vector3d(12, -kInf, 10), false, "外部 (s1 = -Inf)"}
    };

    // AABBなboxについて検証
    for (const auto& [point, expected, description] : test_cases) {
        SCOPED_TRACE(description);
        EXPECT_EQ(box.Contains(point), expected)
            << "point: " << point.transpose();
    }

    // 回転したボックスについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    box.Rotate(rotation);
    p0 = box.GetControl();
    for (const auto& [point, expected, description] : test_cases) {
        SCOPED_TRACE("回転後: " + description);
        auto rotated_point = rotation * (point - p0) + p0;
        EXPECT_EQ(box.Contains(rotated_point), expected)
            << "point: " << rotated_point.transpose();
    }
}

// 点の包含判定: Infinite, Ray, 3D
TEST(BoundingBoxTest, ContainsPoint_InfiniteRay3D) {
    Vector3d p0(10, 10, 10);
    std::array<double, 3> sizes = {kInf, 10.0, 20.0};
    std::array<bool, 3> is_line = {false, false, false};  // kRay for s0
    BoundingBox box(p0, sizes, is_line);

    EXPECT_EQ(box.GetDirectionTypes()[0], DT::kRay);
    EXPECT_EQ(box.GetDirectionTypes()[1], DT::kSegment);
    EXPECT_EQ(box.GetDirectionTypes()[2], DT::kSegment);

    std::vector<std::tuple<Vector3d, bool, std::string>> test_cases = {
        {Vector3d(1000, 15, 20), true, "内部 (遠方)"},
        {Vector3d(10, 15, 20), true, "境界 (始点面)"},
        {Vector3d(9, 15, 20), false, "外部 (始点の手前)"},
        {Vector3d(1000, 9, 20), false, "外部 (有限方向 s1 負)"},
        {Vector3d(1000, 21, 20), false, "外部 (有限方向 s1 正)"},
        {Vector3d(1000, 15, 9), false, "外部 (有限方向 s2 負)"},
        {Vector3d(1000, 15, 31), false, "外部 (有限方向 s2 正)"},
        {Vector3d(10, 10, 10), true, "境界 (頂点 P0)"},
        {Vector3d(10, 20, 30), true, "境界 (頂点 P0 + s1*D1 + s2*D2)"},
        {Vector3d(kInf, 15, 20), true, "内部 (s0 = +Inf)"},
        {Vector3d(-kInf, 15, 20), false, "外部 (s0 = -Inf)"},
        {Vector3d(1000, kInf, 20), false, "外部 (s1 = +Inf)"},
        {Vector3d(1000, -kInf, 20), false, "外部 (s1 = -Inf)"},
        {Vector3d(1000, 15, kInf), false, "外部 (s2 = +Inf)"},
        {Vector3d(1000, 15, -kInf), false, "外部 (s2 = -Inf)"}
    };

    // AABBなboxについて検証
    for (const auto& [point, expected, description] : test_cases) {
        SCOPED_TRACE(description);
        EXPECT_EQ(box.Contains(point), expected)
            << "point: " << point.transpose();
    }

    // 回転したボックスについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    box.Rotate(rotation);
    p0 = box.GetControl();
    for (const auto& [point, expected, description] : test_cases) {
        if (!point.allFinite()) {
            // 無限大の点の比較はスキップ (点だけを与えられても正しく判別できないため)
            continue;
        }

        SCOPED_TRACE("回転後: " + description);
        auto rotated_point = rotation * (point - p0) + p0;
        EXPECT_EQ(box.Contains(rotated_point), expected)
            << "point: " << rotated_point.transpose();
    }
}

// 点の包含判定: Infinite, Line, 3D
TEST(BoundingBoxTest, ContainsPoint_InfiniteLine3D) {
    Vector3d p0(10, 10, 10);
    std::array<double, 3> sizes = {kInf, 10.0, 20.0};
    std::array<bool, 3> is_line = {true, false, false};  // kLine for s0
    BoundingBox box(p0, sizes, is_line);

    EXPECT_EQ(box.GetDirectionTypes()[0], DT::kLine);
    EXPECT_EQ(box.GetDirectionTypes()[1], DT::kSegment);
    EXPECT_EQ(box.GetDirectionTypes()[2], DT::kSegment);

    std::vector<std::tuple<Vector3d, bool, std::string>> test_cases = {
        {Vector3d(1000, 15, 20), true, "内部 (正の遠方)"},
        {Vector3d(-1000, 15, 20), true, "内部 (負の遠方)"},
        {Vector3d(10, 15, 20), true, "内部 (P0付近)"},
        {Vector3d(1000, 9, 20), false, "外部 (有限方向 s1 負)"},
        {Vector3d(-1000, 21, 20), false, "外部 (有限方向 s1 正)"},
        {Vector3d(1000, 15, 9), false, "外部 (有限方向 s2 負)"},
        {Vector3d(-1000, 15, 31), false, "外部 (有限方向 s2 正)"},
        {Vector3d(10, 10, 10), true, "境界 (頂点 P0)"},
        {Vector3d(10, 20, 30), true, "境界 (頂点 P0 + s1*D1 + s2*D2)"},
        {Vector3d(kInf, 15, 20), true, "内部 (s0 = +Inf)"},
        {Vector3d(-kInf, 15, 20), true, "内部 (s0 = -Inf)"},
        {Vector3d(1000, kInf, 20), false, "外部 (s1 = +Inf)"},
        {Vector3d(1000, -kInf, 20), false, "外部 (s1 = -Inf)"},
        {Vector3d(1000, 15, kInf), false, "外部 (s2 = +Inf)"},
        {Vector3d(1000, 15, -kInf), false, "外部 (s2 = -Inf)"}
    };

    // AABBなboxについて検証
    for (const auto& [point, expected, description] : test_cases) {
        SCOPED_TRACE(description);
        EXPECT_EQ(box.Contains(point), expected)
            << "point: " << point.transpose();
    }

    // 回転したボックスについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    box.Rotate(rotation);
    p0 = box.GetControl();
    for (const auto& [point, expected, description] : test_cases) {
        if (!point.allFinite()) {
            // 無限大の点の比較はスキップ (点だけを与えられても正しく判別できないため)
            continue;
        }

        SCOPED_TRACE("回転後: " + description);
        auto rotated_point = rotation * (point - p0) + p0;
        EXPECT_EQ(box.Contains(rotated_point), expected)
            << "point: " << rotated_point.transpose();
    }
}

// boxの包含判定: Finite, 3D
TEST(BoundingBoxTest, ContainsBox_Finite3D) {
    Vector3d pA(0.0, 0.0, 0.0);
    std::array<double, 3> sA = {10.0, 10.0, 10.0};
    BoundingBox A(pA, sA);  // A: [0,10]^3

    struct Case {
        Vector3d pB;
        std::array<double, 3> sB;
        bool expected;
        std::string desc;
    };

    std::vector<Case> cases = {
        { Vector3d(1.0, 1.0, 1.0), {2.0, 2.0, 2.0}, true,
          "完全に内包: B inside A" },
        { Vector3d(1.0, 1.0, 1.0), {2.0, 2.0, 0.0}, true,
          "完全に内包 (2D): B inside A" },
        { Vector3d(0.0, 0.0, 0.0), {2.0, 2.0, 2.0}, true,
          "境界で接する (内側): B starts at A's min" },
        { Vector3d(8.0, 8.0, 8.0), {2.0, 2.0, 2.0}, true,
          "境界で接する (内側): B ends at A's max" },
        { Vector3d{0.0, 0.0, 0.0}, {10.0, 10.0, 0.0}, true,
          "境界で接する (内側, 2D): B matches A in XY, zero height" },
        { Vector3d(5.0, 5.0, 5.0), {10.0, 10.0, 10.0}, false,
          "一部がはみ出す: B extends to [5,15]" },
        { Vector3d(5.0, 5.0, 5.0), {2.0, kInf, 2.0}, false,
          "一部がはみ出す (無限): B extends infinitely in Y" },
        { Vector3d(9.0, 9.0, 9.0), {2.0, 2.0, 0.0}, false,
          "一部がはみ出す (2D): B extends beyond A in all directions" },
        { Vector3d(11.0, 11.0, 11.0), {2.0, 2.0, 2.0}, false,
          "完全に外部: B is outside A" },
        { Vector3d(11.0, 11.0, 11.0), {2.0, 2.0, 0.0}, false,
          "完全に外部 (2D): B is outside A" },
        { Vector3d(-1.0, -1.0, -1.0), {12.0, 12.0, 12.0}, false,
          "AがBに内包される (逆): A is inside B, so A does NOT contain B" }
    };

    // 判定側がAABB
    for (const auto& c : cases) {
        SCOPED_TRACE(c.desc);
        BoundingBox B(c.pB, c.sB);
        EXPECT_EQ(A.Contains(B), c.expected)
            << "A.Contains(B) for B at " << c.pB.transpose()
            << " size=(" << c.sB[0] << "," << c.sB[1] << "," << c.sB[2] << ")";
    }

    // 回転したboxについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    A.Rotate(rotation, Vector3d::Zero());
    for (const auto& c : cases) {
        SCOPED_TRACE("回転後: " + c.desc);
        BoundingBox B(c.pB, c.sB);
        B.Rotate(rotation, Vector3d::Zero());
        auto b_sizes = B.GetSizes();
        EXPECT_EQ(A.Contains(B), c.expected)
            << "A.Contains(B) for rotated B at " << B.GetControl().transpose()
            << " size=(" << b_sizes[0] << "," << b_sizes[1] << "," << b_sizes[2] << ")";
    }
}

// boxの包含判定: Finite, 2D
TEST(BoundingBoxTest, ContainsBox_Finite2D) {
    Vector3d pA(0.0, 0.0, 0.0);
    std::array<double, 2> sA = {10.0, 10.0};
    BoundingBox A(pA, sA);  // A: [0,10]^2 in XY plane

    struct Case {
        Vector3d pB;
        std::array<double, 3> sB;
        bool expected;
        std::string desc;
    };

    std::vector<Case> cases = {
        { Vector3d(1.0, 1.0, 0.0), {2.0, 2.0, 0.0}, true,
          "完全に内包: B inside A" },
        { Vector3d(0.0, 0.0, 0.0), {2.0, 2.0, 0.0}, true,
          "境界で接する (内側): B starts at A's min" },
        { Vector3d(8.0, 8.0, 0.0), {2.0, 2.0, 0.0}, true,
          "境界で接する (内側): B ends at A's max" },
        { Vector3d(5.0, 5.0, 0.0), {10.0, 10.0, 0.0}, false,
          "一部がはみ出す: B extends to [5,15]" },
        { Vector3d(9.0, 9.0, 0.0), {2.0, 2.0, 0.0}, false,
          "一部がはみ出す: B extends beyond A in all directions" },
        { Vector3d(11.0, 11.0, 0.0), {2.0, 2.0, 0.0}, false,
          "完全に外部: B is outside A" },
        { Vector3d(-1.0, -1.0, 0.0), {12.0, 12.0, 0.0}, false,
          "AがBに内包される (逆): A is inside B, so A does NOT contain B" },
        { Vector3d(1.0, 1.0, 1.0), {2.0, 2.0, 0.0}, false,
          "Z方向にずれた: B is above A in Z direction" },
        { Vector3d(0.0, 0.0, 0.0), {10.0, 10.0, 5.0}, false,
          "Z方向に厚みがある: B has thickness in Z direction" }
    };

    // 判定側がAABB
    for (const auto& c : cases) {
        SCOPED_TRACE(c.desc);
        BoundingBox B(c.pB, c.sB);
        EXPECT_EQ(A.Contains(B), c.expected)
            << "A.Contains(B) for B at " << c.pB.transpose()
            << " size=(" << c.sB[0] << "," << c.sB[1] << ")";
    }

    // 回転したboxについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    A.Rotate(rotation, Vector3d::Zero());
    for (const auto& c : cases) {
        SCOPED_TRACE("回転後: " + c.desc);
        BoundingBox B(c.pB, c.sB);
        B.Rotate(rotation, Vector3d::Zero());
        auto b_sizes = B.GetSizes();
        EXPECT_EQ(A.Contains(B), c.expected)
            << "A.Contains(B) for rotated B at " << B.GetControl().transpose()
            << " size=(" << b_sizes[0] << "," << b_sizes[1] << ")";
    }
}

// boxの包含判定: Infinite, Ray, 3D
TEST(BoundingBoxTest, ContainsBox_InfiniteRay3D) {
    Vector3d pA(0.0, 0.0, 0.0);
    std::array<double, 3> sA = {kInf, 10.0, 10.0};
    std::array<bool, 3> is_line_A = {false, false, false};  // kRay for s0
    BoundingBox A(pA, sA, is_line_A);  // A: Ray in X, [0,10] in YZ

    struct Case {
        Vector3d pB;
        std::array<double, 3> sB;
        std::array<bool, 3> is_line_B;
        bool expected;
        std::string desc;
    };

    auto is_line_fff = std::array<bool, 3>{false, false, false};
    std::vector<Case> cases = {
        { Vector3d(1.0, 1.0, 1.0), {2.0, 2.0, 2.0}, is_line_fff, true,
          "完全に内包: B inside A" },
        { Vector3d(1.0, 1.0, 1.0), {kInf, 2.0, 2.0}, is_line_fff, true,
          "完全に内包 (無限; kRay): B inside A" },
        { Vector3d(1.0, 1.0, 1.0), {2.0, 2.0, 0.0}, is_line_fff, true,
          "完全に内包 (2D): B inside A" },
        { Vector3d(0.0, 0.0, 0.0), {2.0, 2.0, 2.0}, is_line_fff, true,
          "境界で接する (内側): B starts at A's min" },
        { Vector3d(1000.0, 8.0, 8.0), {2.0, 2.0, 2.0}, is_line_fff, true,
          "境界で接する (内側、遠方): B far along A's ray direction" },
        { Vector3d(5.0, 5.0, 5.0), {10.0, 10.0, 10.0}, is_line_fff, false,
          "一部がはみ出す: B extends to [5,15] in YZ" },
        { Vector3d(5.0, 5.0, 5.0), {2.0, kInf, 2.0}, is_line_fff, false,
          "一部がはみ出す (無限): B extends infinitely in Y" },
        { Vector3d(9.0, 9.0, 9.0), {kInf, 2.0, 2.0}, {true, false, false}, false,
          "一部がはみ出す (無限; kLine): B inside A" },
        { Vector3d(9.0, 9.0, 9.0), {2.0, 2.0, 0.0}, is_line_fff, false,
          "一部がはみ出す (2D): B extends beyond A in all directions" },
        { Vector3d(11.0, 11.0, 11.0), {2.0, 2.0, 2.0}, is_line_fff, false,
          "完全に外部: B is outside A" },
        { Vector3d(11.0, 11.0, 11.0), {2.0, 2.0, 0.0}, is_line_fff, false,
          "完全に外部 (2D): B is outside A" },
        { Vector3d(-1.0, -1.0, -1.0), {12.0, 12.0, 12.0}, is_line_fff, false,
          "AがBに内包される (逆): A is inside B, so A does NOT contain B" }
    };

    // 判定側がAABB
    for (const auto& c : cases) {
        SCOPED_TRACE(c.desc);
        BoundingBox B(c.pB, c.sB, c.is_line_B);
        EXPECT_EQ(A.Contains(B), c.expected)
            << "A.Contains(B) for B at " << c.pB.transpose()
            << " size=(" << c.sB[0] << "," << c.sB[1] << "," << c.sB[2] << ")";
    }

    // 回転したboxについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    A.Rotate(rotation, Vector3d::Zero());
    for (const auto& c : cases) {
        if (!std::all_of(c.sB.begin(), c.sB.end(), [](double v) { return std::isfinite(v); })) {
            // TODO: 正しく判定できるようにする
            continue;
        }

        SCOPED_TRACE("回転後: " + c.desc);
        BoundingBox B(c.pB, c.sB, c.is_line_B);
        B.Rotate(rotation, Vector3d::Zero());
        auto b_sizes = B.GetSizes();
        EXPECT_EQ(A.Contains(B), c.expected)
            << "A.Contains(B) for rotated B at " << B.GetControl().transpose()
            << " size=(" << b_sizes[0] << "," << b_sizes[1] << "," << b_sizes[2] << ")";
    }
}

// boxの包含判定: Infinite, Line, 3D
TEST(BoundingBoxTest, ContainsBox_InfiniteLine3D) {
    Vector3d pA(0.0, 0.0, 0.0);
    std::array<double, 3> sA = {kInf, 10.0, 10.0};
    std::array<bool, 3> is_line_A = {true, false, false};  // kLine for s0
    BoundingBox A(pA, sA, is_line_A);  // A: Line in X, [0,10] in YZ

    struct Case {
        Vector3d pB;
        std::array<double, 3> sB;
        std::array<bool, 3> is_line_B;
        bool expected;
        std::string desc;
    };

    auto is_line_fff = std::array<bool, 3>{false, false, false};
    std::vector<Case> cases = {
        { Vector3d(1.0, 1.0, 1.0), {2.0, 2.0, 2.0}, is_line_fff, true,
          "完全に内包: B inside A" },
        { Vector3d(1.0, 1.0, 1.0), {kInf, 2.0, 2.0}, is_line_fff, true,
          "完全に内包 (無限; kRay): B inside A" },
        { Vector3d(1.0, 1.0, 1.0), {kInf, 2.0, 2.0}, {true, false, false}, true,
          "完全に内包 (無限; kLine): B inside A" },
        { Vector3d(1.0, 1.0, 1.0), {2.0, 2.0, 0.0}, is_line_fff, true,
          "完全に内包 (2D): B inside A" },
        { Vector3d(0.0, 0.0, 0.0), {2.0, 2.0, 2.0}, is_line_fff, true,
          "境界で接する (内側): B starts at A's min" },
        { Vector3d(1000.0, 8.0, 8.0), {2.0, 2.0, 2.0}, is_line_fff, true,
          "境界で接する (内側、遠方): B far along A's ray direction" },
        { Vector3d(5.0, 5.0, 5.0), {10.0, 10.0, 10.0}, is_line_fff, false,
          "一部がはみ出す: B extends to [5,15] in YZ" },
        { Vector3d(5.0, 5.0, 5.0), {2.0, kInf, 2.0}, is_line_fff, false,
          "一部がはみ出す (無限): B extends infinitely in Y" },
        { Vector3d(9.0, 9.0, 9.0), {2.0, 2.0, 0.0}, is_line_fff, false,
          "一部がはみ出す (2D): B extends beyond A in all directions" },
        { Vector3d(11.0, 11.0, 11.0), {2.0, 2.0, 2.0}, is_line_fff, false,
          "完全に外部: B is outside A" },
        { Vector3d(11.0, 11.0, 11.0), {2.0, 2.0, 0.0}, is_line_fff, false,
          "完全に外部 (2D): B is outside A" },
        { Vector3d(-1.0, -1.0, -1.0), {12.0, 12.0, 12.0}, is_line_fff, false,
          "AがBに内包される (逆): A is inside B, so A does NOT contain B" }
    };

    // 判定側がAABB
    for (const auto& c : cases) {
        SCOPED_TRACE(c.desc);
        BoundingBox B(c.pB, c.sB, c.is_line_B);
        EXPECT_EQ(A.Contains(B), c.expected)
            << "A.Contains(B) for B at " << c.pB.transpose()
            << " size=(" << c.sB[0] << "," << c.sB[1] << "," << c.sB[2] << ")";
    }

    // 回転したboxについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    A.Rotate(rotation, Vector3d::Zero());
    for (const auto& c : cases) {
        if (!std::all_of(c.sB.begin(), c.sB.end(), [](double v) { return std::isfinite(v); })) {
            // TODO: 正しく判定できるようにする
            continue;
        }

        SCOPED_TRACE("回転後: " + c.desc);
        BoundingBox B(c.pB, c.sB, c.is_line_B);
        B.Rotate(rotation, Vector3d::Zero());
        auto b_sizes = B.GetSizes();
        EXPECT_EQ(A.Contains(B), c.expected)
            << "A.Contains(B) for rotated B at " << B.GetControl().transpose()
            << " size=(" << b_sizes[0] << "," << b_sizes[1] << "," << b_sizes[2] << ")";
    }
}



/**
 * 交差判定のテスト
 */

namespace {

BoundingBox FiniteAABBBox2D() {
    Vector3d p0(10, 10, 10);
    std::array<double, 2> sizes_2d = {10.0, 10.0};
    return BoundingBox(p0, sizes_2d);
}
BoundingBox FiniteAABBBox() {
    Vector3d p0(10, 10, 10);
    std::array<double, 3> sizes = {10.0, 10.0, 10.0};
    return BoundingBox(p0, sizes);
}
BoundingBox RayAABBBox() {
    Vector3d p0(10, 10, 10);
    std::array<double, 3> sizes = {kInf, 10.0, 10.0};
    std::array<bool, 3> is_line = {false, false, false};  // kRay for s0
    return BoundingBox(p0, sizes, is_line);
}

}  // namespace

// 線との交差判定: エラーケース
TEST(BoundingBoxTest, IntersectsLine_ErrorCases) {
    BoundingBox box = FiniteAABBBox();

    // 線分の長さが0
    auto start = Vector3d(0, 0, 0);
    auto end = Vector3d(0, 0, 0);
    EXPECT_THROW(box.Intersects(start, end, DT::kSegment), std::invalid_argument);
    EXPECT_THROW(box.Intersects(start, end, DT::kRay), std::invalid_argument);
    EXPECT_THROW(box.Intersects(start, end, DT::kLine), std::invalid_argument);

    // startまたはendにNaNが含まれる
    start(0) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(box.Intersects(start, end, DT::kSegment), std::invalid_argument);
    start(0) = 0.0;
    end(1) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(box.Intersects(start, end, DT::kSegment), std::invalid_argument);

    // startまたはendに無限大が含まれる
    end(1) = kInf;
    EXPECT_THROW(box.Intersects(start, end, DT::kSegment), std::invalid_argument);
    end(1) = -kInf;
    EXPECT_THROW(box.Intersects(start, end, DT::kSegment), std::invalid_argument);
}

// 線との交差判定: kSegment
TEST(BoundingBoxTest, IntersectsLine) {
    std::vector<std::pair<BoundingBox, std::string>> boxes;
    boxes.emplace_back(FiniteAABBBox2D(), "2D Box (p0=(10,10,10), size=(10,10))");
    boxes.emplace_back(FiniteAABBBox(), "3D Box (p0=(10,10,10), size=(10,10,10))");
    boxes.emplace_back(RayAABBBox(), "Ray Box (p0=(10,10,10), size=(Inf,10,10))");

    struct TestCase {
        std::string description;
        Vector3d start;
        Vector3d end;
        std::array<bool, 3> seg_expected;   // 2D, 3D, Ray
        std::array<bool, 3> ray_expected;   // 2D, 3D, Ray
        std::array<bool, 3> line_expected;  // 2D, 3D, Ray

        const bool& Expected(size_t i_type, size_t i_box) const {
            switch (i_type) {
                case 0:  // kSegment
                    return seg_expected[i_box];
                case 1:  // kRay
                    return ray_expected[i_box];
                case 2:  // kLine
                    return line_expected[i_box];
            }
            throw std::out_of_range("Invalid type index");
        }
    };
    std::array<DT, 3> types = {DT::kSegment, DT::kRay, DT::kLine};
    std::array<std::string, 3> type_names = {"Segment", "Ray", "Line"};

    std::vector<TestCase> test_cases = {
        { "(12,12,12)->(18,18,18) -- 内包 (w.o. 2D)",
          Vector3d(12, 12, 12), Vector3d(18, 18, 18),
          {false, true, true}, {false, true, true}, {true, true, true} },
        { "(12,12,10)->(18,18,10) -- 内包",
          Vector3d(12, 12, 10), Vector3d(18, 18, 10),
          {true, true, true}, {true, true, true}, {true, true, true} },
        { "(15,15,0)->(15,15,30) -- 貫通",
          Vector3d(15, 15, 0), Vector3d(15, 15, 30),
          {true, true, true}, {true, true, true}, {true, true, true} },
        { "(0,15,15)->(30,15,15) -- 貫通 (w.o. 2D)",
          Vector3d(0, 15, 15), Vector3d(30, 15, 15),
          {false, true, true}, {false, true, true}, {false, true, true} },
        { "(15,15,15)->(30,15,15) -- 始点が内部 (w.o. 2D)",
          Vector3d(15, 15, 15), Vector3d(30, 15, 15),
          {false, true, true}, {false, true, true}, {false, true, true} },
        { "(0,15,15)->(15,15,15) -- 終点が内部 (w.o. 2D)",
          Vector3d(0, 15, 15), Vector3d(15, 15, 15),
          {false, true, true}, {false, true, true}, {false, true, true} },
        { "(10,10,10)->(10,10,0) -- 頂点に接触",
            Vector3d(10, 10, 10), Vector3d(10, 10, 0),
            {true, true, true}, {true, true, true}, {true, true, true} },
        { "(10,15,10)->(10,15,0) -- 辺に接触",
            Vector3d(10, 15, 10), Vector3d(10, 15, 0),
            {true, true, true}, {true, true, true}, {true, true, true} },
        { "(15,15,10)->(15,15,0) -- 面に接触",
            Vector3d(15, 15, 10), Vector3d(15, 15, 0),
            {true, true, true}, {true, true, true}, {true, true, true} },
        { "(0,15,15)->(9.9,15,15) -- 手前で終了",
          Vector3d(0, 15, 15), Vector3d(9.9, 15, 15),
          {false, false, false}, {false, true, true}, {false, true, true} },
        { "(21,15,15)->(30,15,15) -- 奥から開始",
          Vector3d(21, 15, 15), Vector3d(30, 15, 15),
          {false, false, true}, {false, false, true}, {false, true, true} },
        { "(0,0,0)->(30,0,0) -- 平行 (横)",
          Vector3d(0, 0, 0), Vector3d(30, 0, 0),
          {false, false, false}, {false, false, false}, {false, false, false} },
        { "(0,30,15)->(30,30,15) -- 平行 (上)",
          Vector3d(0, 30, 15), Vector3d(30, 30, 15),
          {false, false, false}, {false, false, false}, {false, false, false} },
    };

    // AABBの場合のテスト
    for (size_t i_box = 0; i_box < boxes.size(); ++i_box) {
        const auto& [box, box_name] = boxes[i_box];

        for (const auto& tc : test_cases) {
            for (size_t i_type = 0; i_type < types.size(); ++i_type) {
                SCOPED_TRACE(box_name + ", " + type_names[i_type] + ": " + tc.description);
                bool result = box.Intersects(tc.start, tc.end, types[i_type]);
                EXPECT_EQ(result, tc.Expected(i_type, i_box));
            }
        }
    }

    // 回転したboxについても検証
    auto rotation = igesio::AngleAxisd(igesio::kPi / 4.0, Vector3d::UnitZ());
    rotation = rotation * igesio::AngleAxisd(igesio::kPi / 6.0, Vector3d::UnitY());
    for (size_t i_box = 0; i_box < boxes.size(); ++i_box) {
        // boxを回転
        auto [box, box_name] = boxes[i_box];
        box.Rotate(rotation);
        auto ctr = box.GetControl();

        for (const auto& tc : test_cases) {
            // 線分の両端点を回転
            auto rotated_start = rotation * (tc.start - ctr) + ctr;
            auto rotated_end = rotation * (tc.end - ctr) + ctr;

            for (size_t i_type = 0; i_type < types.size(); ++i_type) {
                SCOPED_TRACE(box_name + " (回転後), " + type_names[i_type] + ": " + tc.description);
                bool result = box.Intersects(rotated_start, rotated_end, types[i_type]);
                EXPECT_EQ(result, tc.Expected(i_type, i_box));
            }
        }
    }
}
