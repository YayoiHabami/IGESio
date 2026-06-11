/**
 * @file tests/graphics/test_shader_registry.cpp
 * @brief ShaderRegistry (シェーダー定義のレジストリ) の検証
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `ShaderRegistry::Register` / `Find` / `Get` / `Revision` / `AllIds`
 *       - 組み込みシェーダーの設定 (固定IDとメタ情報)
 *       - メタ参照ヘルパー (`UsesLighting` / `IsSurfaceFill` /
 *         `HasSpecificShaderCode` / `ToString`)
 *       - レンダラの遅延コンパイル (Initialize後の登録が次のDrawで有効になる)
 * @note TODO: スレッド安全性は契約外 (登録は描画スレッドと競合しないこと) のため
 *       対象外. GLSLの実コンパイルはMockOpenGL (常に成功) のため、コンパイル
 *       失敗時の例外経路はGL実環境依存であり対象外.
 * @note ShaderRegistryに登録解除は無い (プロセス内で永続する) ため、
 *       各テストの登録名は一意にすること.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/entities/non_iges_entity_base.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/core/i_entity_graphics.h"
#include "igesio/graphics/core/entity_graphics.h"
#include "igesio/graphics/graphics_registry.h"
#include "igesio/graphics/shader_registry.h"
#include "igesio/graphics/renderer.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using i_graph::ShaderCode;
using i_graph::ShaderDrawCategory;
using i_graph::ShaderId;
using i_graph::ShaderInfo;
using i_graph::ShaderRegistry;
using i_graph::test::MockOpenGL;

/// @brief ユーザー登録シェーダーへ採番される最初のID値 (実装の採番起点)
constexpr std::uint32_t kFirstUserIdValue = 256;

/// @brief 最小の完全なシェーダーコードを作成する
/// @return vertex+fragmentのみのShaderCode
/// @note MockOpenGLはコンパイルを常に成功させるため、内容はダミーでよい
ShaderCode MakeMinimalCode() {
    return ShaderCode("void main() {}", "void main() {}");
}

/// @brief 一意な名前のShaderInfoを作成する
/// @param name 一意なシェーダー名
/// @param uses_lighting 光源を使用するか
/// @param category 表示モードによる取捨のカテゴリ
ShaderInfo MakeInfo(const std::string& name, const bool uses_lighting = false,
                    const ShaderDrawCategory category =
                            ShaderDrawCategory::kAlways) {
    ShaderInfo info;
    info.name = name;
    info.code = MakeMinimalCode();
    info.uses_lighting = uses_lighting;
    info.category = category;
    return info;
}

/// @brief 遅延コンパイルテスト用の非IGESエンティティ (形状なしの最小実装)
class LazyShaderEntity : public i_ent::NonIgesEntityBase {
 public:
    /// @brief コンストラクタ
    LazyShaderEntity() = default;
};

/// @brief DrawImplの呼び出し回数 (描画到達の観測用)
int lazy_shader_draw_count = 0;

/// @brief ユーザーシェーダーで描画する最小描画クラス (GLリソースを持たない)
class LazyShaderGraphics : public i_graph::EntityGraphics<LazyShaderEntity> {
 public:
    /// @brief コンストラクタ
    /// @param entity 対象エンティティ
    /// @param gl OpenGLラッパー
    /// @param shader_id 使用するユーザーシェーダーのID
    LazyShaderGraphics(const std::shared_ptr<const LazyShaderEntity>& entity,
                       const std::shared_ptr<i_graph::IOpenGL>& gl,
                       const ShaderId shader_id)
            : EntityGraphics<LazyShaderEntity>(entity, gl, shader_id, false) {}

    /// @brief VAOを持たないため、描画経路の検証用に常に描画可能とする
    bool IsDrawable() const override { return true; }

 protected:
    /// @brief 同期 (GLリソースなしのため何もしない)
    void DoSynchronize() override {}

    /// @brief 描画 (到達を記録する)
    void DrawImpl(igesio::graphics::gl::Uint,
                  const std::pair<float, float>&) const override {
        ++lazy_shader_draw_count;
    }
};

/// @brief スコープ終了時にGraphicsRegistryの登録を解除するガード
/// @tparam T 解除対象のエンティティ型
template <typename T>
class RegistryGuard {
 public:
    /// @brief デストラクタ (登録を解除する)
    ~RegistryGuard() { i_graph::GraphicsRegistry::Unregister<T>(); }
};

}  // namespace



/**
 * 正常系: 組み込みシェーダーの設定
 */

// 組み込みIDはGet/Findで引け、名前が固定値と一致する
TEST(ShaderRegistryTest, BuiltinsSeeded_GetAndFind) {
    const auto* info = ShaderRegistry::Get(ShaderId::kGeneralCurve);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->name, "GeneralCurve");
    EXPECT_FALSE(info->code.IsIncomplete());

    const auto found = ShaderRegistry::Find("GeneralSurface");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, ShaderId::kGeneralSurface);

    // 未登録の名前はnullopt
    EXPECT_FALSE(ShaderRegistry::Find("NoSuchShaderName").has_value());
}

// AllIdsは組み込み11種+センチネル2種を含む
TEST(ShaderRegistryTest, AllIds_ContainsBuiltinsAndSentinels) {
    const auto ids = ShaderRegistry::AllIds();
    auto contains = [&ids](const ShaderId id) {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    };
    EXPECT_TRUE(contains(ShaderId::kGeneralCurve));
    EXPECT_TRUE(contains(ShaderId::kPoint));
    EXPECT_TRUE(contains(ShaderId::kSurfaceEdge));
    EXPECT_TRUE(contains(ShaderId::kRationalBSplineSurface));
    EXPECT_TRUE(contains(ShaderId::kComposite));
    EXPECT_TRUE(contains(ShaderId::kNone));
    EXPECT_GE(ids.size(), 13u);
}

// メタ参照ヘルパーが組み込みのメタ情報を正しく反映する
TEST(ShaderRegistryTest, BuiltinMetadata_DrivesHelpers) {
    // 照明: 面塗り (曲面系) のみ使用する
    EXPECT_TRUE(i_graph::UsesLighting(ShaderId::kGeneralSurface));
    EXPECT_TRUE(i_graph::UsesLighting(ShaderId::kRationalBSplineSurface));
    EXPECT_FALSE(i_graph::UsesLighting(ShaderId::kGeneralCurve));
    EXPECT_FALSE(i_graph::UsesLighting(ShaderId::kSurfaceEdge));
    EXPECT_FALSE(i_graph::UsesLighting(ShaderId::kNone));

    // 面塗り判定
    EXPECT_TRUE(i_graph::IsSurfaceFill(ShaderId::kGeneralSurface));
    EXPECT_TRUE(i_graph::IsSurfaceFill(ShaderId::kRationalBSplineSurface));
    EXPECT_FALSE(i_graph::IsSurfaceFill(ShaderId::kSurfaceEdge));
    EXPECT_FALSE(i_graph::IsSurfaceFill(ShaderId::kGeneralCurve));

    // シェーダーコードの有無 (センチネルと未登録IDはfalse)
    EXPECT_TRUE(i_graph::HasSpecificShaderCode(ShaderId::kGeneralCurve));
    EXPECT_FALSE(i_graph::HasSpecificShaderCode(ShaderId::kComposite));
    EXPECT_FALSE(i_graph::HasSpecificShaderCode(ShaderId::kNone));
    EXPECT_FALSE(i_graph::HasSpecificShaderCode(ShaderId{12345u}));

    // 文字列化 (レジストリの名前; 未登録IDはUnknown)
    EXPECT_EQ(i_graph::ToString(ShaderId::kSurfaceEdge), "SurfaceEdge");
    EXPECT_EQ(i_graph::ToString(ShaderId::kComposite), "Composite");
    EXPECT_EQ(i_graph::ToString(ShaderId{12345u}), "Unknown");
}



/**
 * 正常系: ユーザー登録
 */

// 登録するとユーザーID (256以降) が採番され、Get/Find/ToStringで引ける
TEST(ShaderRegistryTest, Register_AssignsUserIdAndIsRetrievable) {
    const auto rev0 = ShaderRegistry::Revision();
    const auto id = ShaderRegistry::Register(
            MakeInfo("TestUserShaderBasic", true,
                     ShaderDrawCategory::kSurfaceFill));

    EXPECT_GE(id.Value(), kFirstUserIdValue);
    EXPECT_GT(ShaderRegistry::Revision(), rev0);

    const auto* info = ShaderRegistry::Get(id);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->name, "TestUserShaderBasic");
    EXPECT_TRUE(info->uses_lighting);
    EXPECT_EQ(info->category, ShaderDrawCategory::kSurfaceFill);

    const auto found = ShaderRegistry::Find("TestUserShaderBasic");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, id);

    // メタ参照ヘルパーもユーザー定義のメタを反映する
    EXPECT_EQ(i_graph::ToString(id), "TestUserShaderBasic");
    EXPECT_TRUE(i_graph::UsesLighting(id));
    EXPECT_TRUE(i_graph::IsSurfaceFill(id));
    EXPECT_TRUE(i_graph::HasSpecificShaderCode(id));
}

// 複数登録で相異なるIDが採番される
TEST(ShaderRegistryTest, Register_AssignsDistinctIds) {
    const auto id1 = ShaderRegistry::Register(MakeInfo("TestUserShaderA"));
    const auto id2 = ShaderRegistry::Register(MakeInfo("TestUserShaderB"));
    EXPECT_NE(id1, id2);
}



/**
 * 異常系: 不正な登録
 */

// 空の名前はinvalid_argument
TEST(ShaderRegistryTest, Register_ThrowsInvalidArgumentWhenNameIsEmpty) {
    EXPECT_THROW(ShaderRegistry::Register(MakeInfo("")),
                 std::invalid_argument);
}

// 組み込み名への再登録 (差し替え) はinvalid_argument
TEST(ShaderRegistryTest, Register_ThrowsInvalidArgumentWhenNameIsBuiltin) {
    EXPECT_THROW(ShaderRegistry::Register(MakeInfo("GeneralSurface")),
                 std::invalid_argument);
    // 組み込みの定義は変化していない
    const auto* info = ShaderRegistry::Get(ShaderId::kGeneralSurface);
    ASSERT_NE(info, nullptr);
    EXPECT_TRUE(info->uses_lighting);
}

// ユーザー名の重複登録はinvalid_argument (1回目は成功する)
TEST(ShaderRegistryTest, Register_ThrowsInvalidArgumentWhenNameIsDuplicated) {
    EXPECT_NO_THROW(ShaderRegistry::Register(MakeInfo("TestUserShaderDup")));
    EXPECT_THROW(ShaderRegistry::Register(MakeInfo("TestUserShaderDup")),
                 std::invalid_argument);
}

// 不完全なコード (fragment欠落・tcs/tes片方のみ) はinvalid_argument.
// 完全な最小コード (vertex+fragment) は受理される (境界の内側)
TEST(ShaderRegistryTest, Register_ThrowsInvalidArgumentWhenCodeIsIncomplete) {
    const auto rev0 = ShaderRegistry::Revision();

    // fragment欠落
    ShaderInfo vertex_only = MakeInfo("TestUserShaderVertexOnly");
    vertex_only.code = ShaderCode("void main() {}", "");
    EXPECT_THROW(ShaderRegistry::Register(vertex_only), std::invalid_argument);

    // tcsのみ (tes欠落)
    ShaderInfo tcs_only = MakeInfo("TestUserShaderTcsOnly");
    tcs_only.code = ShaderCode("void main() {}", "void main() {}",
                               "", "void main() {}", "");
    EXPECT_THROW(ShaderRegistry::Register(tcs_only), std::invalid_argument);

    // 失敗した登録は痕跡を残さない (リビジョン・名前とも変化なし)
    EXPECT_EQ(ShaderRegistry::Revision(), rev0);
    EXPECT_FALSE(ShaderRegistry::Find("TestUserShaderVertexOnly").has_value());

    // 境界の内側: vertex+fragmentの最小構成は受理される
    EXPECT_NO_THROW(
            ShaderRegistry::Register(MakeInfo("TestUserShaderMinimalOk")));
}



/**
 * レンダラとの連携 (遅延コンパイル)
 */

// Initialize後に登録したシェーダーが次のDrawでコンパイルされ、
// そのシェーダーを使う描画オブジェクトが描画される
TEST(ShaderRegistryRendererTest, DrawCompilesShaderRegisteredAfterInitialize) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    // この時点では未登録のユーザーシェーダーIDを描画クラスへ配線する.
    // 値を先取りできないため、登録→ID確定→描画クラス登録の順に行い、
    // 「レンダラのInitialize後の登録」であることだけを検証の対象とする
    const auto user_id = ShaderRegistry::Register(
            MakeInfo("TestUserShaderLazyCompile"));

    RegistryGuard<LazyShaderEntity> guard;
    i_graph::GraphicsRegistry::Register<LazyShaderEntity>(
            [user_id](const std::shared_ptr<const LazyShaderEntity>& entity,
                      const std::shared_ptr<i_graph::IOpenGL>& gl_ptr)
                    -> std::unique_ptr<i_graph::IEntityGraphics> {
                return std::make_unique<LazyShaderGraphics>(
                        entity, gl_ptr, user_id);
            });

    auto root = i_mod::MakeAssembly();
    root->AddEntity(std::make_shared<LazyShaderEntity>());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // Draw冒頭の遅延コンパイルでユーザーシェーダーのプログラムが作られ、
    // バケットへ収集されてDrawImplまで到達する
    lazy_shader_draw_count = 0;
    renderer.Draw();
    EXPECT_EQ(lazy_shader_draw_count, 1);

    // 2回目のDrawでも描画される (再コンパイルなしの定常経路)
    renderer.Draw();
    EXPECT_EQ(lazy_shader_draw_count, 2);
}
