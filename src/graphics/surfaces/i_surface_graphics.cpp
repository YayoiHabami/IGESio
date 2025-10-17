/**
 * @file graphics/surfaces/i_surface_graphics.cpp
 * @brief ISurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2025-10-10
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/surfaces/i_surface_graphics.h"

#include <memory>
#include <utility>
#include <vector>

namespace {

using igesio::graphics::ISurfaceGraphics;

/// @brief u/vパラメータの値を計算する
/// @param i インデックス
/// @param div 分割数
/// @param range パラメータ範囲 {start, end}
double ComputeParam(const int i, const int div, const std::array<double, 2>& range) {
    return range[0] + (range[1] - range[0]) * i / static_cast<double>(div);
}

/// @brief u/vパラメータのデフォルト分解能数
constexpr int kDefaultDiv = 20;

}  // namespace



/**
 * constructor / destructor
 */

ISurfaceGraphics::ISurfaceGraphics(
        const std::shared_ptr<const entities::ISurface> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

ISurfaceGraphics::~ISurfaceGraphics() {
    Cleanup();
}



/**
 * protected
 */

void ISurfaceGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    // VAOをバインドして描画
    gl_->BindVertexArray(vao_);
    // glDrawElementsでインデックスバッファを用いた描画を行う
    gl_->DrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_INT, 0);
    gl_->BindVertexArray(0);
}

void ISurfaceGraphics::Synchronize() {
    // 既存のリソースを解放
    Cleanup();

    SyncTexture();

    // 頂点・法線データとインデックスデータを生成
    GenerateSurfaceData();

    // VAO, VBO, EBOを生成
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);
    gl_->GenBuffers(1, &ebo_);

    // VAOをバインド
    gl_->BindVertexArray(vao_);

    // VBOに頂点・法線データを転送
    gl_->BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_->BufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(float),
                    vertices_.data(), GL_STATIC_DRAW);
    // 頂点属性を設定
    gl_->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);  // 位置
    gl_->VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                             (void*)(3 * sizeof(float)));
    gl_->EnableVertexAttribArray(1);  // 法線
    gl_->VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                             (void*)(6 * sizeof(float)));
    gl_->EnableVertexAttribArray(2);  // テクスチャ座標

    // EBOにインデックスデータを転送
    gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    gl_->BufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(GLuint),
                    indices_.data(), GL_STATIC_DRAW);

    // VAOのバインドを解除
    gl_->BindVertexArray(0);
}

void ISurfaceGraphics::Cleanup() {
    EntityGraphics::Cleanup();

    // VBOとEBOを解放
    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (ebo_ != 0) {
        gl_->DeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }

    // 頂点・法線データとインデックスデータをクリア
    vertices_.resize(0, 0);
    indices_.clear();
}

void ISurfaceGraphics::GenerateSurfaceData() {
    int u_div = kDefaultDiv;
    int v_div = kDefaultDiv;

    // 頂点・法線データとインデックスデータの初期化
    vertices_.resize(8, (u_div + 1) * (v_div + 1));
    indices_.clear();
    indices_.reserve(u_div * v_div * 6);
    // 各頂点が有効かどうか (1.0f: 有効, 0.0f: 無効)
    // NOTE: IGESioのMatrixがbool型をサポートしていないため、float型で代用
    MatrixXf is_vertex_valid(u_div + 1, v_div + 1);

    // 頂点・法線データを生成
    for (int i = 0; i <= u_div; ++i) {
        double u = ComputeParam(i, u_div, entity_->GetURange());
        for (int j = 0; j <= v_div; ++j) {
            double v = ComputeParam(j, v_div, entity_->GetVRange());
            auto pos_opt = entity_->TryGetPointAt(u, v);
            auto normal_opt = entity_->TryGetNormalAt(u, v);

            Vector3d pos, normal;
            if (pos_opt && normal_opt) {
                // 頂点が有効な場合はその値を使用
                pos = *pos_opt;
                normal = *normal_opt;
                is_vertex_valid(i, j) = 1.0f;
            } else {
                // 頂点が無効な場合はダミー値を設定
                pos = Vector3d::Zero();
                normal = Vector3d::UnitZ();
                is_vertex_valid(i, j) = 0.0f;
            }
            vertices_.block<3, 1>(0, i * (v_div + 1) + j) = pos.cast<float>();
            vertices_.block<3, 1>(3, i * (v_div + 1) + j) = normal.cast<float>();
            // 0-1に正規化した(u, v)をテクスチャ座標として設定
            vertices_(6, i * (v_div + 1) + j) = static_cast<float>(i) / u_div;
            vertices_(7, i * (v_div + 1) + j) = static_cast<float>(j) / v_div;
        }
    }

    // インデックスデータを生成
    // 全頂点が有効 or 左上/右下の三角形が有効なら左上/右下の三角形を描画
    // 1つ以上の頂点が無効 and 左下/右上の三角形が有効なら左下/右上の三角形を描画
    for (int i = 0; i < u_div; ++i) {
        for (int j = 0; j < v_div; ++j) {
            bool ij = is_vertex_valid(i, j) > 0.5f;
            bool i1j = is_vertex_valid(i + 1, j) > 0.5f;
            bool ij1 = is_vertex_valid(i, j + 1) > 0.5f;
            bool i1j1 = is_vertex_valid(i + 1, j + 1) > 0.5f;
            bool is_all_valid = ij && i1j && ij1 && i1j1;

            if (is_all_valid || (ij && i1j && ij1)) {
                // 左上の三角形
                indices_.push_back(i * (v_div + 1) + j);
                indices_.push_back((i + 1) * (v_div + 1) + j);
                indices_.push_back(i * (v_div + 1) + (j + 1));
            }
            if (is_all_valid || (i1j && i1j1 && ij1)) {
                // 右下の三角形
                indices_.push_back((i + 1) * (v_div + 1) + j);
                indices_.push_back((i + 1) * (v_div + 1) + (j + 1));
                indices_.push_back(i * (v_div + 1) + (j + 1));
            }
            if (!is_all_valid && (ij && i1j1 && ij1)) {
                // 左下の三角形
                indices_.push_back(i * (v_div + 1) + j);
                indices_.push_back((i + 1) * (v_div + 1) + (j + 1));
                indices_.push_back(i * (v_div + 1) + (j + 1));
            }
            if (!is_all_valid && (ij && i1j && i1j1)) {
                // 右上の三角形
                indices_.push_back(i * (v_div + 1) + j);
                indices_.push_back((i + 1) * (v_div + 1) + j);
                indices_.push_back((i + 1) * (v_div + 1) + (j + 1));
            }
        }
    }
}
