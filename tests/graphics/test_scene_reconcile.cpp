/**
 * @file tests/graphics/test_scene_reconcile.cpp
 * @brief レンダラのReconcile (Sceneツリーとの遅延同期) の検証
 * @author Yayoi Habami
 * @date 2026-06-06
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 *   - 遅延生成: AddEntityなしでScene設定+Drawのみで描画されること
 *   - Sweep: ツリーから削除されたエンティティが次回Drawで描画されず、
 *     GPUリソースが解放されること
 *   - 自動検知: Draw後のエンティティ追加・可視性変更が手動通知なしで反映されること
 *   - DisplayFilter: 除外型は描画されず、キャッシュは温存されること (再生成なし)
 *   - DisplayFilter (アセンブリ単位): 隠した部分木は走査されず描画オブジェクトが
 *     生成されないこと、生成後に隠してもキャッシュが温存されること
 *   - 表示座標系 (SetViewFrame): model行列に逆行列が前掛けされること、変更で
 *     再テッセレーション/再アップロードが生じないこと、非剛体を拒否すること
 *   - FitView: 走査規則 (フィルタ・表示座標系) を反映した可視BBを使うこと、
 *     GLバックエンド未設定でも動くこと
 *   - Cleanup: GLバックエンド未設定 (nullptr構築) のレンダラでも明示呼び出し・
 *     デストラクタでGLに触れずに破棄できること
 *   - ジオメトリ再同期: 形状編集→次Drawで再テッセレーションされること
 *     (自エンティティ編集と、参照先の子曲線編集の両方)
 *   - 同期キー: 参照先の差し替えでキーが変化すること (リビジョン総和では
 *     相殺しうるケースの回帰)、NeedsResyncの遷移
 *
 * NOTE: 検証はMockOpenGLの呼び出し回数 (draw/gen/delete/buffer_data) の
 *       デルタで行う. 絶対回数は実装詳細のため検証しない.
 * TODO: Walk実行回数のゲート (非dirty再描画で走査スキップ) は観測手段が無いため
 *       対象外 (test_scene_walk.cppの既存TODOと同様). 同値のSetViewFrameが
 *       再走査を誘発しないことも同じ理由で対象外.
 * TODO: マテリアル遅延適用のGL非接触性は、Mockが全GL呼び出しを計数しないため
 *       対象外 (setterがGLメンバへ触れない構造はコードレビューで担保).
 */
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <vector>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"
#include "igesio/entities/surfaces/ruled_surface.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/factory.h"
#include "igesio/graphics/renderer.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector2d;
using igesio::Vector3d;
using i_graph::test::MockOpenGL;

/// @brief スモーク用の単純な円弧 (M_entity=単位)
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return i_ent::MakeCircularArc(
        Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, 1.0), 0.0);
}

/// @brief degree=2 Bezier曲線 (編集可能なNURBS)
std::shared_ptr<i_ent::RationalBSplineCurve> MakeTestCurve() {
    igesio::Matrix3Xd cp(3, 3);
    cp << 0.0, 1.0, 2.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 0.0;
    return i_ent::MakeBezierCurve(cp);
}

/// @brief 双一次NURBS平面 (z=0, ドメイン[0,1]²)
std::shared_ptr<i_ent::RationalBSplineSurface> MakePlane() {
    const std::vector<std::vector<Vector3d>> grid{
        {{0, 0, 0}, {0, 1, 0}},
        {{1, 0, 0}, {1, 1, 0}}};
    return i_ent::MakeBezierSurface(grid);
}

/// @brief 平行移動の同次変換を作る
igesio::Matrix4d Translate(double x, double y, double z) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m(0, 3) = x; m(1, 3) = y; m(2, 3) = z;
    return m;
}

/// @brief 原点の円弧を持つrootと、Translate(100,0,0)の子アセンブリ (円弧1個) を作る
/// @param[out] child 作成した子アセンブリ
/// @return ルートアセンブリ
/// @note 円弧はz=0平面の[0,1]²に収まる (BB中心は各アセンブリ配置での(0.5,0.5,0))
std::shared_ptr<i_mod::Assembly> MakeRootWithFarChild(
        std::shared_ptr<i_mod::Assembly>* child) {
    auto root = i_mod::MakeAssembly();
    root->AddEntity(MakeArc());
    *child = i_mod::MakeAssembly();
    (*child)->SetGlobalTransform(Translate(100.0, 0.0, 0.0));
    (*child)->AddEntity(MakeArc());
    root->AddChildAssembly(*child);
    return root;
}

/// @brief 直近Drawの描画呼び出し数を返すデルタ計測ヘルパー
class DrawCounter {
 public:
    /// @brief コンストラクタ
    /// @param gl 観測対象のモック
    explicit DrawCounter(const std::shared_ptr<MockOpenGL>& gl) : gl_(gl) {}

    /// @brief 前回呼び出しからのdraw呼び出し数デルタを返す
    int Delta() {
        const int now = gl_->draw_arrays_calls + gl_->draw_elements_calls;
        const int delta = now - last_;
        last_ = now;
        return delta;
    }

 private:
    /// @brief 観測対象のモック
    std::shared_ptr<MockOpenGL> gl_;
    /// @brief 前回の累計draw呼び出し数
    int last_ = 0;
};

}  // namespace



/**
 * 遅延生成・Sweep・自動検知
 */

// AddEntityなしでScene設定+Drawのみで描画オブジェクトが遅延生成される
TEST(SceneReconcileTest, LazyCreation_DrawCreatesFromScene) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->AddEntity(MakeArc());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    EXPECT_EQ(gl->gen_vertex_arrays_calls, 0);  // SetSceneでは生成しない
    renderer.Draw();

    EXPECT_GT(gl->gen_vertex_arrays_calls, 0);
    EXPECT_GT(gl->draw_arrays_calls + gl->draw_elements_calls, 0);
}

// ツリーから削除されたエンティティは次回Drawで描画されず、GPUリソースが解放される
TEST(SceneReconcileTest, Sweep_RemovedEntityNotDrawnAndCleaned) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    auto arc = MakeArc();
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    DrawCounter draws(gl);
    renderer.Draw();
    ASSERT_GT(draws.Delta(), 0);

    // 手動の通知なしに、Assembly側の削除だけで描画から消える
    const int deletes_before = gl->delete_vertex_arrays_calls;
    ASSERT_TRUE(root->RemoveEntity(arc->GetID()));
    renderer.Draw();

    EXPECT_EQ(draws.Delta(), 0);
    // SweepがCleanup()を呼び、GPUリソース (VAO) が解放される
    EXPECT_GT(gl->delete_vertex_arrays_calls, deletes_before);
}

// Draw後に追加されたエンティティが手動通知なしで次回Drawに現れる
TEST(SceneReconcileTest, LateAdd_NewEntityAppears) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->AddEntity(MakeArc());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    DrawCounter draws(gl);
    renderer.Draw();
    const int first = draws.Delta();
    ASSERT_GT(first, 0);

    root->AddEntity(MakeArc());
    renderer.Draw();
    EXPECT_GT(draws.Delta(), first);  // 2個分描画される
}

// Draw後の可視性変更 (setter経由) が手動通知なしで反映される
TEST(SceneReconcileTest, VisibilityToggle_AutoDetected) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->AddEntity(MakeArc());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    DrawCounter draws(gl);
    renderer.Draw();
    ASSERT_GT(draws.Delta(), 0);

    root->SetVisible(false);
    renderer.Draw();
    EXPECT_EQ(draws.Delta(), 0);

    root->SetVisible(true);
    renderer.Draw();
    EXPECT_GT(draws.Delta(), 0);
}



/**
 * DisplayFilter
 */

// フィルタ除外型は描画されず、キャッシュは温存される (解除時に再生成しない)
TEST(SceneReconcileTest, Filter_HiddenTypeNotDrawn_CacheRetained) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->AddEntity(MakeArc());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    DrawCounter draws(gl);
    renderer.Draw();
    ASSERT_GT(draws.Delta(), 0);
    const int gens_after_create = gl->gen_vertex_arrays_calls;

    // 円弧型を非表示にする
    i_graph::DisplayFilter filter;
    filter.hidden_types.insert(i_ent::EntityType::kCircularArc);
    renderer.SetDisplayFilter(filter);
    renderer.Draw();
    EXPECT_EQ(draws.Delta(), 0);

    // 解除すると再描画される. キャッシュ温存のため再生成 (Gen) は起きない
    renderer.SetDisplayFilter(i_graph::DisplayFilter{});
    renderer.Draw();
    EXPECT_GT(draws.Delta(), 0);
    EXPECT_EQ(gl->gen_vertex_arrays_calls, gens_after_create);
}

// 隠したアセンブリの部分木は走査されず、描画オブジェクトが生成されない.
// 解除すると生成・描画される
TEST(SceneReconcileTest, HiddenAssembly_SkipsSubtree) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    // 子アセンブリのみにエンティティを置き、生成の有無を子だけで観測する
    auto root = i_mod::MakeAssembly();
    auto child = i_mod::MakeAssembly();
    child->AddEntity(MakeArc());
    root->AddChildAssembly(child);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    i_graph::DisplayFilter filter;
    filter.hidden_assemblies.insert(child->GetID());
    renderer.SetDisplayFilter(filter);

    DrawCounter draws(gl);
    renderer.Draw();
    EXPECT_EQ(draws.Delta(), 0);
    EXPECT_EQ(gl->gen_vertex_arrays_calls, 0);  // 走査しないので生成もされない

    renderer.SetDisplayFilter(i_graph::DisplayFilter{});
    renderer.Draw();
    EXPECT_GT(draws.Delta(), 0);
    EXPECT_GT(gl->gen_vertex_arrays_calls, 0);
}

// 生成後にアセンブリを隠しても描画オブジェクトは温存され、解除時に再生成しない
TEST(SceneReconcileTest, HiddenAssembly_KeepsCache) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    auto child = i_mod::MakeAssembly();
    child->AddEntity(MakeArc());
    root->AddChildAssembly(child);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    DrawCounter draws(gl);
    renderer.Draw();
    ASSERT_GT(draws.Delta(), 0);
    const int gens_after_create = gl->gen_vertex_arrays_calls;
    const int deletes_before = gl->delete_vertex_arrays_calls;

    // 隠す: 描画されないが、Sweepの対象ではないので解放もされない
    i_graph::DisplayFilter filter;
    filter.hidden_assemblies.insert(child->GetID());
    renderer.SetDisplayFilter(filter);
    renderer.Draw();
    EXPECT_EQ(draws.Delta(), 0);
    EXPECT_EQ(gl->delete_vertex_arrays_calls, deletes_before);

    // 解除: 再生成 (Gen) なしに再描画される
    renderer.SetDisplayFilter(i_graph::DisplayFilter{});
    renderer.Draw();
    EXPECT_GT(draws.Delta(), 0);
    EXPECT_EQ(gl->gen_vertex_arrays_calls, gens_after_create);
}



/**
 * 表示座標系 (SetViewFrame)
 */

// 表示座標系の逆行列がmodel行列に前掛けされる (frame⁻¹·G_root·G_child)
TEST(SceneReconcileTest, ViewFrame_PremultipliesInverseIntoModel) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    auto child = i_mod::MakeAssembly();
    child->SetGlobalTransform(Translate(1.0, 2.0, 0.0));
    child->AddEntity(MakeArc());
    root->AddChildAssembly(child);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // 表示座標系を(10,20,30)へ置くと、物体は相対位置 (1-10, 2-20, 0-30) に見える
    renderer.SetViewFrame(Translate(10.0, 20.0, 30.0));
    renderer.Draw();

    ASSERT_EQ(gl->last_matrix_by_name.count("model"), 1u);
    const auto& m = gl->last_matrix_by_name.at("model");  // 列優先16要素
    constexpr float kTol = 1e-5f;
    EXPECT_NEAR(m[12], -9.0f, kTol);
    EXPECT_NEAR(m[13], -18.0f, kTol);
    EXPECT_NEAR(m[14], -30.0f, kTol);

    // 単位行列へ戻すと元の配置に戻る
    renderer.SetViewFrame(igesio::Matrix4d::Identity());
    renderer.Draw();
    const auto& m2 = gl->last_matrix_by_name.at("model");
    EXPECT_NEAR(m2[12], 1.0f, kTol);
    EXPECT_NEAR(m2[13], 2.0f, kTol);
    EXPECT_NEAR(m2[14], 0.0f, kTol);
}

// 表示座標系の変更は再走査のみで、再テッセレーション/GPU再転送/再生成を生じない
TEST(SceneReconcileTest, ViewFrame_ChangeDoesNotResync) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->AddEntity(MakeTestCurve());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    DrawCounter draws(gl);
    renderer.Draw();
    ASSERT_GT(draws.Delta(), 0);
    const int buffers_before = gl->buffer_data_calls;
    const int gens_before = gl->gen_vertex_arrays_calls;

    renderer.SetViewFrame(Translate(5.0, 0.0, 0.0));
    renderer.Draw();
    EXPECT_GT(draws.Delta(), 0);
    EXPECT_EQ(gl->buffer_data_calls, buffers_before);
    EXPECT_EQ(gl->gen_vertex_arrays_calls, gens_before);
    EXPECT_TRUE(renderer.ViewFrame()(0, 3) == 5.0);
}

// 非剛体 (スケール) の表示座標系は拒否され、現在値は変わらない
TEST(SceneReconcileTest, ViewFrame_RejectsNonRigid) {
    i_graph::EntityRenderer renderer(nullptr);  // GL不要
    igesio::Matrix4d scaled = igesio::Matrix4d::Identity();
    scaled(0, 0) = 2.0;
    EXPECT_THROW(renderer.SetViewFrame(scaled), std::invalid_argument);
    EXPECT_TRUE(renderer.ViewFrame()(0, 0) == 1.0);
}



/**
 * FitView (可視エンティティのBB)
 */

// FitViewは走査規則を反映した可視エンティティのみを対象にする
// (隠したアセンブリは含めない)
TEST(SceneReconcileTest, FitView_UsesVisibleEntitiesOnly) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    std::shared_ptr<i_mod::Assembly> child;
    auto root = MakeRootWithFarChild(&child);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    constexpr float kTol = 1e-3f;
    // 両方可視: BBは[0,101]×[0,1]×{0}、中心x=50.5
    renderer.FitView();
    EXPECT_NEAR(renderer.Camera().GetTarget().x(), 50.5f, kTol);
    EXPECT_NEAR(renderer.Camera().GetTarget().y(), 0.5f, kTol);

    // 子を隠す: 原点の円弧のみ、中心(0.5,0.5,0)
    i_graph::DisplayFilter filter;
    filter.hidden_assemblies.insert(child->GetID());
    renderer.SetDisplayFilter(filter);
    renderer.FitView();
    EXPECT_NEAR(renderer.Camera().GetTarget().x(), 0.5f, kTol);
    EXPECT_NEAR(renderer.Camera().GetTarget().y(), 0.5f, kTol);

    // 型フィルタでも同様 (全て除外→BBが空→何もしない=ターゲット不変)
    i_graph::DisplayFilter type_filter;
    type_filter.hidden_types.insert(i_ent::EntityType::kCircularArc);
    renderer.SetDisplayFilter(type_filter);
    renderer.FitView();
    EXPECT_NEAR(renderer.Camera().GetTarget().x(), 0.5f, kTol);
}

// FitViewは表示座標系を反映する (ターゲットは表示座標系での中心)
TEST(SceneReconcileTest, FitView_AppliesViewFrame) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->AddEntity(MakeArc());  // BB中心(0.5,0.5,0)
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.SetViewFrame(Translate(2.0, 0.0, 0.0));
    renderer.FitView();
    constexpr float kTol = 1e-3f;
    EXPECT_NEAR(renderer.Camera().GetTarget().x(), -1.5f, kTol);
    EXPECT_NEAR(renderer.Camera().GetTarget().y(), 0.5f, kTol);
    EXPECT_NEAR(renderer.Camera().GetTarget().z(), 0.0f, kTol);
}

// FitViewはGLバックエンド未設定・Draw前でも動く (GL前提を持たない契約)
TEST(SceneReconcileTest, FitView_WorksWithoutGLBackend) {
    i_graph::EntityRenderer renderer(nullptr);

    std::shared_ptr<i_mod::Assembly> child;
    auto root = MakeRootWithFarChild(&child);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    const auto before = renderer.Camera().GetTarget();
    renderer.FitView();
    constexpr float kTol = 1e-3f;
    EXPECT_NEAR(renderer.Camera().GetTarget().x(), 50.5f, kTol);
    EXPECT_NEAR(renderer.Camera().GetTarget().y(), 0.5f, kTol);
    EXPECT_NE(renderer.Camera().GetTarget().x(), before.x());
}



/**
 * GLバックエンド未設定のレンダラの破棄
 */

// GLバックエンド未設定のレンダラは、Cleanupの明示呼び出しもデストラクタも
// GLに触れずに完了する (nullptr構築を許す契約の裏側)
TEST(SceneReconcileTest, NullBackend_CleanupDoesNotTouchGL) {
    std::shared_ptr<i_mod::Assembly> child;
    auto root = MakeRootWithFarChild(&child);
    i_mod::Scene scene(root);

    {
        i_graph::EntityRenderer renderer(nullptr);
        renderer.SetScene(&scene);
        renderer.FitView();  // シーン設定後の状態からでも
        EXPECT_NO_THROW(renderer.Cleanup());
        // スコープ終了時のデストラクタ (Cleanup経由) でもクラッシュしないこと
    }
    SUCCEED();
}



/**
 * ジオメトリ再同期
 */

// 形状編集 (setter) が次回Drawでの再テッセレーションを引き起こす
TEST(SceneReconcileTest, GeometryEdit_TriggersResync) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    auto curve = MakeTestCurve();
    root->AddEntity(curve);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    DrawCounter draws(gl);
    renderer.Draw();
    ASSERT_GT(draws.Delta(), 0);
    const int buffers_before = gl->buffer_data_calls;

    // 形状編集→ジオメトリリビジョン経由で自動検知され、再アップロードされる
    curve->SetControlPointAt(1, {1.0, 2.0, 0.0});
    renderer.Draw();
    EXPECT_GT(draws.Delta(), 0);
    EXPECT_GT(gl->buffer_data_calls, buffers_before);
}

// 参照先の子曲線の編集が、親 (RuledSurface) の再テッセレーションを引き起こす
// (子のリビジョンは親へ伝播しないため、同期キーの再帰結合で検知する)
TEST(SceneReconcileTest, ChildCurveEdit_TriggersParentResync) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto curve1 = MakeTestCurve();
    auto curve2 = MakeTestCurve();
    // z=1へ持ち上げ、退化しないルールド面 (帯) を構成する
    igesio::Matrix3Xd cp2(3, 3);
    cp2 << 0.0, 1.0, 2.0,
           0.0, 1.0, 0.0,
           1.0, 1.0, 1.0;
    curve2->SetControlPoints(cp2);
    auto rs = i_ent::MakeRuledSurface(curve1, curve2);

    auto root = i_mod::MakeAssembly();
    // 子曲線は物理従属のため独立描画されない (RuledSurface経由で描画)
    root->AddEntities({curve1, curve2, rs});
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    DrawCounter draws(gl);
    renderer.Draw();
    ASSERT_GT(draws.Delta(), 0);
    const int buffers_before = gl->buffer_data_calls;

    // 子曲線の編集はRuledSurface自身のリビジョンを変えないが、
    // 同期キー (子の再帰結合) の変化として検知される
    curve1->SetControlPointAt(1, {1.0, 3.0, 0.0});
    renderer.Draw();
    EXPECT_GT(gl->buffer_data_calls, buffers_before);
}



/**
 * 同期キー (グラフィックス単体)
 */

// 参照先の差し替えで同期キーが変化する (リビジョン総和では相殺しうる回帰)
TEST(SceneReconcileTest, GeometryKey_ChangesOnReferenceSwap) {
    auto gl = std::make_shared<MockOpenGL>();

    auto surf = MakePlane();
    auto loop = i_ent::MakeLinearPath(
            std::vector<Vector2d>{{0.2, 0.2}, {0.8, 0.2}, {0.8, 0.8},
                                  {0.2, 0.8}}, true);
    auto cops = i_ent::MakeCurveOnAParametricSurface(surf, loop).first;
    auto graphics = i_graph::CreateEntityGraphics(
            std::static_pointer_cast<const i_ent::IEntityIdentifier>(cops), gl);
    ASSERT_NE(graphics, nullptr);

    const auto key_before = graphics->CurrentGeometryKey();
    // 参照先の曲面を同一形状・別IDの曲面へ差し替える
    // (同一性(ID)がキーに含まれるため、集合の変化として必ず検知される)
    cops->SetSurface(MakePlane());
    EXPECT_NE(graphics->CurrentGeometryKey(), key_before);
}

// NeedsResyncは構築直後false→編集でtrue→Synchronizeでfalseに遷移する
TEST(SceneReconcileTest, NeedsResync_TransitionsOnEditAndSynchronize) {
    auto gl = std::make_shared<MockOpenGL>();

    auto curve = MakeTestCurve();
    auto graphics = i_graph::CreateEntityGraphics(
            std::static_pointer_cast<const i_ent::IEntityIdentifier>(curve), gl);
    ASSERT_NE(graphics, nullptr);

    // 構築時にSynchronizeが実行されキーが記録済み
    EXPECT_FALSE(graphics->NeedsResync());

    curve->SetControlPointAt(0, {0.0, 0.0, 1.0});
    EXPECT_TRUE(graphics->NeedsResync());

    graphics->Synchronize();
    EXPECT_FALSE(graphics->NeedsResync());
}
