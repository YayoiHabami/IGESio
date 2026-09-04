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
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>

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
    const anim::AnimationTrack track{MakeTargetId(), {}};
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

}  // namespace
