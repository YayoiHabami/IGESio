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
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/interfaces/i_geometry.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/curves/algorithms/curve_line_intersection.h"
#include "igesio/entities/surfaces/algorithms/surface_line_intersection.h"
#include "igesio/graphics/core/i_entity_graphics.h"
#include "igesio/graphics/core/pick_registry.h"



namespace igesio::graphics {

/// @brief 同期キーへ (ObjectID, GeometryRevision) を順序通りに結合する
/// @param seed 現在のキー値
/// @param entity 結合するエンティティ
/// @return 結合後のキー値
/// @note boost::hash_combine方式. リビジョンの総和では参照先交換時に増減が
///       相殺しうるため、同一性(ID)と値(rev)をハッシュで畳み込む
inline uint64_t CombineGeometryKey(uint64_t seed,
                                   const entities::IEntityIdentifier& entity) {
    constexpr uint64_t kGolden = 0x9e3779b97f4a7c15ULL;
    seed ^= std::hash<ObjectID>{}(entity.GetID())
            + kGolden + (seed << 6) + (seed >> 2);
    seed ^= entity.GeometryRevision() + kGolden + (seed << 6) + (seed >> 2);
    return seed;
}

/// @brief エンティティ自身・DE変換チェーン・物理従属子を再帰的にキーへ結合する
/// @param seed 現在のキー値
/// @param entity 起点エンティティ
/// @return 結合後のキー値
/// @note テッセレーションが読む参照先 (DE変換行列・複合曲線の子・トリム面の境界・
///       ルールド面の子曲線等) の形状変更を、親グラフィックスの再同期要否として
///       検知するための走査. DE変換チェーンの循環はSetReference側で防止済み.
///       物理従属はDAGであり循環しない前提
inline uint64_t CombineGeometryKeyRecursive(
        uint64_t seed, const entities::IEntityIdentifier& entity) {
    seed = CombineGeometryKey(seed, entity);
    const auto* base = dynamic_cast<const entities::EntityBase*>(&entity);
    if (base == nullptr) return seed;

    // DE変換チェーン (共有TransformationMatrixの編集を参照元の再同期として検知)
    for (auto t = base->GetTransformationMatrix().GetPointer();
         t != nullptr; t = t->GetRefTransformation()) {
        seed = CombineGeometryKey(seed, *t);
    }
    // 物理従属子 (未解決参照はスキップ. 解決時は集合の変化としてキーに現れる)
    for (const auto& cid : base->GetChildIDs()) {
        if (const auto child = base->GetChildEntity(cid)) {
            seed = CombineGeometryKeyRecursive(seed, *child);
        }
    }
    return seed;
}

/// @brief エンティティの描画情報を管理するクラス
/// @tparam T エンティティの型
/// @tparam has_surfaces Tがサーフェスを持つか (デフォルト: false)
/// @note このクラスが対応するShaderIdはコンストラクタ引数で受け取り、
///       実行時メンバ`shader_id_`に保持する.
/// @note 葉ノードは自前のVAOを`DrawImpl`で描画する. 複合ノード(`child_graphics_`に
///       子を持つ)は描画を子へ委譲し、`DrawImpl`は空実装でよい (CompositeCurve等).
/// @note 以下のメンバ関数のオーバーライドが必要
///       - `EntityGraphics`由来:
///         - `Cleanup`(public): VAO以外のOpenGLリソースを持つ場合
///         - `DrawImpl`(protected): 葉ノードでは常にオーバーライドが必要
template<typename T, bool has_surfaces = false,
         typename = std::enable_if_t<
        std::is_base_of_v<entities::IEntityIdentifier, T>>>
class EntityGraphics : public IEntityGraphics {
 protected:
    /// @brief 描画オブジェクトのID
    ObjectID graphics_id_;

    /// @brief エンティティのポインタ
    std::shared_ptr<const T> entity_;

    /// @brief エンティティの描画用の頂点配列オブジェクト (VAO) のID
    gl::Uint vao_ = 0;
    /// @brief エンティティの描画モード (gl::kLineStrip, gl::kLineLoopなど)
    gl::Enum draw_mode_ = gl::kLineStrip;
    /// @brief テクスチャのID (サーフェスを持つ場合に使用)
    gl::Uint texture_id_ = 0;
    /// @brief このクラスが対応するシェーダーのタイプ
    ShaderId shader_id_;
    /// @brief 子要素の描画オブジェクト (複合ノードの場合に使用. 葉では空)
    /// @note キーはShaderId. CompositeCurve(102)等、複数の子を異なるシェーダーで
    ///       描画する場合に使用する. 子を持つノードは自前VAOを持たない.
    std::unordered_map<ShaderId, std::vector<std::unique_ptr<IEntityGraphics>>>
            child_graphics_;

    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @param shader_id このクラスが対応するシェーダーのタイプ
    /// @param use_entity_transform シェーダーのmodel変数に
    ///        entity_が参照する変換行列を掛け合わせるか
    /// @throw std::invalid_argument entityがnullptrの場合
    EntityGraphics(const std::shared_ptr<const T>& entity,
                   const std::shared_ptr<IOpenGL>& gl,
                   ShaderId shader_id,
                   bool use_entity_transform)
            : IEntityGraphics(gl, use_entity_transform),
              entity_(entity),
              graphics_id_(IDGenerator::Generate(
                    ObjectType::kEntityGraphics,
                    (entity != nullptr) ? static_cast<uint16_t>(entity->GetType()) : 0)),
              shader_id_(shader_id) {
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
          draw_mode_(other.draw_mode_),
          texture_id_(other.texture_id_),
          graphics_id_(std::move(other.graphics_id_)),
          shader_id_(other.shader_id_),
          child_graphics_(std::move(other.child_graphics_)) {
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
            texture_id_ = other.texture_id_;
            graphics_id_ = std::move(other.graphics_id_);
            shader_id_ = other.shader_id_;
            child_graphics_ = std::move(other.child_graphics_);

            // ムーブ元のポインタをnullptrにし、リソースの二重解放を防ぐ
            other.entity_ = nullptr;
            other.vao_ = 0;
        }
        return *this;
    }

    /// @brief エンティティのIDを取得する
    /// @return エンティティのID
    /// @note エンティティが未設定の場合はIDGenerator::UnsetID()を返す
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

    /// @brief 現在の同期キーを計算する
    /// @return 自エンティティ+DE変換チェーン+物理従属子(再帰)の
    ///         (ObjectID, GeometryRevision) ハッシュ結合
    /// @note 複合系 (CompositeCurve/CurveOnSurface/TrimmedSurface) の参照先も
    ///       GetChildIDs経由で結合されるため、通常はオーバーライド不要
    uint64_t CurrentGeometryKey() const override {
        if (!entity_) return 0;
        return CombineGeometryKeyRecursive(0, *entity_);
    }

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param shader_id 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note shader_idに合致する要素がない場合は何もしない
    void Draw(gl::Uint shader, const ShaderId shader_id,
              const std::pair<float, float>& viewport,
              const DrawContext& ctx) const override {
        // 葉ノード: 固有シェーダーが一致する場合に自己描画する
        if (HasSpecificShaderCode(shader_id_) && shader_id == shader_id_) {
            Draw(shader, viewport, ctx);
        }
        // 複合ノード: 子要素を持つ場合は指定型の子へ委譲する
        DrawChildGraphics(shader, shader_id, viewport, ctx);
    }

    /// @brief エンティティの描画を行う (Drawフェーズ)
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    /// @note 「PULLした表示状態の適用 (ApplyRenderState)」と「ジオメトリ発行
    ///       (DrawImpl)」に責務を分離する. 本フェーズは論理状態を保持しない.
    void Draw(gl::Uint shader, const std::pair<float, float>& viewport,
              const DrawContext& ctx) const override {
        if (!entity_) return;
        if (!IsDrawable()) return;

        ApplyRenderState(shader, ctx);  // PULLした表示状態 (変換/色/材質) を適用
        DrawImpl(shader, viewport);      // ジオメトリ発行 (GL描画コマンド)
    }

    /// @brief テクスチャの設定を行う
    void SyncTexture() override {
        if (!entity_ || !material_property_.IsTextureUsable()) return;
        if (!has_surfaces) return;

        gl_->GenTextures(1, &texture_id_);
        gl_->BindTexture(gl::kTexture2D, texture_id_);

        // テクスチャのパラメータを設定
        gl_->TexParameteri(gl::kTexture2D, gl::kTextureWrapS, gl::kClampToEdge);
        gl_->TexParameteri(gl::kTexture2D, gl::kTextureWrapT, gl::kClampToEdge);
        gl_->TexParameteri(gl::kTexture2D, gl::kTextureMinFilter, gl::kLinear);
        gl_->TexParameteri(gl::kTexture2D, gl::kTextureMagFilter, gl::kLinear);

        gl::Enum format = (material_property_.texture.HasAlpha()) ? gl::kRgba : gl::kRgb;
        auto [width, height] = material_property_.texture.GetSize();
        gl_->TexImage2D(gl::kTexture2D, 0, format, width, height, 0,
                        format, gl::kUnsignedByte, material_property_.texture.GetData());
        gl_->GenerateMipmap(gl::kTexture2D);
    }



    /**
     * リソースの確認・変更
     */

    /// @brief 描画用のシェーダーのタイプを取得する
    /// @return 描画用のシェーダーのタイプ
    ShaderId GetShaderId() const override {
        return shader_id_;
    }

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return 自身のShaderIdと、子要素 (複合ノード) のShaderIdの和集合
    std::unordered_set<ShaderId> GetShaderIds() const override {
        std::unordered_set<ShaderId> types = {shader_id_};
        for (const auto& [st, children] : child_graphics_) {
            if (st != ShaderId::kComposite && !children.empty()) {
                types.insert(st);
            } else {
                // 子要素自身がkCompositeの場合は、その子のShaderIdも取得
                for (const auto& child : children) {
                    if (child) types.merge(child->GetShaderIds());
                }
            }
        }
        return types;
    }

    /// @brief 描画用CPUデータを事前構築する (既定: 子要素へカスケード)
    /// @note 複合ノードは子要素 (child_graphics_) のPrewarmCpuを呼び、子に内包された
    ///       遅延生成・テッセレーション等のCPU準備を伝播する。葉ノードは子を持たない
    ///       ためno-op (重いCPU処理を持つ葉は本メソッドを個別にオーバーライドする)。
    void PrewarmCpu() override {
        for (auto& [st, list] : child_graphics_) {
            for (auto& child : list) {
                if (child) child->PrewarmCpu();
            }
        }
    }

    /// @brief 子要素の描画オブジェクトを追加する (複合ノード化)
    /// @param graphics 追加する描画オブジェクト
    /// @note graphicsがnullptrの場合は何もしない. 子のShaderIdでバケットへ格納する.
    void AddChildGraphics(std::unique_ptr<IEntityGraphics>&& graphics) {
        if (!graphics) return;
        const auto st = graphics->GetShaderId();
        child_graphics_[st].emplace_back(std::move(graphics));
    }

    /// @brief OpenGLリソースを解放する
    /// @note GPUへパラメータを渡し、GPU上で離散点を計算する場合は
    ///       オーバーライド不要.
    void Cleanup() override {
        if (vao_ != 0) {
            gl_->DeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (texture_id_ != 0) {
            gl_->DeleteTextures(1, &texture_id_);
            texture_id_ = 0;
        }
        // 子要素 (複合ノード) のリソースも解放する
        for (auto& [st, list] : child_graphics_) {
            for (auto& child : list) {
                if (child) child->Cleanup();
            }
        }
        child_graphics_.clear();
    }

    /// @brief 描画可能な状態かどうかを確認する
    /// @note GPUへパラメータを渡し、GPU上で離散点を計算する場合は
    ///       オーバーライド不要.
    bool IsDrawable() const override {
        // 複合ノード: 全ての子要素が描画可能なら描画可
        if (!child_graphics_.empty()) {
            for (const auto& [st, list] : child_graphics_) {
                for (const auto& child : list) {
                    if (!child || !child->IsDrawable()) return false;
                }
            }
            return true;
        }
        // 葉ノード
        return entity_ && vao_ != 0;
    }



    /**
     * パラメータの取得/設定
     */

    /// @brief グローバル座標系への変換行列を設定する
    /// @param matrix グローバル座標系への変換行列 (親→モデル空間)
    /// @note 子要素 (複合ノード) を持つ場合は、matrix·M_entityを子へ伝播する.
    void SetWorldTransform(const igesio::Matrix4d& matrix) override {
        world_transform_ = matrix;
        if (child_graphics_.empty()) return;

        // 子要素には matrix·M_entity を伝播する (M_entityは各子が自前で扱わない前提).
        // 単精度往復による誤差累積を避けるため、伝播はdoubleで行う
        igesio::Matrix4d child_transform = matrix;
        if (auto ptr =
                std::dynamic_pointer_cast<const entities::EntityBase>(entity_)) {
            const igesio::Matrix4d m_entity = ptr->GetTransformationMatrix()
                    .GetTransformation();
            child_transform = matrix * m_entity;
        }
        for (auto& [st, list] : child_graphics_) {
            for (auto& child : list) {
                if (child) child->SetWorldTransform(child_transform);
            }
        }
    }

    /// @brief グローバル座標系への変換行列をGPU用の単精度で取得する
    /// @return グローバル座標系への変換行列 (float).
    ///         use_entity_transform_がtrueの場合は、
    ///         `SetWorldTransform`で設定された変換行列に
    ///         entity_が参照する変換行列を掛け合わせたものを返す.
    /// @note CPU側の幾何計算 (BB・ピッキング等) では精度を失わないよう
    ///       GetWorldTransformD() を使用すること.
    igesio::Matrix4f GetWorldTransform() const override {
        // 単精度はGL境界でのみ生成する. 一時へのダングリング参照を避けるため、
        // doubleの正準値を名前付きローカルに受けてから単精度へ変換する
        const igesio::Matrix4d w = GetWorldTransformD();
        return w.cast<float>();
    }

    /// @brief グローバル座標系への変換行列を倍精度で取得する (CPU幾何の正準値)
    /// @return グローバル座標系への変換行列 (double).
    ///         use_entity_transform_がtrueの場合は、
    ///         `SetWorldTransform`で設定された変換行列に
    ///         entity_が参照する変換行列を掛け合わせたものを返す.
    igesio::Matrix4d GetWorldTransformD() const override {
        if (use_entity_transform_ && entity_) {
            auto ptr = std::dynamic_pointer_cast<const entities::EntityBase>(entity_);
            if (ptr) {
                const igesio::Matrix4d entity_transform =
                        ptr->GetTransformationMatrix().GetTransformation();
                return world_transform_ * entity_transform;
            }
        }
        return world_transform_;
    }

    /// @brief 現在のメインの色を取得する
    /// @return メインの色 (RGBA; [0, 1]の範囲)
    /// @note SetColorで色をオーバーライドした場合はその色を返す.
    ///       そうでない場合は、エンティティが保持する色を返す.
    std::array<float, 4> GetColor() const override {
        if (!is_color_overridden_) {
            auto base = std::dynamic_pointer_cast<const entities::EntityBase>(entity_);
            if (base) {
                auto [r, g, b] = base->GetColor().GetRGB();
                return {static_cast<float>(r) / 100.0f,
                        static_cast<float>(g) / 100.0f,
                        static_cast<float>(b) / 100.0f,
                        material_property_.opacity};
            }
        }
        return {color_[0], color_[1], color_[2], color_[3]};
    }

    /// @brief メインの色を設定する
    /// @param color メインの色 (RGBA; [0, 1]の範囲)
    /// @note 子要素 (複合ノード) を持つ場合は子にも伝播する.
    void SetColor(const std::array<float, 4>& color) override {
        IEntityGraphics::SetColor(color);
        for (auto& [st, list] : child_graphics_) {
            for (auto& child : list) {
                if (child) child->SetColor(color);
            }
        }
    }

    /// @brief 色をデフォルトのエンティティの色に戻す
    void ResetColor() override {
        is_color_overridden_ = false;
        auto base = std::dynamic_pointer_cast<const entities::EntityBase>(entity_);
        if (base) {
            auto [r, g, b] = base->GetColor().GetRGB();
            color_[0] = static_cast<float>(r) / 100.0f;
            color_[1] = static_cast<float>(g) / 100.0f;
            color_[2] = static_cast<float>(b) / 100.0f;
            color_[3] = 1.0f;  // 不透明度は1.0f (完全に不透明)
        }
        // 子要素 (複合ノード) の色もデフォルトに戻す
        for (auto& [st, list] : child_graphics_) {
            for (auto& child : list) {
                if (child) child->ResetColor();
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
            if (line_weight_number <= 0) return kDefaultLineWidth;

            if (global_param_) {
                return global_param_->GetLineWeight(line_weight_number);
            }
        }
        return kDefaultLineWidth;
    }



    /**
     * ピッキング（レイ交差判定）
     */

    /// @brief レイとの交差判定が可能か
    /// @return PickRegistryにレイ交差関数の登録がある場合、または
    ///         entity_がISurfaceまたはICurveを参照する場合はtrue
    bool CanIntersect() const override {
        if (!entity_) return false;
        // ①PickRegistry (動的型の完全一致) — レイ交差関数の登録があれば可能
        if (PickRegistry::Find(std::type_index(typeid(*entity_))).intersect) {
            return true;
        }
        // ②能力ディスパッチ (ICurve/ISurface)
        const auto* p = entity_.get();
        return dynamic_cast<const entities::ISurface*>(p) != nullptr
            || dynamic_cast<const entities::ICurve*>(p) != nullptr;
    }

    /// @brief レイとエンティティの交差点を求める
    /// @param ray ワールド空間のレイ (kRayとして扱う)
    /// @param params 探索制御パラメータ
    /// @return 交差点のリスト (distance昇順). 交差判定不可の場合は空リスト
    /// @note ルックアップ順は①PickRegistry (動的型の完全一致) →
    ///       ②能力ディスパッチ (ISurface/ICurve). 本関数をオーバーライドする
    ///       描画クラスにはレジストリは適用されない
    std::vector<RayHit> Intersect(
            const Ray& ray,
            const RayIntersectionParams& params) const override {
        if (!entity_) return {};

        // ①PickRegistry (動的型の完全一致). 登録関数はworld_transform_
        //    (親空間→ワールド) を受け取り、ワールド座標のヒットを返す規約
        if (const auto picks =
                    PickRegistry::Find(std::type_index(typeid(*entity_)));
            picks.intersect) {
            return picks.intersect(*entity_, world_transform_, ray, params);
        }

        // ②能力ディスパッチ (ISurface/ICurve)
        // ray.directionが単位ベクトルのため、線パラメータが原点からの距離に等しい
        const Vector3d p1 = ray.origin + ray.direction;
        constexpr auto kRay = numerics::BoundingBox::DirectionType::kRay;

        std::vector<RayHit> result;

        if (const auto* surf =
                dynamic_cast<const entities::ISurface*>(entity_.get())) {
            entities::SurfaceLineIntersectionParams sp;
            sp.u_samples = params.surface_u_samples;
            sp.v_samples = params.surface_v_samples;
            sp.convergence_tol = params.convergence_tol;
            sp.dedup_tol = params.dedup_tol;
            for (const auto& h : entities::IntersectSurfaceWithLine(
                    *surf, ray.origin, p1, kRay, sp)) {
                result.push_back({h.position, h.t});
            }
            return result;
        }

        if (const auto* curve =
                dynamic_cast<const entities::ICurve*>(entity_.get())) {
            entities::CurveLineIntersectionParams cp;
            cp.curve_samples = params.curve_samples;
            cp.convergence_tol = params.convergence_tol;
            cp.dedup_tol = params.dedup_tol;
            // ピッキング経路ではPickEntitiesがワールド換算した値で上書き済み
            cp.hit_tolerance = params.curve_hit_tolerance;
            for (const auto& h : entities::IntersectCurveWithLine(
                    *curve, ray.origin, p1, kRay, cp)) {
                result.push_back({h.position, h.t_line});
            }
            return result;
        }

        return result;
    }

    /// @brief 範囲選択用にエンティティをサンプリングした点列を返す
    /// @param params サンプリング制御パラメータ
    /// @return ワールド座標のサンプル. 判定不可なら空
    /// @note ルックアップ順は①PickRegistry (動的型の完全一致) → ②子集約 →
    ///       ③能力ディスパッチ (ISurface/ICurve). 本関数をオーバーライドする
    ///       描画クラスにはレジストリは適用されない
    SelectionSamples GetSelectionSamples(
            const SelectionSampleParams& params) const override {
        // ①PickRegistry (動的型の完全一致). 子集約・能力ディスパッチより
        //    優先する (CreateEntityGraphicsのレジストリ最優先と同じ規則)
        if (entity_) {
            if (const auto picks =
                        PickRegistry::Find(std::type_index(typeid(*entity_)));
                picks.selection_samples) {
                return picks.selection_samples(*entity_, world_transform_,
                                               params);
            }
        }

        // ②複合ノード: 子要素のサンプルを集約する (子はSetWorldTransform伝播済みの
        // ためワールド座標で得られる. ここでworld_transform_を再適用しないこと)
        if (!child_graphics_.empty()) {
            SelectionSamples result;
            for (const auto& [st, list] : child_graphics_) {
                for (const auto& child : list) {
                    if (!child) continue;
                    auto cs = child->GetSelectionSamples(params);
                    for (auto& pl : cs.polylines) {
                        result.polylines.push_back(std::move(pl));
                    }
                    for (const auto& pt : cs.points) result.points.push_back(pt);
                }
            }
            result.polylines_closed = false;
            return result;
        }

        if (!entity_) return {};

        // world_transform_ (親->ワールド) を適用する
        // (GetWorldBoundingBoxと同じ規約. TryGetPointAtは親空間を返すため、
        //  これを掛けるとワールド座標になる)
        const igesio::Matrix4d& wt = world_transform_;

        if (const auto* surf =
                dynamic_cast<const entities::ISurface*>(entity_.get())) {
            SelectionSamples result = SampleSurfaceBoundary(*surf, params, wt);
            SampleSurfaceInteriorGrid(*surf, params, wt, result);
            return result;
        }
        if (const auto* curve =
                dynamic_cast<const entities::ICurve*>(entity_.get())) {
            return SampleCurve(*curve, params, wt);
        }
        return {};
    }

    /// @brief ワールド座標系におけるバウンディングボックスを取得する
    std::optional<numerics::BoundingBox> GetWorldBoundingBox() const override {
        auto geom = std::dynamic_pointer_cast<const entities::IGeometry>(entity_);
        if (!geom) return std::nullopt;

        // 点・直線状(0/1次元)に退化したメンバは平面/立体のBBを構成できず深度推定に
        // 使えないため、nulloptを返して呼び出し側のフォールバック深度に委ねる。
        const auto defined = geom->GetDefinedBoundingBox();
        if (defined.Dimension() < 2) {
            return std::nullopt;
        }

        // GetBoundingBox()はエンティティ自身のDE変換を適用済み(親空間)。
        // world_transform_ (親→ワールド) を適用してワールド空間のBBを得る。
        auto bb = geom->GetBoundingBox();
        const igesio::Matrix4d& wt = world_transform_;
        const igesio::Matrix3d r = wt.block<3, 3>(0, 0);
        const igesio::Vector3d t = wt.block<3, 1>(0, 3);
        // 回転成分が退化変換(スケール・せん断・射影等)の場合はBBを構成できないため、
        // throwを越境させず深度推定のフォールバックに委ねる(constなピック経路の保護)。
        if (!numerics::IsRotation(r)) return std::nullopt;
        bb.Transform(r, t);
        return bb;
    }



 protected:
    /// @brief 親空間の点にworld_transform_を適用しワールド座標にする
    /// @param wt world_transform_ (double)
    /// @param p 親空間の点
    /// @return ワールド座標の点
    /// @note アフィン変換のためw成分は1のまま. Vector4d経由で乗算し、
    ///       Eigen非使用ビルドでも動作させる
    static Vector3d ToWorld(const igesio::Matrix4d& wt, const Vector3d& p) {
        const igesio::Vector4d w = wt * igesio::Vector4d(p.x(), p.y(), p.z(), 1.0);
        return Vector3d(w.x(), w.y(), w.z());
    }

    /// @brief 曲線区間[ta,tb]を画面弦長基準で再帰的に細分し、中間点を追加する
    /// @param curve 対象の曲線
    /// @param params サンプリング制御パラメータ (適応分割の射影状態を含む)
    /// @param wt world_transform_ (double)
    /// @param ta, wa 区間始点のパラメータとワールド座標 (out には未追加)
    /// @param tb, wb 区間終点のパラメータとワールド座標 (caller が後で追加)
    /// @param depth 残り再帰深さ
    /// @param out 中間点の追加先 (ta側からtb側の順で追加; 両端は含めない)
    static void RefineCurveSegment(
            const entities::ICurve& curve, const SelectionSampleParams& params,
            const igesio::Matrix4d& wt, double ta, const Vector3d& wa,
            double tb, const Vector3d& wb, int depth,
            std::vector<Vector3d>& out) {
        if (depth <= 0) return;
        const auto sa = WorldToScreen(params.adaptive_view_proj,
                params.adaptive_width, params.adaptive_height, wa);
        const auto sb = WorldToScreen(params.adaptive_view_proj,
                params.adaptive_width, params.adaptive_height, wb);
        if (!sa || !sb) return;  // 射影不能ならこれ以上細分しない
        const double dx = sa->x() - sb->x(), dy = sa->y() - sb->y();
        const double tol = params.adaptive_max_chord_px;
        if (dx * dx + dy * dy <= tol * tol) return;  // 画面上で十分短い

        const double tm = 0.5 * (ta + tb);
        const auto pm = curve.TryGetPointAt(tm);
        if (!pm) return;
        const Vector3d wm = ToWorld(wt, *pm);
        RefineCurveSegment(curve, params, wt, ta, wa, tm, wm, depth - 1, out);
        out.push_back(wm);
        RefineCurveSegment(curve, params, wt, tm, wm, tb, wb, depth - 1, out);
    }

    /// @brief 曲線を折れ線群へサンプリングする
    /// @param curve 対象の曲線
    /// @param params サンプリング制御パラメータ
    /// @param wt world_transform_ (double)
    /// @return ワールド座標のサンプル. nulloptの点で折れ線を分割する。
    ///         params.adaptive_refine時は画面弦長基準で区間を細分する
    static SelectionSamples SampleCurve(
            const entities::ICurve& curve, const SelectionSampleParams& params,
            const igesio::Matrix4d& wt) {
        auto range = curve.GetParameterRange();
        double t0 = range[0], t1 = range[1];
        if (std::isinf(t0)) t0 = -kInfiniteParamClamp;
        if (std::isinf(t1)) t1 = kInfiniteParamClamp;

        const int n = std::max(1, params.curve_samples);
        SelectionSamples result;
        std::vector<Vector3d> current;
        bool has_prev = false;
        double prev_t = 0.0;
        Vector3d prev_w;
        for (int i = 0; i <= n; ++i) {
            const double t = t0 + (t1 - t0) * static_cast<double>(i) / n;
            const auto p = curve.TryGetPointAt(t);
            if (!p) {
                if (!current.empty()) {
                    result.polylines.push_back(std::move(current));
                    current.clear();
                }
                has_prev = false;
                continue;
            }
            const Vector3d wp = ToWorld(wt, *p);
            // 直前点との間を画面弦長基準で細分する (中間点のみ追加)
            if (has_prev && params.adaptive_refine) {
                RefineCurveSegment(curve, params, wt, prev_t, prev_w, t, wp,
                                   params.adaptive_max_depth, current);
            }
            current.push_back(wp);
            prev_t = t; prev_w = wp; has_prev = true;
        }
        if (!current.empty()) result.polylines.push_back(std::move(current));
        return result;
    }

    /// @brief 曲面のパラメータ矩形境界を1つの閉ループ折れ線へサンプリングする
    /// @param surf 対象の曲面
    /// @param params サンプリング制御パラメータ
    /// @param wt world_transform_ (double)
    /// @return ワールド座標のサンプル. トリム面は未トリム外形で代用する (TODO)
    static SelectionSamples SampleSurfaceBoundary(
            const entities::ISurface& surf, const SelectionSampleParams& params,
            const igesio::Matrix4d& wt) {
        auto range = surf.GetParameterRange();  // {u0, u1, v0, v1}
        double u0 = range[0], u1 = range[1], v0 = range[2], v1 = range[3];
        if (std::isinf(u0)) u0 = -kInfiniteParamClamp;
        if (std::isinf(u1)) u1 = kInfiniteParamClamp;
        if (std::isinf(v0)) v0 = -kInfiniteParamClamp;
        if (std::isinf(v1)) v1 = kInfiniteParamClamp;

        const int nu = std::max(1, params.surface_u_samples);
        const int nv = std::max(1, params.surface_v_samples);

        // パラメータ矩形の外周を辿る (u,v) 列. 角点重複を避けるため
        // 各辺は始点を含み終点を含まない (全体で閉ループの頂点列となる)
        std::vector<std::pair<double, double>> perimeter;
        for (int i = 0; i < nu; ++i) {  // 辺1: v=v0, u: u0->u1
            const double s = static_cast<double>(i) / nu;
            perimeter.emplace_back(u0 + (u1 - u0) * s, v0);
        }
        for (int i = 0; i < nv; ++i) {  // 辺2: u=u1, v: v0->v1
            const double s = static_cast<double>(i) / nv;
            perimeter.emplace_back(u1, v0 + (v1 - v0) * s);
        }
        for (int i = 0; i < nu; ++i) {  // 辺3: v=v1, u: u1->u0
            const double s = static_cast<double>(i) / nu;
            perimeter.emplace_back(u1 + (u0 - u1) * s, v1);
        }
        for (int i = 0; i < nv; ++i) {  // 辺4: u=u0, v: v1->v0
            const double s = static_cast<double>(i) / nv;
            perimeter.emplace_back(u0, v1 + (v0 - v1) * s);
        }

        SelectionSamples result;
        std::vector<Vector3d> current;
        bool split = false;
        for (const auto& [u, v] : perimeter) {
            const auto p = surf.TryGetPointAt(u, v);
            if (p) {
                current.push_back(ToWorld(wt, *p));
            } else {
                if (!current.empty()) {
                    result.polylines.push_back(std::move(current));
                    current.clear();
                }
                split = true;
            }
        }
        if (!current.empty()) result.polylines.push_back(std::move(current));
        // 分割無く1ループに収まった場合のみ閉ループとして扱う
        result.polylines_closed = !split && result.polylines.size() == 1;
        return result;
    }

    /// @brief 曲面の内部格子点 (トリム領域内) をサンプルへ追加する
    /// @param surf 対象の曲面
    /// @param params サンプリング制御パラメータ
    /// @param wt world_transform_ (double)
    /// @param result 追加先のサンプル (pointsへ格納)
    /// @note 矩形境界だけでは捉えられない、面内に収まる矩形や非凸シルエットの
    ///       取りこぼしを補う。IsInDomainを満たす点のみ追加する
    static void SampleSurfaceInteriorGrid(
            const entities::ISurface& surf, const SelectionSampleParams& params,
            const igesio::Matrix4d& wt, SelectionSamples& result) {
        auto range = surf.GetParameterRange();
        double u0 = range[0], u1 = range[1], v0 = range[2], v1 = range[3];
        if (std::isinf(u0)) u0 = -kInfiniteParamClamp;
        if (std::isinf(u1)) u1 = kInfiniteParamClamp;
        if (std::isinf(v0)) v0 = -kInfiniteParamClamp;
        if (std::isinf(v1)) v1 = kInfiniteParamClamp;

        const int nu = std::max(1, params.surface_u_samples);
        const int nv = std::max(1, params.surface_v_samples);
        for (int i = 1; i < nu; ++i) {
            for (int j = 1; j < nv; ++j) {
                const double u = u0 + (u1 - u0) * static_cast<double>(i) / nu;
                const double v = v0 + (v1 - v0) * static_cast<double>(j) / nv;
                if (!surf.IsInDomain(u, v)) continue;
                const auto p = surf.TryGetPointAt(u, v);
                if (p) result.points.push_back(ToWorld(wt, *p));
            }
        }
    }

 protected:
    /// @brief PULLした表示状態をuniformへ適用する (Drawフェーズの状態適用部)
    /// @param shader プログラムシェーダーのID
    /// @param ctx 表示コンテキスト
    /// @note model(変換)・mainColor(選択ハイライト or エンティティ色)・線幅を設定し、
    ///       has_surfaces時は材質/テクスチャを設定する. 論理状態は保持しない.
    void ApplyRenderState(gl::Uint shader, const DrawContext& ctx) const {
        // Core Profileでは glLineWidth が無効なため、線幅[px]を太線化GSへ渡す
        // (viewportSizeはレンダラが設定; 当該uniform非保持シェーダーでは無害に無視).
        gl_->Uniform1f(gl_->GetUniformLocation(shader, "lineWidth"),
                       static_cast<float>(GetLineWidth()));

        // 全シェーダーで共通のuniform変数を設定
        igesio::Matrix4f model = GetWorldTransform();
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(shader, "model"),
                              1, gl::kFalse, model.data());
        // 選択中はハイライト色をPULLし、そうでなければエンティティの色を使う
        // (選択色をオブジェクトへ焼き込まない)
        const std::array<float, 4> color =
                ctx.IsHighlighted(GetEntityID()) ? ctx.highlight_color : GetColor();
        gl_->Uniform4fv(gl_->GetUniformLocation(shader, "mainColor"),
                        1, color.data());

        // エンティティが面を持っている場合は関連するパラメータを設定
        if constexpr (has_surfaces) {
            gl_->Uniform1f(gl_->GetUniformLocation(shader, "roughness"),
                           material_property_.roughness);
            gl_->Uniform1f(gl_->GetUniformLocation(shader, "metallic"),
                           material_property_.metallic);
            gl_->Uniform1f(gl_->GetUniformLocation(shader, "ao"),
                           material_property_.ao);

            // テクスチャの設定
            gl_->Uniform1i(gl_->GetUniformLocation(shader, "useTexture"),
                           material_property_.IsTextureUsable() ? 1 : 0);
            if (material_property_.IsTextureUsable() && texture_id_ != 0) {
                gl_->ActiveTexture(gl::kTexture0);
                gl_->BindTexture(gl::kTexture2D, texture_id_);
                gl_->Uniform1i(gl_->GetUniformLocation(shader, "textureSampler"), 0);
            }
        }
    }

    /// @brief 子要素 (複合ノード) を指定シェーダータイプで描画する
    /// @param shader プログラムシェーダーのID
    /// @param shader_id 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @param ctx 表示コンテキスト
    /// @note 指定型の直接の子に加え、kCompositeの子へも再帰委譲する. 子が無ければ何もしない.
    void DrawChildGraphics(gl::Uint shader, const ShaderId shader_id,
                           const std::pair<float, float>& viewport,
                           const DrawContext& ctx) const {
        if (child_graphics_.empty()) return;
        // 親(この複合ノード)が選択中なら、子のID判定に依らずハイライトさせる
        DrawContext child_ctx = ctx;
        if (ctx.IsHighlighted(GetEntityID())) child_ctx.force_highlight = true;

        auto it = child_graphics_.find(shader_id);
        if (it != child_graphics_.end()) {
            for (const auto& child : it->second) {
                if (child && child->IsDrawable()) {
                    child->Draw(shader, shader_id, viewport, child_ctx);
                }
            }
        }
        auto cit = child_graphics_.find(ShaderId::kComposite);
        if (cit != child_graphics_.end()) {
            for (const auto& child : cit->second) {
                if (child && child->IsDrawable()) {
                    child->Draw(shader, shader_id, viewport, child_ctx);
                }
            }
        }
    }

    /// @brief エンティティの描画を実装する関数
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note OpenGLの描画コマンドを実行する
    virtual void DrawImpl(gl::Uint, const std::pair<float, float>&) const = 0;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_ENTITY_GRAPHICS_H_
