/**
 * @file graphics/core/composite_entity_graphics.h
 * @brief 複数の子要素を持ち、各子要素をそれぞれのシェーダで描画する
 *        エンティティの描画情報を管理するクラス
 * @author Yayoi Habami
 * @date 2025-08-10
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_COMPOSITE_ENTITY_GRAPHICS_H_
#define IGESIO_GRAPHICS_CORE_COMPOSITE_ENTITY_GRAPHICS_H_

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/entity_base.h"
#include "igesio/graphics/core/i_entity_graphics.h"



namespace igesio::graphics {

/// @brief 複数の子要素を持ち、各子要素をそれぞれのシェーダで描画する
///        エンティティの描画情報を管理するクラス
/// @note Composite Curveなど、複数の子要素を持ち、それぞれの子要素で
///       異なるシェーダーを使用する場合に使用される.
template<typename T, typename = std::enable_if_t<
      std::is_base_of_v<entities::IEntityIdentifier, T>>>
class CompositeEntityGraphics : public IEntityGraphics {
 protected:
    /// @brief 描画オブジェクトのID
    ObjectID graphics_id_;

    /// @brief エンティティのポインタ
    std::shared_ptr<const T> entity_;

    /// @brief 子要素のエンティティの描画用オブジェクト
    /// @note キーはShaderType、値はIEntityGraphicsの配列
    std::unordered_map<ShaderType, std::vector<std::unique_ptr<IEntityGraphics>>>
            child_graphics_;

    /// @brief シェーダーのタイプ
    /// @note このクラスは複数の子要素を持つため、
    ///       ShaderTypeは常にkCompositeを使用する
    static constexpr ShaderType kShaderType = ShaderType::kComposite;



 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @param use_entity_transform シェーダーのmodel変数に
    ///        entity_が参照する変換行列を掛け合わせるか
    /// @throw std::invalid_argument entityがnullptrの場合
    CompositeEntityGraphics(const std::shared_ptr<const T> entity,
                            const std::shared_ptr<IOpenGL> gl,
                            bool use_entity_transform)
            : IEntityGraphics(gl, use_entity_transform),
              entity_(std::move(entity)),
              graphics_id_(IDGenerator::Generate(
                    ObjectType::kEntityGraphics,
                    (entity != nullptr) ? static_cast<uint16_t>(entity->GetType()) : 0)) {
        if (!entity_) {
            throw std::invalid_argument("Entity pointer cannot be null");
        }

        // 色をエンティティから取得 (IGESの色は0-100の範囲)
        ResetColor();
    }

    // コピーコンストラクタとコピー代入演算子を禁止
    CompositeEntityGraphics(const CompositeEntityGraphics&) = delete;
    CompositeEntityGraphics& operator=(const CompositeEntityGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のCompositeEntityGraphics
    /// @note ムーブ元のリソース所有権を放棄
    CompositeEntityGraphics(CompositeEntityGraphics&& other) noexcept
        : IEntityGraphics(std::move(other)),
          entity_(std::move(other.entity_)),
          graphics_id_(std::move(other.graphics_id_)) {
        // ムーブ元のポインタをnullptrにし、リソースの二重解放を防ぐ
        other.entity_ = nullptr;
    }

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のCompositeEntityGraphics
    /// @return *this
    /// @note ムーブ元のリソース所有権を放棄
    CompositeEntityGraphics& operator=(CompositeEntityGraphics&& other) noexcept {
        if (this != &other) {
            // 基底クラスのムーブ代入演算子を呼び出す
            IEntityGraphics::operator=(std::move(other));

            // メンバをムーブ
            entity_ = std::move(other.entity_);
            graphics_id_ = std::move(other.graphics_id_);

            // ムーブ元のポインタをnullptrにし、リソースの二重解放を防ぐ
            other.entity_ = nullptr;
        }
        return *this;
    }

    /// @brief エンティティのIDを取得する
    /// @return エンティティのID
    /// @note エンティティが未設定の場合はUnsetIDを返す
    const ObjectID& GetEntityID() const override {
        if (entity_) {
            return entity_->GetID();
        }
        return IDGenerator::UnsetID();
    }

    /// @brief 描画オブジェクトのIDを取得する
    /// @return 描画オブジェクトのID
    const ObjectID& GetGraphicsID() const override {
        return graphics_id_;
    }

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param shader_type 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note shader_typeの子要素のみを描画する. 子要素の描画は
    ///       子要素に移譲する.
    void Draw(GLuint shader, const ShaderType shader_type,
              const std::pair<float, float>& viewport) const override {
        // kCompositeは特殊なシェーダータイプであり、描画処理は行わない
        if (shader_type == ShaderType::kComposite) return;

        // 指定されたシェーダータイプの子要素が存在する場合のみ描画
        auto it = child_graphics_.find(shader_type);
        if (it != child_graphics_.end()) {
            for (const auto& child : it->second) {
                if (child && child->IsDrawable()) {
                    child->Draw(shader, shader_type, viewport);
                }
            }
        }

        // 子要素にkCompositeが含まれる場合は再帰的に描画
        auto composite_it = child_graphics_.find(ShaderType::kComposite);
        if (composite_it != child_graphics_.end()) {
            for (const auto& child : composite_it->second) {
                if (child && child->IsDrawable()) {
                    child->Draw(shader, shader_type, viewport);
                }
            }
        }
    }

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note 何もしない. 子要素の描画は`Draw(shader, shader_type, viewport)`で行う.
    void Draw(GLuint shader, const std::pair<float, float>& viewport) const override {}



    /**
     * 描画用パラメータの取得・設定
     */

    /// @brief グローバル座標系への変換行列を設定する
    /// @param matrix グローバル座標系への変換行列
    /// @note エンティティが定義される空間から、グローバル座標系への変換を行う
    ///       同次変換行列. `CircularArcGraphics`等の、パラメータをGPUに渡す
    ///       グラフィックスクラスでは、エンティティの定義空間からグローバル座標系への
    ///       変換 (すなわちエンティティがDEレコード7で指定する変換行列と、
    ///       モデル空間までの各親の変換行列をすべて掛け合わせたもの) を指定する.
    ///       `ICurveGraphics`等の、離散化をCPU上で行うグラフィックスクラスでは、
    ///       エンティティがDEレコード7で指定する変換行列を含まない.
    /// @note Section 3.2.3の物理的な依存関係を持たないエンティティについては、
    ///       子要素に親要素の変換行列を伝播させないようにすること (`trans_matrix`を、
    ///       `matrix`としてオーバライドする).
    void SetWorldTransform(const igesio::Matrix4f& matrix) override {
        world_transform_ = matrix;

        // 子要素の変換行列も更新
        auto trans_matrix = matrix * entity_->GetTransformationMatrix()
                                            .GetTransformation().template cast<float>();
        for (auto& [shader_type, graphics_list] : child_graphics_) {
            for (auto& graphics : graphics_list) {
                if (graphics) graphics->SetWorldTransform(trans_matrix);
            }
        }
    }

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

    /// @brief メインの色を設定する
    /// @param color メインの色 (RGBA; [0, 1]の範囲)
    /// @note この関数で設定可能な色は描画上の色であり、エンティティが保持する
    ///       (IGES側で定義する) 色とは異なる.したがって、この関数で色を設定したとしても、
    ///       エンティティのインスタンスの色情報は変更されない.
    void SetColor(const std::array<GLfloat, 4>& color) override {
        IEntityGraphics::SetColor(color);

        // 子要素にも色を伝播
        for (auto& [shader_type, graphics_list] : child_graphics_) {
            for (auto& graphics : graphics_list) {
                if (graphics) {
                    graphics->SetColor(color);
                }
            }
        }
    }

    /// @brief 色をデフォルトのエンティティの色に戻す
    void ResetColor() override {
        is_color_overridden_ = false;

        // エンティティの色を取得して設定
        auto base = std::dynamic_pointer_cast<const entities::EntityBase>(entity_);
        if (base) {
            auto [r, g, b] = base->GetColor().GetRGB();
            color_[0] = static_cast<GLfloat>(r) / 100.0f;
            color_[1] = static_cast<GLfloat>(g) / 100.0f;
            color_[2] = static_cast<GLfloat>(b) / 100.0f;
            color_[3] = 1.0f;  // 不透明度は1.0f (完全に不透明)
        }

        // 子要素の色もデフォルトに戻す
        for (auto& [shader_type, graphics_list] : child_graphics_) {
            for (auto& graphics : graphics_list) {
                if (graphics) {
                    graphics->ResetColor();
                }
            }
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
            if (line_weight_number < 0) return kDefaultLineWidth;

            if (global_param_) {
                return global_param_->GetLineWeight(line_weight_number);
            }
        }
        return kDefaultLineWidth;
    }



    /**
     * リソースの確認・変更
     */

    /// @brief 子要素の追加
    /// @param graphics 描画オブジェクト
    /// @note graphicsがnullptrの場合は何もしない
    void AddChildGraphics(std::unique_ptr<IEntityGraphics>&& graphics) {
        if (!graphics) return;

        // 子要素のShaderTypeを取得
        auto shader_type = graphics->GetShaderType();
        child_graphics_[shader_type].emplace_back(std::move(graphics));
    }

    /// @brief 描画用のシェーダーのタイプを取得する
    /// @return 描画用のシェーダーのタイプ
    ShaderType GetShaderType() const override {
        return kShaderType;
    }

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return 全ての可能なシェーダータイプのリスト
    /// @note 子要素がある場合、`GetShaderType`のShaderTypeに加えて、
    ///       各子要素のShaderTypeも含まれる.
    std::unordered_set<ShaderType> GetShaderTypes() const override {
        std::unordered_set<ShaderType> shader_types = {GetShaderType()};
        for (const auto& [shader_type, children] : child_graphics_) {
            if (shader_type != ShaderType::kComposite && !children.empty()) {
                shader_types.insert(shader_type);
            } else {
                // 子要素自身がCompositeの場合は、子要素のShaderTypeも取得
                for (const auto& child : children) {
                    shader_types.merge(child->GetShaderTypes());
                }
            }
        }
        return shader_types;
    }

    /// @brief OpenGLリソースを解放する
    /// @note 子要素の描画オブジェクトのリソースも解放する
    void Cleanup() override {
        for (auto& [shader_type, graphics_list] : child_graphics_) {
            for (auto& graphics : graphics_list) {
                if (graphics) {
                    graphics->Cleanup();
                }
            }
        }
        child_graphics_.clear();
    }

    /// @brief 描画可能な状態かどうかを確認する
    /// @note すべての子要素が描画可能である場合にtrueを返す
    bool IsDrawable() const override {
        for (const auto& [shader_type, graphics_list] : child_graphics_) {
            for (const auto& graphics : graphics_list) {
                if (!graphics || !graphics->IsDrawable()) {
                    return false;  // 1つでも描画不可ならfalse
                }
            }
        }
        return true;  // すべての子要素が描画可能
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_COMPOSITE_ENTITY_GRAPHICS_H_
