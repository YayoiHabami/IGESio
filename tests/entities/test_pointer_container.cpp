/**
 * @file entities/test_pointer_container.cpp
 * @brief entities/pointer_container.hのテスト
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 * - 参照解決失敗時のReferenceError送出 (4経路):
 *   ①ポインタ未設定 ②weak_ptr失効 ③格納型と要求型の不一致
 *   ④IEntityIdentifierからの変換失敗 (null/変換不能)
 * - TryGetEntityの非送出 (nullopt) 対比
 * - 正常系: 構築・SetPointer・GetEntity・GetEntityVariant
 *
 * TODO: OverwritePointer等の設定系APIの網羅は対象外
 *       (本ファイルはエラー体系 (ReferenceError) の検証を主目的とする)
 */
#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include "igesio/common/errors.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/pointer_container.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;

/// @brief Lineのみを参照可能なコンテナ (shared_ptr版)
using SharedLineContainer = i_ent::PointerContainer<false, i_ent::Line>;
/// @brief Lineのみを参照可能なコンテナ (weak_ptr版)
using WeakLineContainer = i_ent::PointerContainer<true, i_ent::Line>;
/// @brief LineとCircularArcを参照可能なコンテナ (shared_ptr版)
using SharedCurveContainer =
        i_ent::PointerContainer<false, i_ent::Line, i_ent::CircularArc>;

/// @brief 単位長の線分エンティティを作成する
std::shared_ptr<i_ent::Line> MakeLine() {
    return i_ent::MakeLine(Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0));
}

/// @brief 単位円 (の3/4円弧) エンティティを作成する
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, 1.0));
}

}  // namespace



/**
 * 正常系
 */

// shared_ptrからの構築でポインタが設定され、GetEntityで取得できる
TEST(PointerContainerTest, GetEntity_ReturnsStoredPointer) {
    auto line = MakeLine();
    SharedLineContainer container(line);

    EXPECT_TRUE(container.IsPointerSet());
    EXPECT_EQ(container.GetID(), line->GetID());
    EXPECT_EQ(container.GetEntity<i_ent::Line>(), line);
    EXPECT_NO_THROW(container.GetEntityVariant());
}

// IDのみで構築したコンテナにSetPointerで参照を解決できる
TEST(PointerContainerTest, SetPointer_ResolvesReferenceById) {
    auto line = MakeLine();
    SharedLineContainer container(line->GetID());
    EXPECT_FALSE(container.IsPointerSet());

    EXPECT_TRUE(container.SetPointer(line));
    EXPECT_TRUE(container.IsPointerSet());
    EXPECT_EQ(container.GetEntity<i_ent::Line>(), line);
}

// IDが一致しない場合はSetPointerは失敗する (falseを返し、throwしない)
TEST(PointerContainerTest, SetPointer_ReturnsFalseWhenIdMismatch) {
    auto line1 = MakeLine();
    auto line2 = MakeLine();
    SharedLineContainer container(line1->GetID());

    EXPECT_FALSE(container.SetPointer(line2));
    EXPECT_FALSE(container.IsPointerSet());
}



/**
 * エラー系①: ポインタ未設定
 */

// 未設定のままGetEntityを呼ぶとReferenceErrorを投げる
TEST(PointerContainerTest, GetEntity_ThrowsReferenceErrorWhenPointerNotSet) {
    auto line = MakeLine();
    SharedLineContainer container(line->GetID());

    EXPECT_THROW(container.GetEntity<i_ent::Line>(), igesio::ReferenceError);
    EXPECT_THROW(container.GetEntityVariant(), igesio::ReferenceError);
    EXPECT_THROW(container.GetEntityAsIEntityIdentifier(), igesio::ReferenceError);
}

// 未設定の場合、TryGetEntityはthrowせずnulloptを返す (GetEntityとの対比)
TEST(PointerContainerTest, TryGetEntity_ReturnsNulloptWhenPointerNotSet) {
    auto line = MakeLine();
    SharedLineContainer container(line->GetID());

    EXPECT_EQ(container.TryGetEntity<i_ent::Line>(), std::nullopt);
}



/**
 * エラー系②: weak_ptr失効 (参照先の破棄)
 */

// 参照先のshared_ptrが破棄された後はReferenceErrorを投げる
TEST(PointerContainerTest, GetEntity_ThrowsReferenceErrorWhenReferenceExpired) {
    auto line = MakeLine();
    WeakLineContainer container(line);
    ASSERT_TRUE(container.IsPointerSet());

    line.reset();  // 参照先を破棄してweak_ptrを失効させる

    EXPECT_FALSE(container.IsPointerSet());
    EXPECT_THROW(container.GetEntity<i_ent::Line>(), igesio::ReferenceError);
    EXPECT_THROW(container.GetEntityVariant(), igesio::ReferenceError);
}

// 失効した場合もTryGetEntityはthrowせずnulloptを返す
TEST(PointerContainerTest, TryGetEntity_ReturnsNulloptWhenReferenceExpired) {
    auto line = MakeLine();
    WeakLineContainer container(line);
    line.reset();

    EXPECT_EQ(container.TryGetEntity<i_ent::Line>(), std::nullopt);
}



/**
 * エラー系③: 格納型と要求型の不一致
 */

// Lineを保持するコンテナからCircularArcとして取得しようとすると
// ReferenceErrorを投げる
TEST(PointerContainerTest, GetEntity_ThrowsReferenceErrorWhenTypeMismatch) {
    auto line = MakeLine();
    SharedCurveContainer container(line);
    ASSERT_TRUE(container.IsHoldingType<i_ent::Line>());
    ASSERT_FALSE(container.IsHoldingType<i_ent::CircularArc>());

    EXPECT_THROW(container.GetEntity<i_ent::CircularArc>(), igesio::ReferenceError);
    // 一致する型では取得できる
    EXPECT_EQ(container.GetEntity<i_ent::Line>(), line);
}



/**
 * エラー系④: IEntityIdentifierからの変換失敗
 */

// nullのIEntityIdentifierから構築しようとするとReferenceErrorを投げる
TEST(PointerContainerTest, Constructor_ThrowsReferenceErrorWhenIdentifierIsNull) {
    std::shared_ptr<const i_ent::IEntityIdentifier> null_identifier;

    EXPECT_THROW(SharedLineContainer{null_identifier}, igesio::ReferenceError);
}

// DerivedTypesのいずれにも変換できないIEntityIdentifierから構築しようとすると
// ReferenceErrorを投げる
TEST(PointerContainerTest, Constructor_ThrowsReferenceErrorWhenNoConvertibleType) {
    std::shared_ptr<const i_ent::IEntityIdentifier> arc = MakeArc();

    // CircularArcはLine専用コンテナのDerivedTypesに含まれない
    EXPECT_THROW(SharedLineContainer{arc}, igesio::ReferenceError);
}

// 変換可能な型であればIEntityIdentifier経由でも構築できる (対比)
TEST(PointerContainerTest, Constructor_AcceptsConvertibleIdentifier) {
    auto line = MakeLine();
    std::shared_ptr<const i_ent::IEntityIdentifier> identifier = line;

    SharedLineContainer container(identifier);
    EXPECT_TRUE(container.IsPointerSet());
    EXPECT_EQ(container.GetEntity<i_ent::Line>(), line);
}
