/**
 * @file tests/extensions/animation/test_animation_player.cpp
 * @brief AnimationPlayer (extensions/animation) のテスト
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 * @note 対象は以下の振る舞い:
 *       - 正常系 (代表値): Bindの基準姿勢捕捉・未解決ID報告、設計提示例の
 *         クリップでのステップ姿勢適用 (T·G_base)、Pause/Seek/速度変更/
 *         ループ/終端保持/再スタート/Stop・Unbindの基準姿勢復元
 *       - リビジョン検証: キー境界を跨がないAdvanceでモデルリビジョンが
 *         変化しないこと (差分適用)
 *       - 正常系 (退化): 空クリップ、weak_ptr失効トラックのスキップ、
 *         再バインド時の復元
 *       - 異常系: rootがnullptr・負のdt・非正の速度
 */
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/animation/animation_clip.h"
#include "igesio/extensions/animation/animation_player.h"

namespace {

namespace anim = igesio::extensions::animation;
namespace i_mod = igesio::models;
using igesio::Matrix4d;
using igesio::IDGenerator;
using igesio::ObjectType;

/// @brief 行列比較の許容誤差
constexpr double kTol = 1e-12;

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

/// @brief 設計提示例のクリップとAssemblyツリーを構築する共通フィクスチャ
/// @note ツリーはroot直下に子A・Bを持ち、それぞれ基準姿勢 (非恒等の平行移動)
///       を設定する. クリップは提示例と同じキー配置:
///       A: (0.5, T1), (2.0, T2), (2.1, T3) / B: (0.5, T1), (3.0, T4)
///       durationは4.0を明示設定する (最終キー姿勢の保持区間を持たせるため).
class AnimationPlayerTest : public ::testing::Test {
 protected:
    /// @brief ツリーとクリップを構築する
    void SetUp() override {
        root_ = i_mod::MakeAssembly("root");
        a_ = i_mod::MakeAssembly("A");
        b_ = i_mod::MakeAssembly("B");
        root_->AddChildAssembly(a_);
        root_->AddChildAssembly(b_);
        a_->SetGlobalTransform(base_a_);
        b_->SetGlobalTransform(base_b_);

        clip_.AddKey(a_->GetID(), 0.5, t1_);
        clip_.AddKey(a_->GetID(), 2.0, t2_);
        clip_.AddKey(a_->GetID(), 2.1, t3_);
        clip_.AddKey(b_->GetID(), 0.5, t1_);
        clip_.AddKey(b_->GetID(), 3.0, t4_);
        clip_.SetDuration(4.0);
    }

    /// @brief 大域変換が期待値と一致することを検証する
    /// @param node 検証対象のAssembly
    /// @param expected 期待する大域変換
    static void ExpectPose(const std::shared_ptr<i_mod::Assembly>& node,
                           const Matrix4d& expected) {
        EXPECT_TRUE(node->GetGlobalTransform().isApprox(expected, kTol))
                << "actual:\n" << node->GetGlobalTransform()
                << "\nexpected:\n" << expected;
    }

    /// @brief ルートAssembly
    std::shared_ptr<i_mod::Assembly> root_;
    /// @brief アニメーション対象の子Assembly A
    std::shared_ptr<i_mod::Assembly> a_;
    /// @brief アニメーション対象の子Assembly B
    std::shared_ptr<i_mod::Assembly> b_;
    /// @brief Aの基準姿勢 (非恒等でT·G_baseの合成順を検証する)
    const Matrix4d base_a_ = MakeTranslation(5.0, 0.0, 0.0);
    /// @brief Bの基準姿勢
    const Matrix4d base_b_ = MakeTranslation(0.0, 5.0, 0.0);
    /// @brief キー変換T1〜T4 (互いに区別可能な平行移動)
    const Matrix4d t1_ = MakeTranslation(0.0, 0.0, 1.0);
    const Matrix4d t2_ = MakeTranslation(0.0, 0.0, 2.0);
    const Matrix4d t3_ = MakeTranslation(0.0, 0.0, 3.0);
    const Matrix4d t4_ = MakeTranslation(0.0, 0.0, 4.0);
    /// @brief 提示例のクリップ
    anim::AnimationClip clip_;
};



/**
 * Bind (正常系・異常系)
 */

TEST_F(AnimationPlayerTest, Bind_CapturesBasePoseAndResolvesAllTargets) {
    anim::AnimationPlayer player;
    const auto unresolved = player.Bind(root_, clip_);
    EXPECT_TRUE(unresolved.empty());
    EXPECT_TRUE(player.IsBound());
    EXPECT_EQ(player.State(), anim::PlaybackState::kStopped);
    EXPECT_DOUBLE_EQ(player.Duration(), 4.0);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 0.0);
    // 時刻0にキーが無いため姿勢は基準のまま
    ExpectPose(a_, base_a_);
    ExpectPose(b_, base_b_);
}

TEST_F(AnimationPlayerTest, Bind_ReportsUnresolvedTargets) {
    // ツリーに存在しないIDを対象とするトラックを混ぜる
    const auto ghost_id = IDGenerator::Generate(ObjectType::kAssembly);
    clip_.AddKey(ghost_id, 1.0, t1_);

    anim::AnimationPlayer player;
    const auto unresolved = player.Bind(root_, clip_);
    ASSERT_EQ(unresolved.size(), 1u);
    EXPECT_EQ(unresolved[0], ghost_id);

    // 解決済みトラックは通常どおり再生されること (部分バインド)
    player.Play();
    player.Advance(1.0);
    ExpectPose(a_, t1_ * base_a_);
}

TEST_F(AnimationPlayerTest, Bind_AppliesKeyAtTimeZeroImmediately) {
    anim::AnimationClip clip;
    clip.AddKey(a_->GetID(), 0.0, t1_);

    anim::AnimationPlayer player;
    player.Bind(root_, clip);
    // 時刻0のキーはバインド直後に反映されること
    ExpectPose(a_, t1_ * base_a_);
}

TEST_F(AnimationPlayerTest, Bind_RestoresPreviousBind) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(1.0);
    ExpectPose(a_, t1_ * base_a_);

    // 再バインドで前のバインドの姿勢が基準へ復元されること
    player.Bind(root_, anim::AnimationClip());
    ExpectPose(a_, base_a_);
    ExpectPose(b_, base_b_);
}

TEST_F(AnimationPlayerTest, Bind_ThrowsInvalidArgumentWhenRootIsNull) {
    anim::AnimationPlayer player;
    EXPECT_THROW(player.Bind(nullptr, clip_), std::invalid_argument);
}



/**
 * Advance (姿勢適用)
 */

TEST_F(AnimationPlayerTest, Advance_AppliesStepPosesPerExample) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();

    // t=0.4: 両者とも基準姿勢
    player.Advance(0.4);
    ExpectPose(a_, base_a_);
    ExpectPose(b_, base_b_);

    // t=0.6: 両者ともT1
    player.Advance(0.2);
    ExpectPose(a_, t1_ * base_a_);
    ExpectPose(b_, t1_ * base_b_);

    // t=2.05: AはT2、BはT1のまま
    player.Advance(1.45);
    ExpectPose(a_, t2_ * base_a_);
    ExpectPose(b_, t1_ * base_b_);

    // t=2.5: AはT3
    player.Advance(0.45);
    ExpectPose(a_, t3_ * base_a_);
    ExpectPose(b_, t1_ * base_b_);

    // t=3.5: BはT4、AはT3のまま
    player.Advance(1.0);
    ExpectPose(a_, t3_ * base_a_);
    ExpectPose(b_, t4_ * base_b_);
}

TEST_F(AnimationPlayerTest, Advance_DoesNothingWhenNotPlaying) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Advance(2.0);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 0.0);
    ExpectPose(a_, base_a_);
}

TEST_F(AnimationPlayerTest, Advance_BumpsRevisionOnlyOnKeyCrossing) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    // t=1.0: 0.5のキーを跨ぐ (ここで姿勢適用が発生する)
    player.Advance(1.0);
    const auto revision = root_->Revision();

    // 同一キー区間内 ([0.5, 2.0)) の前進ではリビジョンが変化しないこと
    player.Advance(0.3);
    player.Advance(0.3);
    EXPECT_EQ(root_->Revision(), revision);

    // 2.0のキーを跨ぐとリビジョンが変化すること
    player.Advance(0.5);
    EXPECT_GT(root_->Revision(), revision);
}

TEST_F(AnimationPlayerTest, Advance_ThrowsInvalidArgumentWhenDtNegative) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    // 境界: 0.0は受理、その直外は拒否
    EXPECT_THROW(player.Advance(-1e-9), std::invalid_argument);
    EXPECT_NO_THROW(player.Advance(0.0));
}



/**
 * Pause / Seek / 速度
 */

TEST_F(AnimationPlayerTest, Pause_HoldsTimeAndPose) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(1.0);
    player.Pause();
    EXPECT_EQ(player.State(), anim::PlaybackState::kPaused);

    // 一時停止中のAdvanceでは時刻・姿勢が変化しないこと
    player.Advance(5.0);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 1.0);
    ExpectPose(a_, t1_ * base_a_);

    // 再開後は続きから進むこと
    player.Play();
    player.Advance(1.2);  // t=2.2
    ExpectPose(a_, t3_ * base_a_);
}

TEST_F(AnimationPlayerTest, Seek_AppliesPoseImmediatelyWithoutPlaying) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(2.5);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 2.5);
    ExpectPose(a_, t3_ * base_a_);
    ExpectPose(b_, t1_ * base_b_);
}

TEST_F(AnimationPlayerTest, Seek_ClampsToValidRange) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(-1.0);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 0.0);
    player.Seek(100.0);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 4.0);
}

TEST_F(AnimationPlayerTest, SetSpeed_ScalesTimeAdvance) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.SetSpeed(4.0);
    player.Play();
    // 実時間0.25s × 4倍速 = 内部時刻1.0s
    player.Advance(0.25);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 1.0);
    ExpectPose(a_, t1_ * base_a_);
}

TEST_F(AnimationPlayerTest, SetSpeed_ThrowsInvalidArgumentWhenNotPositive) {
    anim::AnimationPlayer player;
    // 境界: 0は拒否、正の微小値は受理
    EXPECT_THROW(player.SetSpeed(0.0), std::invalid_argument);
    EXPECT_THROW(player.SetSpeed(-1.0), std::invalid_argument);
    EXPECT_NO_THROW(player.SetSpeed(1e-9));
}



/**
 * 終端処理・ループ
 */

TEST_F(AnimationPlayerTest, Finish_HoldsLastPoseWhenLoopDisabled) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(10.0);
    EXPECT_EQ(player.State(), anim::PlaybackState::kFinished);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 4.0);
    // 最終姿勢を保持すること
    ExpectPose(a_, t3_ * base_a_);
    ExpectPose(b_, t4_ * base_b_);
}

TEST_F(AnimationPlayerTest, Loop_WrapsAroundEnd) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.SetLoop(true);
    player.Play();
    player.Seek(3.9);
    player.Advance(0.2);  // t=4.1 → 巻き戻して0.1
    EXPECT_EQ(player.State(), anim::PlaybackState::kPlaying);
    EXPECT_NEAR(player.CurrentTime(), 0.1, kTol);
    // 巻き戻し後の時刻に対応する姿勢 (先頭キーより前=基準姿勢) になること
    ExpectPose(a_, base_a_);
    ExpectPose(b_, base_b_);
}

TEST_F(AnimationPlayerTest, Play_RestartsFromBeginningAfterFinished) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(10.0);
    ASSERT_EQ(player.State(), anim::PlaybackState::kFinished);

    // 終端からのPlayは先頭へ巻き戻して再生を始めること
    player.Play();
    EXPECT_EQ(player.State(), anim::PlaybackState::kPlaying);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 0.0);
    ExpectPose(a_, base_a_);
}



/**
 * Stop / Unbind (復元)
 */

TEST_F(AnimationPlayerTest, Stop_RestoresBasePoses) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(2.5);
    player.Stop();
    EXPECT_EQ(player.State(), anim::PlaybackState::kStopped);
    EXPECT_DOUBLE_EQ(player.CurrentTime(), 0.0);
    ExpectPose(a_, base_a_);
    ExpectPose(b_, base_b_);
}

TEST_F(AnimationPlayerTest, Unbind_RestoresBasePosesAndClearsState) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(2.5);
    player.Unbind();
    EXPECT_FALSE(player.IsBound());
    EXPECT_DOUBLE_EQ(player.Duration(), 0.0);
    ExpectPose(a_, base_a_);
    ExpectPose(b_, base_b_);
}



/**
 * 退化ケース
 */

TEST_F(AnimationPlayerTest, EmptyClip_FinishesImmediately) {
    anim::AnimationPlayer player;
    player.Bind(root_, anim::AnimationClip());
    EXPECT_DOUBLE_EQ(player.Duration(), 0.0);
    player.Play();
    player.Advance(0.1);
    EXPECT_EQ(player.State(), anim::PlaybackState::kFinished);
}

TEST_F(AnimationPlayerTest, ExpiredTarget_IsSkippedSafely) {
    // 後から削除する子Assemblyを対象に含める
    auto doomed = i_mod::MakeAssembly("doomed");
    root_->AddChildAssembly(doomed);
    const auto doomed_id = doomed->GetID();
    clip_.AddKey(doomed_id, 1.0, t1_);

    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();

    // 対象をツリーから削除し、テスト側の強参照も手放す (weak_ptr失効)
    ASSERT_TRUE(root_->RemoveChildAssembly(doomed_id));
    doomed.reset();

    // 失効トラックを含んだままキーを跨いでもクラッシュせず、他は動作すること
    EXPECT_NO_THROW(player.Advance(1.5));
    ExpectPose(a_, t1_ * base_a_);
}

}  // namespace
