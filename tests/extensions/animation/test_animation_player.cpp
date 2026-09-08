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
 *       - 可視性トラック: Bindの基準可視性捕捉・未解決IDの重複排除、時刻0キーの
 *         即時適用、キー境界での切替とリビジョン差分適用、Seekの即時反映、
 *         Stop/Unbindでの基準可視性 (false) の復元、外部変更の上書き規約
 *       - TakeTimeChange: 未バインド/Bind直後の初期状態、Advanceの単調性、
 *         Seek前後進、ループ巻き戻し、終端保持、Stop/Play(終端後)の非単調、
 *         Take後のリセット
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
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
/// @note ツリーはroot直下に子A・B・Cを持ち、A・Bには基準姿勢 (非恒等の
///       平行移動) を、Cには基準可視性false (復元順序の検証用) を設定する.
///       クリップは提示例と同じ変換キー配置:
///       A: (0.5, T1), (2.0, T2), (2.1, T3) / B: (0.5, T1), (3.0, T4)
///       に加え、Cの可視性キー (1.0, true), (3.0, false) とイベントトラック
///       "stage": (0.5, 1), (2.0, 2), (3.0, 3) を持つ.
///       durationは4.0を明示設定する (最終キー状態の保持区間を持たせるため).
class AnimationPlayerTest : public ::testing::Test {
 protected:
    /// @brief ツリーとクリップを構築する
    void SetUp() override {
        root_ = i_mod::MakeAssembly("root");
        a_ = i_mod::MakeAssembly("A");
        b_ = i_mod::MakeAssembly("B");
        c_ = i_mod::MakeAssembly("C");
        root_->AddChildAssembly(a_);
        root_->AddChildAssembly(b_);
        root_->AddChildAssembly(c_);
        a_->SetGlobalTransform(base_a_);
        b_->SetGlobalTransform(base_b_);
        c_->SetVisible(false);

        clip_.AddKey(a_->GetID(), 0.5, t1_);
        clip_.AddKey(a_->GetID(), 2.0, t2_);
        clip_.AddKey(a_->GetID(), 2.1, t3_);
        clip_.AddKey(b_->GetID(), 0.5, t1_);
        clip_.AddKey(b_->GetID(), 3.0, t4_);
        clip_.AddVisibilityKey(c_->GetID(), 1.0, true);
        clip_.AddVisibilityKey(c_->GetID(), 3.0, false);
        clip_.AddEvent("stage", 0.5, 1);
        clip_.AddEvent("stage", 2.0, 2);
        clip_.AddEvent("stage", 3.0, 3);
        clip_.SetDuration(4.0);
    }

    /// @brief 時刻変化の記録が期待どおりであることを検証する
    /// @param change 検証する記録
    /// @param changed 期待するchanged
    /// @param from 期待するfrom [s]
    /// @param to 期待するto [s]
    /// @param monotone 期待するmonotone
    static void ExpectChange(const anim::TimeChange& change,
                             const bool changed, const double from,
                             const double to, const bool monotone) {
        EXPECT_EQ(change.changed, changed);
        EXPECT_NEAR(change.from, from, kTol);
        EXPECT_NEAR(change.to, to, kTol);
        EXPECT_EQ(change.monotone, monotone);
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
    /// @brief 可視性トラックの対象の子Assembly C (基準可視性false)
    std::shared_ptr<i_mod::Assembly> c_;
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



/**
 * 可視性トラック (Bind)
 */

TEST_F(AnimationPlayerTest, Bind_CapturesBaseVisibility) {
    anim::AnimationPlayer player;
    const auto unresolved = player.Bind(root_, clip_);
    EXPECT_TRUE(unresolved.empty());
    // 時刻0に可視性キーが無いため、基準可視性 (false) のまま
    EXPECT_FALSE(c_->Display().visible);
    // 可視性キーを跨いだ後、基準可視性 (false) へ復元されること
    player.Seek(1.5);
    EXPECT_TRUE(c_->Display().visible);
    player.Stop();
    EXPECT_FALSE(c_->Display().visible);
}

TEST_F(AnimationPlayerTest, Bind_ReportsUnresolvedVisibilityTargetOnce) {
    // 変換と可視性の両方で未解決となる同一IDは1回だけ報告されること
    const auto ghost_id = IDGenerator::Generate(ObjectType::kAssembly);
    clip_.AddKey(ghost_id, 1.0, t1_);
    clip_.AddVisibilityKey(ghost_id, 1.0, false);
    // 可視性のみ未解決のIDも報告されること
    const auto ghost_vis_id = IDGenerator::Generate(ObjectType::kAssembly);
    clip_.AddVisibilityKey(ghost_vis_id, 2.0, false);

    anim::AnimationPlayer player;
    const auto unresolved = player.Bind(root_, clip_);
    ASSERT_EQ(unresolved.size(), 2u);
    EXPECT_EQ(unresolved[0], ghost_id);
    EXPECT_EQ(unresolved[1], ghost_vis_id);

    // 解決済みの可視性トラックは通常どおり動作すること (部分バインド)
    player.Seek(1.5);
    EXPECT_TRUE(c_->Display().visible);
}

TEST_F(AnimationPlayerTest, Bind_AppliesVisibilityKeyAtTimeZero) {
    anim::AnimationClip clip;
    clip.AddVisibilityKey(a_->GetID(), 0.0, false);

    anim::AnimationPlayer player;
    player.Bind(root_, clip);
    // 時刻0のキーはバインド直後に反映されること
    EXPECT_FALSE(a_->Display().visible);
    // Unbindで基準可視性 (true) へ戻ること
    player.Unbind();
    EXPECT_TRUE(a_->Display().visible);
}



/**
 * 可視性トラック (Advance / Seek)
 */

TEST_F(AnimationPlayerTest, Advance_TogglesVisibilityAtKeyBoundaries) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();

    // t=0.75: 先頭キーより前は基準可視性 (false)
    player.Advance(0.75);
    EXPECT_FALSE(c_->Display().visible);
    // t=1.25: (1.0, true) を跨ぐ
    player.Advance(0.5);
    EXPECT_TRUE(c_->Display().visible);
    // t=2.75: 次のキーの直前まで保持
    player.Advance(1.5);
    EXPECT_TRUE(c_->Display().visible);
    // t=3.5: (3.0, false) を跨ぐ
    player.Advance(0.75);
    EXPECT_FALSE(c_->Display().visible);
    // 終端到達後も最終キーの状態を保持すること
    player.Advance(10.0);
    EXPECT_EQ(player.State(), anim::PlaybackState::kFinished);
    EXPECT_FALSE(c_->Display().visible);
}

TEST_F(AnimationPlayerTest, Advance_BumpsRevisionOnlyOnVisibilityKeyCrossing) {
    // 可視性トラックのみのクリップで、リビジョンの差分適用を確認する
    anim::AnimationClip clip;
    clip.AddVisibilityKey(c_->GetID(), 1.0, true);
    clip.AddVisibilityKey(c_->GetID(), 2.0, true);   // 同値の連続キー
    clip.AddVisibilityKey(c_->GetID(), 3.0, false);
    clip.SetDuration(4.0);

    anim::AnimationPlayer player;
    player.Bind(root_, clip);
    player.Play();
    // t=1.0: 1.0のキーを跨ぐ (ここで可視性が変化する)
    player.Advance(1.0);
    EXPECT_TRUE(c_->Display().visible);
    const auto revision = root_->Revision();

    // 同一キー区間内 ([1.0, 2.0)) の前進ではリビジョンが変化しないこと
    player.Advance(0.3);
    player.Advance(0.3);
    EXPECT_EQ(root_->Revision(), revision);

    // 同値キー (2.0, true) を跨いでも値が変わらないためバンプされないこと
    player.Advance(0.5);
    EXPECT_EQ(root_->Revision(), revision);

    // 値が変わるキー (3.0, false) を跨ぐとバンプされること
    player.Advance(1.0);
    EXPECT_FALSE(c_->Display().visible);
    EXPECT_GT(root_->Revision(), revision);
}

TEST_F(AnimationPlayerTest, Seek_AppliesVisibilityImmediately) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    // 停止中のSeekでも即時に反映されること
    player.Seek(2.0);
    EXPECT_TRUE(c_->Display().visible);
    player.Seek(3.0);
    EXPECT_FALSE(c_->Display().visible);
    // 先頭キーより前へ戻すと基準可視性 (false) になること
    player.Seek(0.5);
    EXPECT_FALSE(c_->Display().visible);
    // 変換トラックとの同時適用
    player.Seek(2.5);
    EXPECT_TRUE(c_->Display().visible);
    ExpectPose(a_, t3_ * base_a_);
}



/**
 * 可視性トラック (復元・規約)
 */

TEST_F(AnimationPlayerTest, Stop_RestoresBaseVisibility) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(2.0);
    ASSERT_TRUE(c_->Display().visible);
    player.Stop();
    // 基準可視性 (false) が正しく戻ること (trueへの一律復元ではない)
    EXPECT_FALSE(c_->Display().visible);
    ExpectPose(a_, base_a_);
}

TEST_F(AnimationPlayerTest, Unbind_RestoresBaseVisibility) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(2.0);
    ASSERT_TRUE(c_->Display().visible);
    player.Unbind();
    EXPECT_FALSE(player.IsBound());
    EXPECT_FALSE(c_->Display().visible);
    ExpectPose(a_, base_a_);
}

TEST_F(AnimationPlayerTest, ExternalVisibilityChange_IsOverriddenAtNextKey) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(1.5);
    ASSERT_TRUE(c_->Display().visible);

    // 利用側が直接変更した値は、同一キー区間内では維持される
    c_->SetVisible(false);
    player.Advance(0.5);  // t=2.0 (可視性キーは無い)
    EXPECT_FALSE(c_->Display().visible);

    // 次のキー境界でクリップの値に上書きされること
    c_->SetVisible(true);
    player.Advance(1.0);  // t=3.0 → (3.0, false)
    EXPECT_FALSE(c_->Display().visible);

    // Stopでは外部変更ではなくBind時の値 (false) へ戻ること
    c_->SetVisible(true);
    player.Stop();
    EXPECT_FALSE(c_->Display().visible);
}



/**
 * TakeTimeChange
 */

TEST_F(AnimationPlayerTest, TakeTimeChange_UnboundIsUnchanged) {
    anim::AnimationPlayer player;
    ExpectChange(player.TakeTimeChange(), false, 0.0, 0.0, true);
    // 未バインド時のAdvance/Seek/Stopは記録に影響しないこと
    player.Advance(1.0);
    player.Seek(2.0);
    player.Stop();
    ExpectChange(player.TakeTimeChange(), false, 0.0, 0.0, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_InitialAfterBindIsUnchanged) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    ExpectChange(player.TakeTimeChange(), false, 0.0, 0.0, true);
    // Play・Pause・SetSpeed・SetLoopだけでは時刻は動かないこと
    player.Play();
    player.Pause();
    player.SetSpeed(2.0);
    player.SetLoop(true);
    ExpectChange(player.TakeTimeChange(), false, 0.0, 0.0, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_AdvanceIsMonotone) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    // 2回のAdvanceが1回のTakeにまとめられ、from=最初・to=最後となること
    player.Advance(0.5);
    player.Advance(0.7);
    ExpectChange(player.TakeTimeChange(), true, 0.0, 1.2, true);
    // 続きのAdvanceは前回Take時点を起点とすること
    player.Advance(0.3);
    ExpectChange(player.TakeTimeChange(), true, 1.2, 1.5, true);
    // dt=0のAdvanceは時刻が動かないため changed=false
    player.Advance(0.0);
    ExpectChange(player.TakeTimeChange(), false, 1.5, 1.5, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_SeekForwardIsMonotone) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(1.0);
    player.Seek(2.5);
    ExpectChange(player.TakeTimeChange(), true, 0.0, 2.5, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_SeekBackwardIsNotMonotone) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(2.5);
    player.TakeTimeChange();
    player.Seek(1.0);
    ExpectChange(player.TakeTimeChange(), true, 2.5, 1.0, false);

    // 後退してから前進し、from <= to となっても monotone == false のまま
    player.Seek(0.5);
    player.Seek(3.0);
    ExpectChange(player.TakeTimeChange(), true, 1.0, 3.0, false);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_SeekSameTimeIsUnchanged) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(1.0);
    player.TakeTimeChange();
    player.Seek(1.0);
    ExpectChange(player.TakeTimeChange(), false, 1.0, 1.0, true);
    // クランプにより同時刻となるSeekも changed=false
    player.Seek(4.0);
    player.TakeTimeChange();
    player.Seek(100.0);
    ExpectChange(player.TakeTimeChange(), false, 4.0, 4.0, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_LoopWrapIsNotMonotone) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.SetLoop(true);
    player.Play();
    player.Seek(3.9);
    player.TakeTimeChange();
    // 巻き戻して0.1へ. from=3.9 > to=0.1
    player.Advance(0.2);
    ExpectChange(player.TakeTimeChange(), true, 3.9, 0.1, false);

    // 巻き戻し後に前進し、from <= to となっても monotone == false のまま
    player.Seek(0.5);
    player.TakeTimeChange();
    player.Advance(4.2);  // 4.7 → 0.7
    ExpectChange(player.TakeTimeChange(), true, 0.5, 0.7, false);
    EXPECT_EQ(player.State(), anim::PlaybackState::kPlaying);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_FinishHoldsDurationAndStaysMonotone) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(1.0);
    player.TakeTimeChange();
    // 終端到達: to=Duration, 単調のまま
    player.Advance(10.0);
    EXPECT_EQ(player.State(), anim::PlaybackState::kFinished);
    ExpectChange(player.TakeTimeChange(), true, 1.0, 4.0, true);
    // 終端で停止中のAdvanceは時刻が動かない
    player.Advance(1.0);
    ExpectChange(player.TakeTimeChange(), false, 4.0, 4.0, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_StopResetsToZeroNotMonotone) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(2.5);
    player.TakeTimeChange();
    player.Stop();
    ExpectChange(player.TakeTimeChange(), true, 2.5, 0.0, false);
    // 時刻0でのStopは時刻が動かないため changed=false
    player.Stop();
    ExpectChange(player.TakeTimeChange(), false, 0.0, 0.0, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_UnbindResetsToZeroNotMonotone) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(2.5);
    player.TakeTimeChange();
    player.Unbind();
    // Unbindによる0への巻き戻しは1回だけ報告されること
    ExpectChange(player.TakeTimeChange(), true, 2.5, 0.0, false);
    ExpectChange(player.TakeTimeChange(), false, 0.0, 0.0, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_PlayAfterFinishedNotMonotone) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Play();
    player.Advance(10.0);
    ASSERT_EQ(player.State(), anim::PlaybackState::kFinished);
    player.TakeTimeChange();
    // 終端からのPlayは先頭へ巻き戻すため非単調
    player.Play();
    ExpectChange(player.TakeTimeChange(), true, 4.0, 0.0, false);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_ResetsAfterTake) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(2.5);
    player.Seek(1.0);
    ExpectChange(player.TakeTimeChange(), true, 0.0, 1.0, false);
    // Take後は changed=false, monotone=true, from=現在時刻 にリセットされること
    ExpectChange(player.TakeTimeChange(), false, 1.0, 1.0, true);
}

TEST_F(AnimationPlayerTest, TakeTimeChange_RebindResetsRecord) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    player.Seek(2.5);
    // 再Bindは記録を初期化する (前バインドのUnbindによる巻き戻しは報告しない)
    player.Bind(root_, clip_);
    ExpectChange(player.TakeTimeChange(), false, 0.0, 0.0, true);
}



/**
 * イベントトラック (プレイヤー経由の問い合わせ)
 */

TEST_F(AnimationPlayerTest, EventTrack_QueriedThroughClipAndCurrentTime) {
    anim::AnimationPlayer player;
    player.Bind(root_, clip_);
    const auto* stage = player.Clip().FindEventTrack("stage");
    ASSERT_NE(stage, nullptr);
    EXPECT_EQ(anim::ActiveEventValue(*stage, player.CurrentTime()),
              std::nullopt);

    player.Play();
    player.Advance(1.0);
    EXPECT_EQ(anim::ActiveEventValue(*stage, player.CurrentTime()),
              std::optional<std::int64_t>(1));
    // TakeTimeChangeの区間で跨いだイベントキーを列挙できること
    const auto tc = player.TakeTimeChange();
    ASSERT_TRUE(tc.changed);
    const auto range = anim::EventKeysBetween(*stage, tc.from, tc.to);
    EXPECT_EQ(range.first, 0u);
    EXPECT_EQ(range.second, 1u);

    // 未バインド後のClip()は空クリップとなり、トラックは見つからないこと
    player.Unbind();
    EXPECT_EQ(player.Clip().FindEventTrack("stage"), nullptr);
}

}  // namespace
