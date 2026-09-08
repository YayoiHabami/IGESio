/**
 * @file tests/extensions/animation/test_animation_clip.cpp
 * @brief AnimationClip (extensions/animation) のテスト
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 * @note 対象は以下の振る舞い:
 *       - 正常系 (代表値): AddKeyの時刻昇順挿入・ターゲット毎のトラック集約、
 *         ActiveKeyIndexのステップ意味論 (半開区間・左閉境界)、
 *         Durationの既定値 (最大キー時刻) と明示設定
 *       - 正常系 (境界値): 時刻0のキー、キー時刻ちょうどの検索、
 *         明示durationちょうどのキー追加
 *       - 正常系 (退化): 空トラック・空クリップ
 *       - 異常系: 負時刻・同時刻重複・非剛体変換・duration違反
 *       - IsRigidTransform: 回転+並進の受理、スケール・射影成分の拒否
 *       - 可視性トラック: AddVisibilityKeyの整列・集約・同値連続の受理、
 *         ActiveKeyIndexのステップ意味論、異常系
 *       - イベントトラック: AddEvent/FindEventTrack、ActiveEventValue、
 *         EventKeysBetweenの前進 (左開右閉)・後退・同時刻・範囲外、異常系
 *       - Duration: 3種のキーの最大時刻の共有と下限検査
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/animation/animation_clip.h"

namespace {

namespace anim = igesio::extensions::animation;
using igesio::Matrix4d;
using igesio::ObjectID;
using igesio::IDGenerator;
using igesio::ObjectType;

/// @brief 平行移動行列を作成する
/// @param x, y, z 平行移動量
/// @return 平行移動のみの剛体変換行列
Matrix4d MakeTranslation(const double x, const double y, const double z) {
    Matrix4d m = Matrix4d::Identity();
    m(0, 3) = x;
    m(1, 3) = y;
    m(2, 3) = z;
    return m;
}

/// @brief Z軸回りの回転行列を作成する
/// @param angle_rad 回転角 [rad]
/// @return 回転のみの剛体変換行列
Matrix4d MakeZRotation(const double angle_rad) {
    Matrix4d m = Matrix4d::Identity();
    const double c = std::cos(angle_rad), s = std::sin(angle_rad);
    m(0, 0) = c;
    m(0, 1) = -s;
    m(1, 0) = s;
    m(1, 1) = c;
    return m;
}

/// @brief テスト用のターゲットIDを生成する
ObjectID MakeTargetId() {
    return IDGenerator::Generate(ObjectType::kAssembly);
}



/**
 * AddKey (正常系)
 */

TEST(AnimationClipTest, AddKey_StoresSortedByTime) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    // 逆順・混在順で追加しても時刻昇順に整列されること
    clip.AddKey(id, 2.0, MakeTranslation(2.0, 0.0, 0.0));
    clip.AddKey(id, 0.5, MakeTranslation(1.0, 0.0, 0.0));
    clip.AddKey(id, 2.1, MakeTranslation(3.0, 0.0, 0.0));

    ASSERT_EQ(clip.Tracks().size(), 1u);
    const auto& keys = clip.Tracks()[0].keys;
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_DOUBLE_EQ(keys[0].time_sec, 0.5);
    EXPECT_DOUBLE_EQ(keys[1].time_sec, 2.0);
    EXPECT_DOUBLE_EQ(keys[2].time_sec, 2.1);
    // 時刻に対応する変換が保持されていること
    EXPECT_DOUBLE_EQ(keys[0].transform(0, 3), 1.0);
    EXPECT_DOUBLE_EQ(keys[1].transform(0, 3), 2.0);
    EXPECT_DOUBLE_EQ(keys[2].transform(0, 3), 3.0);
}

TEST(AnimationClipTest, AddKey_MergesTracksByTarget) {
    anim::AnimationClip clip;
    const auto id_a = MakeTargetId();
    const auto id_b = MakeTargetId();
    clip.AddKey(id_a, 0.5, MakeTranslation(1.0, 0.0, 0.0));
    clip.AddKey(id_b, 0.5, MakeTranslation(1.0, 0.0, 0.0));
    clip.AddKey(id_a, 2.0, MakeTranslation(2.0, 0.0, 0.0));

    ASSERT_EQ(clip.Tracks().size(), 2u);
    EXPECT_EQ(clip.Tracks()[0].target, id_a);
    EXPECT_EQ(clip.Tracks()[0].keys.size(), 2u);
    EXPECT_EQ(clip.Tracks()[1].target, id_b);
    EXPECT_EQ(clip.Tracks()[1].keys.size(), 1u);
}

TEST(AnimationClipTest, AddKey_AcceptsTimeZero) {
    anim::AnimationClip clip;
    EXPECT_NO_THROW(clip.AddKey(MakeTargetId(), 0.0, Matrix4d::Identity()));
}



/**
 * ActiveKeyIndex (ステップ意味論)
 */

TEST(AnimationClipTest, ActiveKeyIndex_StepSemantics) {
    // 設計の提示例: (0.5, T1), (2, T2), (2.1, T3)
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    clip.AddKey(id, 0.5, MakeTranslation(1.0, 0.0, 0.0));
    clip.AddKey(id, 2.0, MakeTranslation(2.0, 0.0, 0.0));
    clip.AddKey(id, 2.1, MakeTranslation(3.0, 0.0, 0.0));
    const auto& track = clip.Tracks()[0];

    // 先頭キーより前は基準姿勢 (nullopt)
    EXPECT_EQ(anim::ActiveKeyIndex(track, 0.0), std::nullopt);
    EXPECT_EQ(anim::ActiveKeyIndex(track, 0.49), std::nullopt);
    // キー時刻ちょうどでそのキーが有効になる (左閉)
    EXPECT_EQ(anim::ActiveKeyIndex(track, 0.5), std::optional<std::size_t>(0));
    // 次のキーの直前まで有効 (半開区間)
    EXPECT_EQ(anim::ActiveKeyIndex(track, 1.99), std::optional<std::size_t>(0));
    EXPECT_EQ(anim::ActiveKeyIndex(track, 2.0), std::optional<std::size_t>(1));
    EXPECT_EQ(anim::ActiveKeyIndex(track, 2.05), std::optional<std::size_t>(1));
    EXPECT_EQ(anim::ActiveKeyIndex(track, 2.1), std::optional<std::size_t>(2));
    // 最終キーは終端まで保持
    EXPECT_EQ(anim::ActiveKeyIndex(track, 100.0), std::optional<std::size_t>(2));
}

TEST(AnimationClipTest, ActiveKeyIndex_EmptyTrackReturnsNullopt) {
    const anim::AnimationTrack track{{}, MakeTargetId()};
    EXPECT_EQ(anim::ActiveKeyIndex(track, 0.0), std::nullopt);
    EXPECT_EQ(anim::ActiveKeyIndex(track, 10.0), std::nullopt);
}



/**
 * Duration
 */

TEST(AnimationClipTest, Duration_DefaultsToMaxKeyTime) {
    anim::AnimationClip clip;
    const auto id_a = MakeTargetId();
    const auto id_b = MakeTargetId();
    clip.AddKey(id_a, 2.1, Matrix4d::Identity());
    clip.AddKey(id_b, 3.0, Matrix4d::Identity());
    EXPECT_DOUBLE_EQ(clip.Duration(), 3.0);
}

TEST(AnimationClipTest, Duration_ExplicitOverride) {
    anim::AnimationClip clip;
    clip.AddKey(MakeTargetId(), 3.0, Matrix4d::Identity());
    clip.SetDuration(5.0);
    EXPECT_DOUBLE_EQ(clip.Duration(), 5.0);
}

TEST(AnimationClipTest, Duration_EmptyClipIsZero) {
    const anim::AnimationClip clip;
    EXPECT_DOUBLE_EQ(clip.Duration(), 0.0);
    EXPECT_TRUE(clip.Tracks().empty());
}



/**
 * AddKey / SetDuration (異常系)
 */

TEST(AnimationClipTest, AddKey_ThrowsInvalidArgumentWhenTimeNegative) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    // 境界: 0.0は受理、その直外は拒否
    EXPECT_THROW(clip.AddKey(id, -1e-9, Matrix4d::Identity()),
                 std::invalid_argument);
    EXPECT_NO_THROW(clip.AddKey(id, 0.0, Matrix4d::Identity()));
}

TEST(AnimationClipTest, AddKey_ThrowsInvalidArgumentWhenDuplicateTime) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    clip.AddKey(id, 0.5, Matrix4d::Identity());
    EXPECT_THROW(clip.AddKey(id, 0.5, MakeTranslation(1.0, 0.0, 0.0)),
                 std::invalid_argument);
    // 別ターゲットの同時刻は受理されること
    EXPECT_NO_THROW(clip.AddKey(MakeTargetId(), 0.5, Matrix4d::Identity()));
}

TEST(AnimationClipTest, AddKey_ThrowsInvalidArgumentWhenTransformNotRigid) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    // スケール成分を含む行列は拒否
    Matrix4d scaled = Matrix4d::Identity();
    scaled(0, 0) = 2.0;
    EXPECT_THROW(clip.AddKey(id, 0.0, scaled), std::invalid_argument);
    // 回転+並進の合成は受理
    EXPECT_NO_THROW(clip.AddKey(
            id, 0.0, MakeZRotation(0.7) * MakeTranslation(1.0, 2.0, 3.0)));
}

TEST(AnimationClipTest, AddKey_ThrowsInvalidArgumentWhenBeyondExplicitDuration) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    clip.SetDuration(5.0);
    // 境界: durationちょうどは受理、その直外は拒否
    EXPECT_NO_THROW(clip.AddKey(id, 5.0, Matrix4d::Identity()));
    EXPECT_THROW(clip.AddKey(id, 5.0 + 1e-9, Matrix4d::Identity()),
                 std::invalid_argument);
}

TEST(AnimationClipTest, SetDuration_ThrowsInvalidArgumentWhenLessThanLastKey) {
    anim::AnimationClip clip;
    clip.AddKey(MakeTargetId(), 3.0, Matrix4d::Identity());
    // 境界: 最大キー時刻ちょうどは受理、その直外は拒否
    EXPECT_NO_THROW(clip.SetDuration(3.0));
    EXPECT_THROW(clip.SetDuration(3.0 - 1e-9), std::invalid_argument);
}

TEST(AnimationClipTest, SetDuration_ThrowsInvalidArgumentWhenNegative) {
    anim::AnimationClip clip;
    EXPECT_THROW(clip.SetDuration(-1e-9), std::invalid_argument);
    EXPECT_NO_THROW(clip.SetDuration(0.0));
}



/**
 * IsRigidTransform
 */

TEST(AnimationClipTest, IsRigidTransform_AcceptsRotationAndTranslation) {
    EXPECT_TRUE(anim::IsRigidTransform(Matrix4d::Identity()));
    EXPECT_TRUE(anim::IsRigidTransform(MakeTranslation(10.0, -5.0, 3.0)));
    EXPECT_TRUE(anim::IsRigidTransform(
            MakeZRotation(1.2) * MakeTranslation(1.0, 2.0, 3.0)));
}

TEST(AnimationClipTest, IsRigidTransform_RejectsScaleAndProjection) {
    // スケール
    Matrix4d scaled = Matrix4d::Identity();
    scaled(1, 1) = 0.5;
    EXPECT_FALSE(anim::IsRigidTransform(scaled));
    // 射影成分 (最下行の摂動)
    Matrix4d projective = Matrix4d::Identity();
    projective(3, 0) = 0.1;
    EXPECT_FALSE(anim::IsRigidTransform(projective));
    // 鏡映 (行列式-1) は回転でないため拒否
    Matrix4d mirrored = Matrix4d::Identity();
    mirrored(2, 2) = -1.0;
    EXPECT_FALSE(anim::IsRigidTransform(mirrored));
}



/**
 * AddVisibilityKey (正常系)
 */

TEST(AnimationClipTest, AddVisibilityKey_StoresSortedByTime) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    // 逆順・混在順で追加しても時刻昇順に整列されること
    clip.AddVisibilityKey(id, 2.0, true);
    clip.AddVisibilityKey(id, 0.5, false);
    clip.AddVisibilityKey(id, 2.1, false);

    ASSERT_EQ(clip.VisibilityTracks().size(), 1u);
    const auto& keys = clip.VisibilityTracks()[0].keys;
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_DOUBLE_EQ(keys[0].time_sec, 0.5);
    EXPECT_FALSE(keys[0].visible);
    EXPECT_DOUBLE_EQ(keys[1].time_sec, 2.0);
    EXPECT_TRUE(keys[1].visible);
    EXPECT_DOUBLE_EQ(keys[2].time_sec, 2.1);
    EXPECT_FALSE(keys[2].visible);
    // 変換トラックには影響しないこと
    EXPECT_TRUE(clip.Tracks().empty());
}

TEST(AnimationClipTest, AddVisibilityKey_MergesTracksByTarget) {
    anim::AnimationClip clip;
    const auto id_a = MakeTargetId();
    const auto id_b = MakeTargetId();
    clip.AddVisibilityKey(id_a, 0.5, false);
    clip.AddVisibilityKey(id_b, 0.5, false);
    clip.AddVisibilityKey(id_a, 2.0, true);

    ASSERT_EQ(clip.VisibilityTracks().size(), 2u);
    EXPECT_EQ(clip.VisibilityTracks()[0].target, id_a);
    EXPECT_EQ(clip.VisibilityTracks()[0].keys.size(), 2u);
    EXPECT_EQ(clip.VisibilityTracks()[1].target, id_b);
    EXPECT_EQ(clip.VisibilityTracks()[1].keys.size(), 1u);

    // 同じターゲットの変換トラックと可視性トラックは独立に保持されること
    clip.AddKey(id_a, 1.0, Matrix4d::Identity());
    ASSERT_EQ(clip.Tracks().size(), 1u);
    EXPECT_EQ(clip.Tracks()[0].keys.size(), 1u);
    EXPECT_EQ(clip.VisibilityTracks()[0].keys.size(), 2u);
}

TEST(AnimationClipTest, AddVisibilityKey_AcceptsTimeZero) {
    anim::AnimationClip clip;
    EXPECT_NO_THROW(clip.AddVisibilityKey(MakeTargetId(), 0.0, false));
}

TEST(AnimationClipTest, AddVisibilityKey_AllowsRepeatedValue) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    // 連続する同値キーは拒否しない (生成側で省くかは任意)
    clip.AddVisibilityKey(id, 1.0, true);
    EXPECT_NO_THROW(clip.AddVisibilityKey(id, 2.0, true));
    EXPECT_EQ(clip.VisibilityTracks()[0].keys.size(), 2u);
}

TEST(AnimationClipTest, ActiveKeyIndex_VisibilityTrack_StepSemantics) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    clip.AddVisibilityKey(id, 0.5, false);
    clip.AddVisibilityKey(id, 2.0, true);
    const auto& track = clip.VisibilityTracks()[0];

    // 先頭キーより前は基準可視性 (nullopt)
    EXPECT_EQ(anim::ActiveKeyIndex(track, 0.0), std::nullopt);
    EXPECT_EQ(anim::ActiveKeyIndex(track, 0.49), std::nullopt);
    // キー時刻ちょうどでそのキーが有効になる (左閉)
    EXPECT_EQ(anim::ActiveKeyIndex(track, 0.5), std::optional<std::size_t>(0));
    EXPECT_EQ(anim::ActiveKeyIndex(track, 1.99), std::optional<std::size_t>(0));
    EXPECT_EQ(anim::ActiveKeyIndex(track, 2.0), std::optional<std::size_t>(1));
    // 最終キーは終端まで保持
    EXPECT_EQ(anim::ActiveKeyIndex(track, 100.0), std::optional<std::size_t>(1));
    // 空トラック
    const anim::VisibilityTrack empty{{}, MakeTargetId()};
    EXPECT_EQ(anim::ActiveKeyIndex(empty, 1.0), std::nullopt);
}



/**
 * AddVisibilityKey (異常系)
 */

TEST(AnimationClipTest, AddVisibilityKey_ThrowsInvalidArgumentWhenTimeNegative) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    EXPECT_THROW(clip.AddVisibilityKey(id, -1e-9, true), std::invalid_argument);
    EXPECT_NO_THROW(clip.AddVisibilityKey(id, 0.0, true));
}

TEST(AnimationClipTest, AddVisibilityKey_ThrowsInvalidArgumentWhenDuplicateTime) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    clip.AddVisibilityKey(id, 0.5, true);
    EXPECT_THROW(clip.AddVisibilityKey(id, 0.5, false), std::invalid_argument);
    // 別ターゲットの同時刻は受理されること
    EXPECT_NO_THROW(clip.AddVisibilityKey(MakeTargetId(), 0.5, false));
    // 同ターゲットの変換キーと同時刻でも、トラック種が異なれば受理されること
    EXPECT_NO_THROW(clip.AddKey(id, 0.5, Matrix4d::Identity()));
}

TEST(AnimationClipTest,
     AddVisibilityKey_ThrowsInvalidArgumentWhenBeyondExplicitDuration) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    clip.SetDuration(5.0);
    EXPECT_NO_THROW(clip.AddVisibilityKey(id, 5.0, false));
    EXPECT_THROW(clip.AddVisibilityKey(id, 5.0 + 1e-9, false),
                 std::invalid_argument);
}



/**
 * AddEvent / FindEventTrack (正常系)
 */

TEST(AnimationClipTest, AddEvent_StoresSortedByTime) {
    anim::AnimationClip clip;
    clip.AddEvent("block", 2.0, 20);
    clip.AddEvent("block", 0.5, 5);
    clip.AddEvent("block", 2.1, 21);

    ASSERT_EQ(clip.EventTracks().size(), 1u);
    EXPECT_EQ(clip.EventTracks()[0].name, "block");
    const auto& keys = clip.EventTracks()[0].keys;
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_DOUBLE_EQ(keys[0].time_sec, 0.5);
    EXPECT_EQ(keys[0].value, 5);
    EXPECT_DOUBLE_EQ(keys[1].time_sec, 2.0);
    EXPECT_EQ(keys[1].value, 20);
    EXPECT_DOUBLE_EQ(keys[2].time_sec, 2.1);
    EXPECT_EQ(keys[2].value, 21);
}

TEST(AnimationClipTest, AddEvent_MergesTracksByName) {
    anim::AnimationClip clip;
    clip.AddEvent("tool", 0.0, 1);
    clip.AddEvent("block", 0.0, 0);
    clip.AddEvent("tool", 3.0, 2);

    ASSERT_EQ(clip.EventTracks().size(), 2u);
    EXPECT_EQ(clip.EventTracks()[0].name, "tool");
    EXPECT_EQ(clip.EventTracks()[0].keys.size(), 2u);
    EXPECT_EQ(clip.EventTracks()[1].name, "block");
    EXPECT_EQ(clip.EventTracks()[1].keys.size(), 1u);
}

TEST(AnimationClipTest, FindEventTrack_ReturnsTrackByName) {
    anim::AnimationClip clip;
    clip.AddEvent("tool", 0.0, 1);
    clip.AddEvent("block", 0.0, 0);
    const auto* track = clip.FindEventTrack("block");
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(track->name, "block");
    EXPECT_EQ(track, &clip.EventTracks()[1]);
}

TEST(AnimationClipTest, FindEventTrack_ReturnsNullptrWhenMissing) {
    anim::AnimationClip clip;
    EXPECT_EQ(clip.FindEventTrack("block"), nullptr);
    clip.AddEvent("tool", 0.0, 1);
    EXPECT_EQ(clip.FindEventTrack("block"), nullptr);
    EXPECT_EQ(clip.FindEventTrack(""), nullptr);
}

TEST(AnimationClipTest, ActiveEventValue_StepSemantics) {
    anim::AnimationClip clip;
    clip.AddEvent("stage", 0.5, 1);
    clip.AddEvent("stage", 2.0, 2);
    const auto& track = clip.EventTracks()[0];

    // 先頭キーより前は値なし
    EXPECT_EQ(anim::ActiveEventValue(track, 0.0), std::nullopt);
    EXPECT_EQ(anim::ActiveEventValue(track, 0.49), std::nullopt);
    // 左閉・半開区間
    EXPECT_EQ(anim::ActiveEventValue(track, 0.5), std::optional<std::int64_t>(1));
    EXPECT_EQ(anim::ActiveEventValue(track, 1.99), std::optional<std::int64_t>(1));
    EXPECT_EQ(anim::ActiveEventValue(track, 2.0), std::optional<std::int64_t>(2));
    EXPECT_EQ(anim::ActiveEventValue(track, 100.0), std::optional<std::int64_t>(2));
    // ActiveKeyIndexとの整合
    EXPECT_EQ(anim::ActiveKeyIndex(track, 1.0), std::optional<std::size_t>(0));
    // 空トラック
    const anim::EventTrack empty{{}, "empty"};
    EXPECT_EQ(anim::ActiveEventValue(empty, 1.0), std::nullopt);
    EXPECT_EQ(anim::ActiveKeyIndex(empty, 1.0), std::nullopt);
}



/**
 * EventKeysBetween
 */

TEST(AnimationClipTest, EventKeysBetween_ForwardExcludesFromIncludesTo) {
    anim::AnimationClip clip;
    clip.AddEvent("s", 1.0, 1);
    clip.AddEvent("s", 2.0, 2);
    clip.AddEvent("s", 3.0, 3);
    const auto& track = clip.EventTracks()[0];
    using Range = std::pair<std::size_t, std::size_t>;

    // from=1.0で既に有効なキー0は含まず、to=3.0ちょうどのキー2は含む
    EXPECT_EQ(anim::EventKeysBetween(track, 1.0, 3.0), Range(1, 3));
    // 先頭キーより前から始めると、先頭キーを含む
    EXPECT_EQ(anim::EventKeysBetween(track, 0.0, 1.0), Range(0, 1));
    // 区間内にキー1つ
    EXPECT_EQ(anim::EventKeysBetween(track, 1.5, 2.5), Range(1, 2));
    // 全キーを跨ぐ
    EXPECT_EQ(anim::EventKeysBetween(track, 0.0, 10.0), Range(0, 3));
}

TEST(AnimationClipTest, EventKeysBetween_BackwardReturnsUndoneKeys) {
    anim::AnimationClip clip;
    clip.AddEvent("s", 1.0, 1);
    clip.AddEvent("s", 2.0, 2);
    clip.AddEvent("s", 3.0, 3);
    const auto& track = clip.EventTracks()[0];
    using Range = std::pair<std::size_t, std::size_t>;

    // 3.0→1.0: to < t_k <= from を満たすキー1, 2 (昇順)
    EXPECT_EQ(anim::EventKeysBetween(track, 3.0, 1.0), Range(1, 3));
    // 2.5→0.0: 有効だったキー0, 1が無効になる (3.0はfrom時点で未到達のため含まない)
    EXPECT_EQ(anim::EventKeysBetween(track, 2.5, 0.0), Range(0, 2));
    // 10.0→0.0: 全キーが無効になる
    EXPECT_EQ(anim::EventKeysBetween(track, 10.0, 0.0), Range(0, 3));
    // 1.5→1.0: キー1.0はtoで依然有効なため含まない
    EXPECT_EQ(anim::EventKeysBetween(track, 1.5, 1.0), Range(1, 1));
}

TEST(AnimationClipTest, EventKeysBetween_SameTimeIsEmpty) {
    anim::AnimationClip clip;
    clip.AddEvent("s", 1.0, 1);
    clip.AddEvent("s", 2.0, 2);
    const auto& track = clip.EventTracks()[0];
    const auto r1 = anim::EventKeysBetween(track, 1.0, 1.0);
    EXPECT_EQ(r1.first, r1.second);
    const auto r2 = anim::EventKeysBetween(track, 1.5, 1.5);
    EXPECT_EQ(r2.first, r2.second);
}

TEST(AnimationClipTest, EventKeysBetween_NoKeysInRange) {
    anim::AnimationClip clip;
    clip.AddEvent("s", 1.0, 1);
    clip.AddEvent("s", 2.0, 2);
    const auto& track = clip.EventTracks()[0];
    using Range = std::pair<std::size_t, std::size_t>;

    // キーの間の区間・最終キー以降・先頭キー以前
    EXPECT_EQ(anim::EventKeysBetween(track, 1.2, 1.8), Range(1, 1));
    EXPECT_EQ(anim::EventKeysBetween(track, 2.5, 9.0), Range(2, 2));
    EXPECT_EQ(anim::EventKeysBetween(track, 0.0, 0.5), Range(0, 0));
    // 空トラック
    const anim::EventTrack empty{{}, "empty"};
    EXPECT_EQ(anim::EventKeysBetween(empty, 0.0, 10.0), Range(0, 0));
}



/**
 * AddEvent (異常系)
 */

TEST(AnimationClipTest, AddEvent_ThrowsInvalidArgumentWhenNameEmpty) {
    anim::AnimationClip clip;
    EXPECT_THROW(clip.AddEvent("", 0.0, 1), std::invalid_argument);
    EXPECT_TRUE(clip.EventTracks().empty());
}

TEST(AnimationClipTest, AddEvent_ThrowsInvalidArgumentWhenTimeNegative) {
    anim::AnimationClip clip;
    EXPECT_THROW(clip.AddEvent("s", -1e-9, 1), std::invalid_argument);
    EXPECT_NO_THROW(clip.AddEvent("s", 0.0, 1));
}

TEST(AnimationClipTest, AddEvent_ThrowsInvalidArgumentWhenDuplicateTime) {
    anim::AnimationClip clip;
    clip.AddEvent("s", 0.5, 1);
    EXPECT_THROW(clip.AddEvent("s", 0.5, 2), std::invalid_argument);
    // 別名トラックの同時刻は受理されること
    EXPECT_NO_THROW(clip.AddEvent("t", 0.5, 2));
}

TEST(AnimationClipTest, AddEvent_ThrowsInvalidArgumentWhenBeyondExplicitDuration) {
    anim::AnimationClip clip;
    clip.SetDuration(5.0);
    EXPECT_NO_THROW(clip.AddEvent("s", 5.0, 1));
    EXPECT_THROW(clip.AddEvent("s", 5.0 + 1e-9, 2), std::invalid_argument);
}



/**
 * Duration (3種のキーの共有)
 */

TEST(AnimationClipTest, Duration_IncludesVisibilityAndEventKeys) {
    anim::AnimationClip clip;
    const auto id = MakeTargetId();
    clip.AddKey(id, 1.0, Matrix4d::Identity());
    EXPECT_DOUBLE_EQ(clip.Duration(), 1.0);
    clip.AddVisibilityKey(id, 2.5, false);
    EXPECT_DOUBLE_EQ(clip.Duration(), 2.5);
    clip.AddEvent("s", 4.0, 1);
    EXPECT_DOUBLE_EQ(clip.Duration(), 4.0);
    // より早い時刻のキーを追加しても最大値は維持されること
    clip.AddVisibilityKey(id, 0.5, true);
    EXPECT_DOUBLE_EQ(clip.Duration(), 4.0);
}

TEST(AnimationClipTest, Duration_VisibilityOnlyClipUsesVisibilityKeys) {
    anim::AnimationClip clip;
    clip.AddVisibilityKey(MakeTargetId(), 3.0, false);
    EXPECT_DOUBLE_EQ(clip.Duration(), 3.0);
    EXPECT_TRUE(clip.Tracks().empty());
}

TEST(AnimationClipTest, SetDuration_ThrowsWhenLessThanLastVisibilityOrEventKey) {
    anim::AnimationClip clip;
    clip.AddVisibilityKey(MakeTargetId(), 3.0, false);
    EXPECT_NO_THROW(clip.SetDuration(3.0));
    EXPECT_THROW(clip.SetDuration(3.0 - 1e-9), std::invalid_argument);

    anim::AnimationClip event_clip;
    event_clip.AddEvent("s", 2.0, 1);
    EXPECT_NO_THROW(event_clip.SetDuration(2.0));
    EXPECT_THROW(event_clip.SetDuration(2.0 - 1e-9), std::invalid_argument);
}

}  // namespace
