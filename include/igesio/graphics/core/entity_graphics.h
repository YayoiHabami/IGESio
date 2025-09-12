/**
 * @file graphics/core/entity_graphics.h
 * @brief エンティティの描画情報を管理するクラスを定義する
 * @author Yayoi Habami
 * @date 2025-08-05
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_ENTITY_GRAPHICS_H_
#define IGESIO_GRAPHICS_CORE_ENTITY_GRAPHICS_H_

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/entity_base.h"
#include "igesio/graphics/core/i_entity_graphics.h"



namespace igesio::graphics {

/// @brief エンティティの描画情報を管理するクラス
/// @tparam T エンティティの型
/// @note 以下のメンバ関数のオーバーライドが必要
///       - `EntityGraphics`由来:
///         - `Cleanup`(public): VAO以外のOpenGLリソースを持つ場合
///         - `DrawImpl`(protected): 常にオーバーライドが必要
template<typename T, ShaderType ShaderType_, typename = std::enable_if_t<
        std::is_base_of_v<entities::IEntityIdentifier, T>>>
class EntityGraphics : public IEntityGraphics {
 protected:
    /// @brief エンティティのポインタ
    std::shared_ptr<const T> entity_;

    /// @brief エンティティの描画用の頂点配列オブジェクト (VAO) のID
    GLuint vao_ = 0;
    /// @brief エンティティの描画モード (GL_LINE_STRIP, GL_LINE_LOOPなど)
    GLenum draw_mode_ = GL_LINE_STRIP;

    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @param use_entity_transform シェーダーのmodel変数に
    ///        entity_が参照する変換行列を掛け合わせるか
    /// @throw std::invalid_argument entityがnullptrの場合
    EntityGraphics(const std::shared_ptr<const T> entity,
                   const std::shared_ptr<IOpenGL> gl,
                   bool use_entity_transform)
            : IEntityGraphics(gl, use_entity_transform),
              entity_(entity) {
        if (!entity_) {
            throw std::invalid_argument("Entity pointer cannot be null");
        }

        // 色をエンティティから取得 (IGESの色は0-100の範囲)
        ResetColor();
    }

 public:
    // コピーコンストラクタとコピー代入演算子を禁止
    EntityGraphics(const EntityGraphics&) = delete;
    EntityGraphics& operator=(const EntityGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のEntityGraphics
    /// @note ムーブ元のリソース所有権を放棄
    EntityGraphics(EntityGraphics&& other) noexcept
        : IEntityGraphics(std::move(other)),
          entity_(std::move(other.entity_)),
          vao_(other.vao_),
          draw_mode_(other.draw_mode_) {
        // ムーブ元のポインタをnullptrにし、リソースの二重解放を防ぐ
        other.entity_ = nullptr;
        other.vao_ = 0;
    }

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のEntityGraphics
    /// @return *this
    /// @note ムーブ元のリソース所有権を放棄
    EntityGraphics& operator=(EntityGraphics&& other) noexcept {
        if (this != &other) {
            // 自身の既存リソースを解放
            Cleanup();

            // IEntityGraphicsのムーブ
            IEntityGraphics::operator=(std::move(other));

            // メンバをムーブ
            entity_ = std::move(other.entity_);
            vao_ = other.vao_;
            draw_mode_ = other.draw_mode_;

            // ムーブ元のポインタをnullptrにし、リソースの二重解放を防ぐ
            other.entity_ = nullptr;
            other.vao_ = 0;
        }
        return *this;
    }

    /// @brief エンティティのIDを取得する
    /// @return エンティティのID
    /// @note エンティティが未設定の場合はkUnsetIDを返す
    uint64_t GetEntityID() const override {
        if (entity_) {
            return entity_->GetID();
        }
        return kUnsetID;
    }

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param shader_type 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note shader_typeに合致する要素がない場合は何もしない
    void Draw(GLuint shader, const ShaderType shader_type,
              const std::pair<float, float>& viewport) const override {
        // シェーダータイプが合致していることのみ確認
        if (shader_type != ShaderType_) return;

        Draw(shader, viewport);
    }

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void Draw(GLuint shader, const std::pair<float, float>& viewport) const {
        if (!entity_) return;
        if (!IsDrawable()) return;

        gl_->LineWidth(GetLineWidth());

        // 全シェーダーで共通のuniform変数を設定
        igesio::Matrix4f model = GetWorldTransform();
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(shader, "model"),
                              1, GL_FALSE, model.data());
        gl_->Uniform4fv(gl_->GetUniformLocation(shader, "mainColor"),
                        1, GetColor().data());

        DrawImpl(shader, viewport);
    }



    /**
     * リソースの確認・変更
     */

    /// @brief 描画用のシェーダーのタイプを取得する
    /// @return 描画用のシェーダーのタイプ
    ShaderType GetShaderType() const override {
        return ShaderType_;
    }

    /// @brief OpenGLリソースを解放する
    /// @note GPUへパラメータを渡し、GPU上で離散点を計算する場合は
    ///       オーバーライド不要.
    void Cleanup() override {
        if (vao_ != 0) {
            gl_->DeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
    }

    /// @brief 描画可能な状態かどうかを確認する
    /// @note GPUへパラメータを渡し、GPU上で離散点を計算する場合は
    ///       オーバーライド不要.
    bool IsDrawable() const override {
        return entity_ && vao_ != 0;
    }



    /**
     * パラメータの取得/設定
     */

    /// @brief グローバル座標系への変換行列を取得する
    /// @return グローバル座標系への変換行列.
    ///         use_entity_transform_がtrueの場合は、
    ///         `SetWorldTransform`で設定された変換行列に
    ///         entity_が参照する変換行列を掛け合わせたものを返す.
    igesio::Matrix4f GetWorldTransform() const override {
        if (use_entity_transform_ && entity_) {
            auto ptr = std::dynamic_pointer_cast<const entities::EntityBase>(entity_);
            if (ptr) {
                // entity_が参照する変換行列を掛け合わせる
                auto entity_transform = ptr->GetTransformationMatrix()
                        .GetTransformation().template cast<float>();
                return world_transform_ * entity_transform;
            }
        }
        return world_transform_;
    }

    /// @brief 現在のメインの色を取得する
    /// @return メインの色 (RGBA; [0, 1]の範囲)
    /// @note SetColorで色をオーバーライドした場合はその色を返す.
    ///       そうでない場合は、エンティティが保持する色を返す.
    std::array<GLfloat, 4> GetColor() const override {
        if (!is_color_overridden_) {
            auto base = std::dynamic_pointer_cast<const entities::EntityBase>(entity_);
            if (base) {
                auto [r, g, b] = base->GetColor().GetRGB();
                return {static_cast<GLfloat>(r) / 100.0f,
                        static_cast<GLfloat>(g) / 100.0f,
                        static_cast<GLfloat>(b) / 100.0f,
                        1.0f};  // 不透明度は1.0f (完全に不透明)
            }
        }
        return {color_[0], color_[1], color_[2], color_[3]};
    }

    /// @brief 色をデフォルトのエンティティの色に戻す
    void ResetColor() override {
        is_color_overridden_ = false;
        auto base = std::dynamic_pointer_cast<const entities::EntityBase>(entity_);
        if (base) {
            auto [r, g, b] = base->GetColor().GetRGB();
            color_[0] = static_cast<GLfloat>(r) / 100.0f;
            color_[1] = static_cast<GLfloat>(g) / 100.0f;
            color_[2] = static_cast<GLfloat>(b) / 100.0f;
            color_[3] = 1.0f;  // 不透明度は1.0f (完全に不透明)
        }
    }

    /// @brief 線の太さを取得する
    /// @return 線の太さ
    double GetLineWidth() const override {
        auto base = std::dynamic_pointer_cast<const entities::EntityBase>(entity_);
        if (base) {
            // エンティティの線の太さ番号を取得
            int line_weight_number = base->GetLineWeightNumber();

            // 0以下の場合はデフォルトの線幅を返す
            if (line_weight_number <= 0) return kDefaultLineWidth;

            if (global_param_) {
                return global_param_->GetLineWeight(line_weight_number);
            }
        }
        return kDefaultLineWidth;
    }



 protected:
    /// @brief エンティティの描画を実装する関数
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note OpenGLの描画コマンドを実行する
    virtual void DrawImpl(GLuint, const std::pair<float, float>&) const = 0;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_ENTITY_GRAPHICS_H_
