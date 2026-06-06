/**
 * @file tests/models/test_flatten.cpp
 * @brief models/flatten.h (Materialize/Flatten) とライタのフラット出力のテスト (P7)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   - models::Materialize  (配置をDE変換124へ畳み込んだ複製の生成)
 *   - models::Flatten      (Assembly階層の配置を畳み込んだフラットなIgesData生成)
 *   - writer.cpp::CollectAllEntities (全子孫Assemblyの所有エンティティをフラット出力)
 *
 * 設計方針:
 *   - 合成順序の検証は閉形式で予測できる合成Line (M_entity単位) を主に用いる。
 *   - 非単位M_entityとの合成はOverwriteTransformationMatrixで既知の124を仕込んで検証する。
 *   - スケールを含む変換 (§11末尾) は当面対象外 (124相当の回転+並進のみ)。
 *
 * TODO: 共有定義の重複 (DAG→ツリー: 同一被参照を異経路の異なるM_chainで複製) は所属排他+
 *       合成サーフェス構築の都合で本フェーズでは未カバー。
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "igesio/reader.h"
#include "igesio/writer.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/views/curve_view.h"
#include "igesio/entities/transformations/transformation_matrix.h"
#include "igesio/models/assembly.h"
#include "igesio/models/iges_data.h"
#include "igesio/models/flatten.h"

namespace {

namespace fs = std::filesystem;
namespace i_ent = igesio::entities;
namespace i_mdl = igesio::models;
namespace i_num = igesio::numerics;
using igesio::ObjectID;
using igesio::Matrix3d;
using igesio::Matrix4d;
using igesio::Vector3d;
using i_mdl::Assembly;
using i_mdl::CoordFrame;
using i_mdl::MakeAssembly;
using i_ent::Line;

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief π/2 (回転角に使用)
constexpr double kPiHalf = 1.57079632679489661923;

/// @brief 並進のみの同次変換行列を生成する
igesio::Matrix4d MakeTranslation(const Vector3d& t) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m.topRightCorner<3, 1>() = t;
    return m;
}

/// @brief 回転 R(angle, axis) と並進 t からなる同次変換行列を生成する
igesio::Matrix4d MakeTransform(const double angle, const Vector3d& axis,
                               const Vector3d& t) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    if (angle != 0.0) {
        m.topLeftCorner<3, 3>() =
            igesio::AngleAxisd(angle, axis.normalized()).toRotationMatrix();
    }
    m.topRightCorner<3, 1>() = t;
    return m;
}

/// @brief 始点・終点を持つ線分エンティティ (M_entityは単位) を生成する
std::shared_ptr<Line> MakeLine(const Vector3d& start, const Vector3d& end) {
    return i_ent::MakeLine(start, end);
}

/// @brief Assembly直下から最初のLine(110)を曲線として取り出す
std::shared_ptr<const i_ent::ICurve> FirstLineCurve(const Assembly& node) {
    for (const auto& [id, e] : node.GetEntities()) {
        if (e->GetType() == i_ent::EntityType::kLine) {
            return std::dynamic_pointer_cast<const i_ent::ICurve>(e);
        }
    }
    return nullptr;
}

/// @brief 指定型のエンティティ数を数える
size_t CountType(const Assembly& node, const i_ent::EntityType type) {
    size_t n = 0;
    for (const auto& [id, e] : node.GetEntities()) {
        if (e->GetType() == type) ++n;
    }
    return n;
}

}  // namespace



/**
 * Materialize
 */

// 単位配置: 124は生成されず、複製は別ポインタ・別IDで同型
TEST(MaterializeTest, IdentityPlacementKeepsTransformNull) {
    auto line = MakeLine({0, 0, 0}, {1, 0, 0});
    const auto res = i_mdl::Materialize(*line, Matrix4d::Identity());

    EXPECT_EQ(res.transform, nullptr);
    ASSERT_NE(res.entity, nullptr);
    EXPECT_NE(res.entity.get(), line.get());
    EXPECT_NE(res.entity->GetID(), line->GetID());
    EXPECT_EQ(res.entity->GetType(), line->GetType());
}

// 並進配置: 新124を生成し、畳み込み後の変換と点が配置適用と一致する
TEST(MaterializeTest, FoldsTranslationIntoNew124) {
    auto line = MakeLine({0, 0, 0}, {1, 0, 0});  // M_entity単位
    const Matrix4d p = MakeTranslation({1, 2, 3});
    const auto res = i_mdl::Materialize(*line, p);

    ASSERT_NE(res.transform, nullptr);
    // M_entityが単位なので folded = p
    const Matrix4d folded = res.entity->GetTransformationMatrix().GetTransformation();
    EXPECT_TRUE(i_num::IsApproxEqual(folded, p));

    auto clone = std::dynamic_pointer_cast<const i_ent::ICurve>(res.entity);
    ASSERT_NE(clone, nullptr);
    const double t = 0.5;
    const auto p_clone = clone->TryGetPointAt(t);  // M_entity = folded = p を適用
    const auto p_def = line->TryGetPointAt(t);      // M_entity単位 = 定義空間点
    ASSERT_TRUE(p_clone.has_value());
    ASSERT_TRUE(p_def.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(
            *p_clone, i_num::ApplyTransform(p, *p_def, true)));
}

// 既存M_entityとの合成: folded = placement·M_entity
TEST(MaterializeTest, ComposesWithExistingMEntity) {
    auto line = MakeLine({0, 0, 0}, {1, 0, 0});
    const Matrix3d r =
        igesio::AngleAxisd(kPiHalf, Vector3d(0, 0, 1).normalized()).toRotationMatrix();
    const Vector3d t_ent(5, 0, 0);
    ASSERT_TRUE(line->OverwriteTransformationMatrix(
            i_ent::MakeTransformationMatrix(r, t_ent)));
    const Matrix4d m_entity = line->GetTransformationMatrix().GetTransformation();

    const Matrix4d p = MakeTranslation({0, 10, 0});
    const auto res = i_mdl::Materialize(*line, p);
    ASSERT_NE(res.transform, nullptr);

    const Matrix4d folded = res.entity->GetTransformationMatrix().GetTransformation();
    const Matrix4d expected = p * m_entity;
    EXPECT_TRUE(i_num::IsApproxEqual(folded, expected));

    // 点の二経路一致: 複製(folded適用) == 元(placement p 後掛け)
    auto clone = std::dynamic_pointer_cast<const i_ent::ICurve>(res.entity);
    ASSERT_NE(clone, nullptr);
    const double t = 0.5;
    const auto p_clone = clone->TryGetPointAt(t);
    const auto p_via = line->TryGetPointAt(t, p);
    ASSERT_TRUE(p_clone.has_value());
    ASSERT_TRUE(p_via.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*p_clone, *p_via));
}

// 回転配置: 接線は回転のみ適用され並進は乗らない
TEST(MaterializeTest, RotationAppliesToVectorNotTranslation) {
    auto line = MakeLine({0, 0, 0}, {1, 0, 0});
    const Matrix4d p = MakeTransform(kPiHalf, {0, 0, 1}, {10, 20, 30});
    const auto res = i_mdl::Materialize(*line, p);

    auto clone = std::dynamic_pointer_cast<const i_ent::ICurve>(res.entity);
    ASSERT_NE(clone, nullptr);
    const double t = 0.5;
    const auto tang_clone = clone->TryGetTangentAt(t);
    const auto tang_def = line->TryGetTangentAt(t);
    ASSERT_TRUE(tang_clone.has_value());
    ASSERT_TRUE(tang_def.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(
            *tang_clone, i_num::ApplyTransform(p, *tang_def, false)));
}



/**
 * Flatten
 */

// フラット化後は子Assemblyを持たない
TEST(FlattenTest, ProducesFlatTreeWithoutChildAssemblies) {
    i_mdl::IgesData src;
    auto child = MakeAssembly();
    child->SetGlobalTransform(MakeTranslation({10, 0, 0}));
    child->AddEntity(MakeLine({0, 0, 0}, {1, 0, 0}));
    src.Root().AddChildAssembly(child);

    const i_mdl::IgesData out = i_mdl::Flatten(src);
    EXPECT_TRUE(out.Root().GetChildAssemblies().empty());
    EXPECT_GT(out.Root().GetEntityCount(), 0u);
}

// description が複製される
TEST(FlattenTest, CopiesDescription) {
    i_mdl::IgesData src;
    src.description = "P7 flatten description";
    src.Root().AddEntity(MakeLine({0, 0, 0}, {1, 0, 0}));

    const i_mdl::IgesData out = i_mdl::Flatten(src);
    EXPECT_EQ(out.description, src.description);
}

// 畳み込んだワールド点が、元のAssemblyビューのワールド点と一致する (エンドツーエンド不変)
TEST(FlattenTest, FoldedWorldPointMatchesAssemblyView) {
    i_mdl::IgesData src;
    auto child = MakeAssembly();
    child->SetGlobalTransform(MakeTransform(kPiHalf, {0, 0, 1}, {1, 2, 3}));
    auto line = MakeLine({0, 0, 0}, {1, 0, 0});
    const auto line_id = child->AddEntity(line);
    src.Root().AddChildAssembly(child);

    // 元: Assemblyビュー経由のワールド点
    auto view = src.Root().GetCurveView(line_id, CoordFrame::World());
    ASSERT_NE(view, nullptr);
    const double t = 0.5;
    const auto orig = view->TryGetPointAt(t);
    ASSERT_TRUE(orig.has_value());

    // フラット後: 唯一の幾何曲線 (複製Line) のワールド点
    const i_mdl::IgesData out = i_mdl::Flatten(src);
    auto clone = FirstLineCurve(out.Root());
    ASSERT_NE(clone, nullptr);
    const auto folded = clone->TryGetPointAt(t);
    ASSERT_TRUE(folded.has_value());

    EXPECT_TRUE(i_num::IsApproxEqual(*orig, *folded));
}

// 非単位配置では生成124がフラットマップに含まれ、複製がそれを参照する
TEST(FlattenTest, EmitsGeneratedTransformAndClosure) {
    i_mdl::IgesData src;
    auto child = MakeAssembly();
    child->SetGlobalTransform(MakeTranslation({7, 0, 0}));
    child->AddEntity(MakeLine({0, 0, 0}, {1, 0, 0}));
    src.Root().AddChildAssembly(child);

    const i_mdl::IgesData out = i_mdl::Flatten(src);

    // 生成124がちょうど1つ存在する
    EXPECT_EQ(CountType(out.Root(), i_ent::EntityType::kTransformationMatrix), 1u);

    // 複製Lineが、出力マップ内に存在する124を参照する
    std::shared_ptr<i_ent::EntityBase> clone;
    for (const auto& [id, e] : out.Root().GetEntities()) {
        if (e->GetType() == i_ent::EntityType::kLine) clone = e;
    }
    ASSERT_NE(clone, nullptr);
    bool refs_present_transform = false;
    for (const auto& rid : clone->GetReferencedEntityIDs()) {
        auto ref = out.Root().GetEntity(rid);
        if (ref &&
            ref->GetType() == i_ent::EntityType::kTransformationMatrix) {
            refs_present_transform = true;
        }
    }
    EXPECT_TRUE(refs_present_transform);
}



/**
 * ライタのフラット出力 (CollectAllEntities)
 */

// 子Assemblyの所有エンティティも出力される (再帰集約)
TEST(WriterFlatOutputTest, FlattensChildAssemblyEntities) {
    i_mdl::IgesData src;
    src.Root().AddEntity(MakeLine({0, 0, 0}, {1, 0, 0}));
    src.Root().AddEntity(MakeLine({0, 0, 0}, {0, 1, 0}));
    auto child = MakeAssembly();
    child->AddEntity(MakeLine({0, 0, 0}, {0, 0, 1}));
    src.Root().AddChildAssembly(child);

    const std::string tmp =
        (fs::temp_directory_path() / "igesio_p7_flat_output.iges").string();
    ASSERT_TRUE(igesio::WriteIges(src, tmp));

    const i_mdl::IgesData reloaded = igesio::ReadIges(tmp);
    EXPECT_TRUE(reloaded.Root().GetChildAssemblies().empty());
    EXPECT_EQ(CountType(reloaded.Root(), i_ent::EntityType::kLine), 3u);
}
