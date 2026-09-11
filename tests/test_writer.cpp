/**
 * @file test_writer.cpp
 * @brief writer.hのテスト
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/factory.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/transformations/transformation_matrix.h"
#include "igesio/models/iges_data.h"
#include "igesio/reader.h"
#include "igesio/writer.h"

namespace {

namespace iio = igesio;
namespace i_ent = igesio::entities;
namespace i_num = igesio::numerics;
namespace fs = std::filesystem;
using igesio::Vector2d;
using igesio::Vector3d;

/// @brief 浮動小数点比較の許容誤差 (メモリ上の展開)
constexpr double kTol = 1e-9;
/// @brief ファイル往復の許容誤差 (IGESの実数表記を経由するため緩める)
constexpr double kFileTol = 1e-6;

/// @brief 指定タイプのDEレコードの添字を返す (無ければnullopt)
/// @param des DEセクション
/// @param type 探すエンティティタイプ
std::optional<size_t> FindDeIndex(
        const std::vector<i_ent::RawEntityDE>& des, const i_ent::EntityType type) {
    for (size_t i = 0; i < des.size(); ++i) {
        if (des[i].entity_type == type) return i;
    }
    return std::nullopt;
}

/// @brief 展開テスト用の時計回り弧 (中心 (1,2)、z_t = 0.5、始点(2,2) → 終点(1,3))
std::shared_ptr<i_ent::CircularArc> MakeClockwiseArc() {
    return i_ent::MakeCircularArc(
            Vector2d(1.0, 2.0), Vector2d(2.0, 2.0), Vector2d(1.0, 3.0), 0.5, true);
}

/// @brief テスト用IGESファイルを格納するディレクトリ
const std::string kTestIgesDirPath =
        fs::path(__FILE__).parent_path().append("test_data").string();
/// @brief テスト用IGESファイルのパス (立方体の一辺が丸められたもの)
const std::string kSingleRoundCubePath =
        fs::path(kTestIgesDirPath).append("single_rounded_cube.iges").string();
/// @brief テストによる出力フォルダ
const std::string kOutputDirPath =
        fs::path(kTestIgesDirPath).append("output").string();

}  // namespace


/*******************************************************************************
 * WriteIgesIntermediateのテスト
 ******************************************************************************/

TEST(WriteIgesIntermediateTest, NormalCase) {
    // まずは読み込み
    auto data = iio::ReadIgesIntermediate(kSingleRoundCubePath);

    // 書き込み先のファイルパス
    const std::string output_path =
            fs::path(kOutputDirPath).append("single_rounded_cube_copied.iges").string();

    // 書き込みを実行
    ASSERT_NO_THROW(iio::WriteIgesIntermediate(data, output_path));

    // 書き込んだファイルが存在することを確認
    ASSERT_TRUE(fs::exists(output_path));
}



/*******************************************************************************
 * ユーザー定義エンティティ (kUserDefined) の往復テスト
 ******************************************************************************/

// ユーザー定義番号 (602) のエンティティがWriteIges→ReadIgesで往復すること.
// creator未登録のためUnsupportedEntityとして保持され、ファイルへは
// 実番号 (602) が出力される
TEST(WriteIgesTest, UserDefinedEntityRoundTrip) {
    iio::models::IgesData data;
    auto entity = iio::entities::EntityFactory::CreateEntity(
            iio::entities::RawEntityDE::ByDefaultUserDefined(602),
            iio::IGESParameterVector{1.0, 2.0, 3.0}, {});
    data.Root().AddEntity(entity);

    const std::string output_path =
            fs::path(kOutputDirPath).append("user_defined_roundtrip.iges").string();
    // UnsupportedEntityを含むため、save_unsupported = true で書き出す
    ASSERT_NO_THROW(iio::WriteIges(data, output_path, true));
    ASSERT_TRUE(fs::exists(output_path));

    // 読み戻して実番号が保持されていることを確認
    auto read = iio::ReadIges(output_path);
    const auto& entities = read.Root().GetEntities();
    ASSERT_EQ(entities.size(), 1u);
    // GetTypeNumberはEntityBaseのAPIのため、型を指定して取得する
    const auto read_entity = read.Root().GetEntityAs<iio::entities::EntityBase>(
            entities.begin()->first);
    ASSERT_NE(read_entity, nullptr);
    EXPECT_EQ(read_entity->GetType(), iio::entities::EntityType::kUserDefined);
    EXPECT_EQ(read_entity->GetTypeNumber(), 602);
    EXPECT_FALSE(read_entity->IsSupported());
}



/*******************************************************************************
 * 時計回り円弧の出力時展開 (ExpandForExport → 鏡映CCW弧 + Type 124)
 ******************************************************************************/

// CW弧1つのモデルはDE 2件 (Type 100・Type 124) に展開され、Type 100のDE7が
// Type 124を指し、PDのy成分が鏡映される
TEST(WriteIgesTest, ClockwiseArc_ExpandsToCcwArcAndMatrix) {
    iio::models::IgesData data;
    const auto arc = MakeClockwiseArc();
    data.Root().AddEntity(arc);

    const auto intermediate = iio::ConvertToIntermediate(data);
    const auto& des = intermediate.directory_entry_section;
    const auto& pds = intermediate.parameter_data_section;
    ASSERT_EQ(des.size(), 2u);
    ASSERT_EQ(pds.size(), 2u);

    const auto arc_index = FindDeIndex(des, i_ent::EntityType::kCircularArc);
    const auto tm_index = FindDeIndex(des, i_ent::EntityType::kTransformationMatrix);
    ASSERT_TRUE(arc_index.has_value());
    ASSERT_TRUE(tm_index.has_value());
    EXPECT_EQ(des[*arc_index].transformation_matrix,
              static_cast<int>(des[*tm_index].sequence_number));
    EXPECT_EQ(des[*tm_index].form_number, 0);
    EXPECT_EQ(des[*tm_index].status.subordinate_entity_switch,
              i_ent::SubordinateEntitySwitch::kIndependent);

    // PD: {zt, xc, yc, xs, 2yc−ys, xt, 2yc−yt} = {0.5, 1, 2, 2, 2, 1, 1}
    const auto& arc_pd = pds[*arc_index].data;
    ASSERT_EQ(arc_pd.size(), 7u);
    const std::vector<double> expected{0.5, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(std::stod(arc_pd[i]), expected[i], kTol) << "i = " << i;
    }

    // モデル本体は変更されない
    EXPECT_TRUE(arc->IsClockwise());
    EXPECT_EQ(data.Root().GetEntities().size(), 1u);
    EXPECT_EQ(arc->GetTransformationMatrix().GetValueType(),
              i_ent::DEFieldValueType::kDefault);
}

// CCW弧のみのモデルは展開されず、DE/PDが元のまま
TEST(WriteIgesTest, CcwOnlyModel_IsUnchanged) {
    iio::models::IgesData data;
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(1.0, 2.0), Vector2d(2.0, 2.0), Vector2d(1.0, 3.0), 0.5);
    data.Root().AddEntity(arc);

    const auto intermediate = iio::ConvertToIntermediate(data);
    const auto& des = intermediate.directory_entry_section;
    const auto& pds = intermediate.parameter_data_section;
    ASSERT_EQ(des.size(), 1u);
    ASSERT_EQ(pds.size(), 1u);
    EXPECT_EQ(des[0].entity_type, i_ent::EntityType::kCircularArc);
    EXPECT_EQ(des[0].transformation_matrix, 0);

    const std::vector<double> expected{0.5, 1.0, 2.0, 2.0, 2.0, 1.0, 3.0};
    ASSERT_EQ(pds[0].data.size(), 7u);
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(std::stod(pds[0].data[i]), expected[i], kTol) << "i = " << i;
    }
}

// Write → Read で復元した弧 (CCW + Type 124) のモデル空間の位置・接線が
// 元のCW弧と各パラメータで一致する (元の弧がM0を参照する場合も含む)
TEST(WriteIgesTest, ClockwiseArc_RoundTripPreservesGeometry) {
    for (const bool with_m0 : {false, true}) {
        SCOPED_TRACE(with_m0 ? "with M0" : "without M0");
        iio::models::IgesData data;
        const auto arc = MakeClockwiseArc();
        if (with_m0) {
            const auto m0 = i_ent::MakeRotation(
                    iio::kPi / 3.0, Vector3d(0.0, 0.0, 1.0), Vector3d(1.0, 1.0, 1.0));
            data.Root().AddEntity(m0);
            ASSERT_TRUE(arc->OverwriteTransformationMatrix(m0));
        }
        data.Root().AddEntity(arc);

        const std::string output_path = fs::path(kOutputDirPath).append(
                with_m0 ? "clockwise_arc_roundtrip_m0.iges"
                        : "clockwise_arc_roundtrip.iges").string();
        ASSERT_NO_THROW(iio::WriteIges(data, output_path));
        ASSERT_TRUE(fs::exists(output_path));

        const auto read = iio::ReadIges(output_path);
        const auto found = read.Root().FindEntitiesByType(
                i_ent::EntityType::kCircularArc, true);
        ASSERT_EQ(found.size(), 1u);
        const auto read_arc = read.Root().GetEntityAs<i_ent::CircularArc>(
                found[0]->GetID());
        ASSERT_NE(read_arc, nullptr);
        // ファイル上は常にCCW弧で、DE7の変換行列 (鏡映) が解決されている
        EXPECT_FALSE(read_arc->IsClockwise());
        ASSERT_NE(read_arc->GetTransformationMatrix().GetPointer(), nullptr);
        if (with_m0) {
            // 鏡映行列がさらにM0へ連鎖する
            EXPECT_NE(read_arc->GetTransformationMatrix().GetPointer()
                              ->GetRefTransformation(), nullptr);
        }

        const auto range = arc->GetParameterRange();
        const auto range_r = read_arc->GetParameterRange();
        const double sweep = range[1] - range[0];
        EXPECT_NEAR(range_r[1] - range_r[0], sweep, kFileTol);
        for (const double u : {0.0, sweep / 3.0, sweep}) {
            const auto original = arc->TryGetDerivatives(range[0] + u, 1);
            const auto restored = read_arc->TryGetDerivatives(range_r[0] + u, 1);
            ASSERT_TRUE(original.has_value());
            ASSERT_TRUE(restored.has_value());
            EXPECT_TRUE(i_num::IsApproxEqual(
                    restored.value()[0], original.value()[0], kFileTol))
                    << "u = " << u;
            EXPECT_TRUE(i_num::IsApproxEqual(
                    restored.value()[1], original.value()[1], kFileTol))
                    << "u = " << u;
        }
    }
}

// Type 102の子がCW弧でも、出力後のType 102のPDは同じDE枠 (置換弧) を指す
TEST(WriteIgesTest, CompositeCurve_WithClockwiseChildKeepsReference) {
    iio::models::IgesData data;
    const auto line = i_ent::MakeLine(Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0));
    // 中心 (1,1)、始点 (1,0) → 終点 (0,1) を時計回り (原点側へ膨らむ弧)
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(1.0, 1.0), Vector2d(1.0, 0.0), Vector2d(0.0, 1.0), 0.0, true);
    const auto composite = i_ent::MakeCompositeCurve({line, arc});
    data.Root().AddEntity(line);
    data.Root().AddEntity(arc);
    data.Root().AddEntity(composite);

    const auto intermediate = iio::ConvertToIntermediate(data);
    const auto& des = intermediate.directory_entry_section;
    const auto& pds = intermediate.parameter_data_section;
    ASSERT_EQ(des.size(), 4u);

    const auto arc_index = FindDeIndex(des, i_ent::EntityType::kCircularArc);
    const auto composite_index = FindDeIndex(des, i_ent::EntityType::kCompositeCurve);
    const auto tm_index = FindDeIndex(des, i_ent::EntityType::kTransformationMatrix);
    ASSERT_TRUE(arc_index.has_value());
    ASSERT_TRUE(composite_index.has_value());
    ASSERT_TRUE(tm_index.has_value());

    // Type 102のPD ({N, ptr...}) が置換弧のDE枠を指す
    const std::string arc_pointer = std::to_string(des[*arc_index].sequence_number);
    const auto& composite_pd = pds[*composite_index].data;
    EXPECT_NE(std::find(composite_pd.begin(), composite_pd.end(), arc_pointer),
              composite_pd.end());
    // 置換弧はType 124を指し、始終点のy成分が鏡映される (start (1,2)、end (0,1))
    EXPECT_EQ(des[*arc_index].transformation_matrix,
              static_cast<int>(des[*tm_index].sequence_number));
    const auto& arc_pd = pds[*arc_index].data;
    ASSERT_EQ(arc_pd.size(), 7u);
    EXPECT_NEAR(std::stod(arc_pd[3]), 1.0, kTol);
    EXPECT_NEAR(std::stod(arc_pd[4]), 2.0, kTol);
    EXPECT_NEAR(std::stod(arc_pd[5]), 0.0, kTol);
    EXPECT_NEAR(std::stod(arc_pd[6]), 1.0, kTol);
}
