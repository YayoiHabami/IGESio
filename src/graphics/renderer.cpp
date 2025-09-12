/**
 * @file graphics/renderer.cpp
 * @brief IGESエンティティの描画を行うレンダラークラスを定義する
 * @author Yayoi Habami
 * @date 2025-08-07
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/renderer.h"

#include <memory>
#include <string>
#include <utility>

#include "./shaders.h"

namespace {

namespace i_graph = igesio::graphics;
using EntityRenderer = igesio::graphics::EntityRenderer;

}  // namespace



EntityRenderer::EntityRenderer(std::shared_ptr<IOpenGL> gl,
                               const int width, const int height)
        : gl_(std::move(gl)), display_width_(width), display_height_(height) {
    default_global_param_ = std::make_shared<const models::GraphicsGlobalParam>(
            igesio::models::kDefaultModelSpaceScale,
            igesio::models::kDefaultLineWeightGradations,
            igesio::models::kDefaultLineWeightGradations);
}



bool EntityRenderer::IsInitialized() const {
    return !shader_programs_.empty();
}

void EntityRenderer::Initialize() {
    InitShaders();

    // 深度テストを有効化
    gl_->Enable(GL_DEPTH_TEST);
}

void EntityRenderer::Cleanup() {
    // 描画オブジェクトのクリーンアップ
    for (auto& [shader_type, objects] : draw_objects_) {
        for (auto& [id, object] : objects) {
            object->Cleanup();
        }
        objects.clear();
    }
    draw_objects_.clear();

    // シェーダープログラムの削除
    for (auto& [shader_type, program_id] : shader_programs_) {
        gl_->DeleteProgram(program_id);
    }
    shader_programs_.clear();

    // OpenGLリソースの解放
    gl_->Disable(GL_DEPTH_TEST);
}



/**
 * エンティティの取得/設定
 */

void EntityRenderer::AddGraphicsObject(
        std::unique_ptr<IEntityGraphics>&& graphics) {
    if (!graphics) return;

    if (HasEntity(graphics->GetEntityID())) {
        // すでに同じIDのエンティティが存在する場合は何もしない
        return;
    }

    // 新しい描画オブジェクトを追加
    draw_objects_[graphics->GetShaderType()][graphics->GetEntityID()]
            = std::move(graphics);
}

void EntityRenderer::RemoveEntity(const uint64_t id) {
    for (auto& [shader_type, objects] : draw_objects_) {
        auto it = objects.find(id);
        if (it != objects.end()) {
            it->second->Cleanup();  // OpenGLリソースを解放
            objects.erase(it);  // 描画オブジェクトを削除
            return;  // 削除が完了したら終了
        }
    }
}

bool EntityRenderer::HasEntity(const uint64_t id) const {
    // draw_objects_を走査
    for (const auto& [shader_type, objects] : draw_objects_) {
        if (objects.find(id) != objects.end()) {
            return true;
        }
    }
    return false;
}

i_graph::ShaderType
EntityRenderer::GetEntityShaderType(const uint64_t id) const {
    for (const auto& [shader_type, objects] : draw_objects_) {
        if (objects.find(id) != objects.end()) {
            return shader_type;
        }
    }
    return ShaderType::kNone;
}

bool EntityRenderer::HasGraphicsObject(const ShaderType shader_type) const {
    // draw_objects_のキーを走査
    auto it = draw_objects_.find(shader_type);
    if (it != draw_objects_.end() && !it->second.empty()) {
        return true;  // 指定されたシェーダータイプに対応する描画オブジェクトが存在する
    }

    // kCompositeな描画オブジェクトを走査
    auto composite_it = draw_objects_.find(ShaderType::kComposite);
    if (composite_it != draw_objects_.end()) {
        for (const auto& [id, object] : composite_it->second) {
            auto shaders = object->GetShaderTypes();
            if (shaders.find(shader_type) != shaders.end()) {
                return true;  // 子要素に指定されたシェーダータイプが存在する
            }
        }
    }

    return false;
}



/**
 * エンティティ以外の要素の取得/設定
 */

std::pair<int, int> EntityRenderer::GetDisplaySize() const {
    return {display_width_, display_height_};
}

void EntityRenderer::SetDisplaySize(const int width, const int height) {
    display_width_ = width;
    display_height_ = height;
    is_resized_ = true;  // サイズが変更されたことを記録
}

std::array<float, 4> EntityRenderer::GetBackgroundColor() const {
    return background_color_;
}

/// @brief 背景色の参照を取得する
/// @return 背景色の参照 (RGBA) [0.0 - 1.0]
std::array<float, 4>& EntityRenderer::GetBackgroundColorRef() {
    return background_color_;
}

/// @brief 背景色を設定する
/// @param color 背景色 (RGBA) [0.0 - 1.0]
void EntityRenderer::SetBackgroundColor(
        const float red, const float green,
        const float blue, const float alpha) {
    background_color_ = {red, green, blue, alpha};
}



/**
 * 描画
 */

void EntityRenderer::Draw() {
    // 描画対象のサイズが0なら何もしない
    if (display_width_ <= 0 || display_height_ <= 0) return;

    // 背景色と深度バッファをクリア
    gl_->ClearColor(background_color_[0], background_color_[1],
                 background_color_[2], background_color_[3]);
    gl_->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 描画するエンティティが1つもない場合は何もしない
    if (IsEmpty()) return;

    if (is_resized_) {
        gl_->Viewport(0, 0, display_width_, display_height_);
        is_resized_ = false;  // リサイズフラグをリセット
    }

    // ビュー行列と投影行列を取得
    auto view_matrix = camera_.GetViewMatrix();
    auto projection_matrix = camera_.GetProjectionMatrix(
        static_cast<float>(display_width_) / display_height_);

    // 各シェーダープログラムごとに描画
    for (const auto& [shader_type, program_id] : shader_programs_) {
        if (!HasGraphicsObject(shader_type)) {
            // このシェーダータイプの描画オブジェクトがない、または
            // 対応する描画オブジェクトが1つもない場合はスキップ
            continue;
        }

        gl_->UseProgram(program_id);

        // 共通のuniform変数を設定
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(program_id, "view"),
                              1, GL_FALSE, view_matrix.data());
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(program_id, "projection"),
                              1, GL_FALSE, projection_matrix.data());

        // 各エンティティを描画
        DrawChildren(program_id, shader_type,
                     std::pair<float, float>{display_width_, display_height_});
    }
}

void EntityRenderer::DrawChildren(
        GLuint program_id, const ShaderType shader_type,
        const std::pair<float, float>& viewport) const {
    // 対応するシェーダーコードを持たない場合は何もしない
    if (!HasSpecificShaderCode(shader_type)) return;

    // shader_typeの直接の子要素を描画
    auto it = draw_objects_.find(shader_type);
    if (it != draw_objects_.end()) {
        for (const auto& [id, object] : it->second) {
            object->Draw(program_id, shader_type, viewport);
        }
    }

    // kCompositeな描画オブジェクトの子要素を描画
    auto composite_it = draw_objects_.find(ShaderType::kComposite);
    if (composite_it != draw_objects_.end()) {
        for (const auto& [id, object] : composite_it->second) {
            // 仮にshader_typeに対応する子要素がなければ何も実行されないため、
            // objectがshader_typeに対応する子要素を持つかは確認しない
            object->Draw(program_id, shader_type, viewport);
        }
    }
}



/**
 * private member functions
 */

GLuint EntityRenderer::CompileVertexShader(const std::string& vertex_source) {
    if (vertex_source.empty()) {
        throw igesio::ImplementationError("Vertex shader source is empty");
    }

    GLuint vertex_shader = gl_->CreateShader(GL_VERTEX_SHADER);
    const char* source_cstr = vertex_source.c_str();
    gl_->ShaderSource(vertex_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(vertex_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
        gl_->DeleteShader(vertex_shader);
        throw igesio::ImplementationError(
            "Failed to compile vertex shader: " + std::string(info_log));
    }
    return vertex_shader;
}

GLuint EntityRenderer::CompileGeometryShader(const std::string& geometry_source) {
    if (geometry_source.empty()) {
        return 0;  // ジオメトリシェーダーが空の場合は0を返す
    }

    GLuint geometry_shader = gl_->CreateShader(GL_GEOMETRY_SHADER);
    const char* source_cstr = geometry_source.c_str();
    gl_->ShaderSource(geometry_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(geometry_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(geometry_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(geometry_shader, 512, nullptr, info_log);
        gl_->DeleteShader(geometry_shader);
        throw igesio::ImplementationError(
            "Failed to compile geometry shader: " + std::string(info_log));
    }
    return geometry_shader;
}

std::pair<float, float> EntityRenderer::CompileTCSAndTES(
        const std::string& tcs_source, const std::string& tes_source) {
    if (tcs_source.empty() || tes_source.empty()) {
        return {0, 0};  // TCSまたはTESが空の場合は0を返す
    }

    GLuint tcs_shader = gl_->CreateShader(GL_TESS_CONTROL_SHADER);
    const char* tcs_cstr = tcs_source.c_str();
    gl_->ShaderSource(tcs_shader, 1, &tcs_cstr, nullptr);
    gl_->CompileShader(tcs_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(tcs_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(tcs_shader, 512, nullptr, info_log);
        gl_->DeleteShader(tcs_shader);
        throw igesio::ImplementationError(
            "Failed to compile tessellation control shader: " + std::string(info_log));
    }

    GLuint tes_shader = gl_->CreateShader(GL_TESS_EVALUATION_SHADER);
    const char* tes_cstr = tes_source.c_str();
    gl_->ShaderSource(tes_shader, 1, &tes_cstr, nullptr);
    gl_->CompileShader(tes_shader);

    // エラーチェック
    gl_->GetShaderiv(tes_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(tes_shader, 512, nullptr, info_log);
        gl_->DeleteShader(tes_shader);
        throw igesio::ImplementationError(
            "Failed to compile tessellation evaluation shader: " + std::string(info_log));
    }

    return {tcs_shader, tes_shader};
}

GLuint EntityRenderer::CompileFragmentShader(const std::string& fragment_source) {
    if (fragment_source.empty()) {
        throw igesio::ImplementationError("Fragment shader source is empty");
    }

    GLuint fragment_shader = gl_->CreateShader(GL_FRAGMENT_SHADER);
    const char* source_cstr = fragment_source.c_str();
    gl_->ShaderSource(fragment_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(fragment_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
        gl_->DeleteShader(fragment_shader);
        throw igesio::ImplementationError(
            "Failed to compile fragment shader: " + std::string(info_log));
    }
    return fragment_shader;
}

GLuint EntityRenderer::CreateShaderProgram(
        GLuint vertex_shader, GLuint fragment_shader,
        GLuint geometry_shader, GLuint tcs_shader, GLuint tes_shader) {
    GLuint program_id = gl_->CreateProgram();

    if (vertex_shader == 0 || fragment_shader == 0) {
        throw igesio::ImplementationError("Vertex or Fragment shader is not provided.");
    }

    // 存在する各シェーダーをプログラムにリンクする
    gl_->AttachShader(program_id, vertex_shader);
    if (geometry_shader != 0) gl_->AttachShader(program_id, geometry_shader);
    if (tcs_shader != 0 && tes_shader != 0) {
        gl_->AttachShader(program_id, tcs_shader);
        gl_->AttachShader(program_id, tes_shader);
    }
    gl_->AttachShader(program_id, fragment_shader);
    gl_->LinkProgram(program_id);

    // リンクのエラーチェック
    int success;
    char info_log[512];
    gl_->GetProgramiv(program_id, GL_LINK_STATUS, &success);
    if (!success) {
        gl_->GetProgramInfoLog(program_id, 512, nullptr, info_log);
        throw igesio::ImplementationError(
            "Failed to link shader program: " + std::string(info_log));
    }

    return program_id;
}

void EntityRenderer::InitShaders() {
    for (int i = 0; i < static_cast<int>(ShaderType::kNone); ++i) {
        auto shader_type = static_cast<ShaderType>(i);
        auto shader_opt = shaders::GetShaderCode(shader_type);
        if (!shader_opt || shader_opt->IsIncomplete()) {
            continue;  // シェーダーが未実装の場合はスキップ
        }
        auto code = *shader_opt;

        GLuint vertex_shader = 0, geometry_shader = 0,
               tcs_shader = 0, tes_shader = 0, fragment_shader = 0,
               program_id = 0;

        try {
            // 各シェーダーをコンパイルする (存在しない場合は0が返る)
            vertex_shader = CompileVertexShader(code.vertex);
            geometry_shader = CompileGeometryShader(code.geometry);
            std::tie(tcs_shader, tes_shader) = CompileTCSAndTES(code.tcs, code.tes);
            fragment_shader = CompileFragmentShader(code.fragment);

            // 各シェーダーをリンクしてプログラムを作成
            program_id = CreateShaderProgram(
                vertex_shader, fragment_shader,
                geometry_shader, tcs_shader, tes_shader);

            // リンク後、各シェーダーオブジェクトを削除
            gl_->DeleteShader(vertex_shader);
            if (geometry_shader != 0) gl_->DeleteShader(geometry_shader);
            if (tcs_shader != 0) gl_->DeleteShader(tcs_shader);
            if (tes_shader != 0) gl_->DeleteShader(tes_shader);
            gl_->DeleteShader(fragment_shader);
        } catch (const igesio::ImplementationError& e) {
            // エラーが発生した場合はリソースを解放
            if (vertex_shader != 0) gl_->DeleteShader(vertex_shader);
            if (geometry_shader != 0) gl_->DeleteShader(geometry_shader);
            if (tcs_shader != 0) gl_->DeleteShader(tcs_shader);
            if (tes_shader != 0) gl_->DeleteShader(tes_shader);
            if (fragment_shader != 0) gl_->DeleteShader(fragment_shader);
            if (program_id != 0) gl_->DeleteProgram(program_id);
            throw e;  // エラーを再スロー
        }

        shader_programs_[shader_type] = program_id;
    }
}

bool EntityRenderer::IsEmpty() const {
    if (draw_objects_.empty()) return true;

    // 1つもエンティティ（描画オブジェクト）が設定されていない場合は空
    for (const auto& [shader_type, objects] : draw_objects_) {
        if (!objects.empty()) return false;
    }
    return true;
}
