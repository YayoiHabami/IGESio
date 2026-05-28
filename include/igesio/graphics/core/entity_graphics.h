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
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/interfaces/i_geometry.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/curves/algorithms/curve_line_intersection.h"
#include "igesio/entities/surfaces/algorithms/surface_line_intersection.h"
#include "igesio/graphics/core/i_entity_graphics.h"



namespace igesio::graphics {

/// @brief エンティティの描画情報を管理するクラス
/// @tparam T エンティティの型
/// @tparam ShaderType_ このクラスが対応するシェーダーのタイプ
/// @tparam has_surfaces Tがサーフェスを持つか (デフォルト: false)
/// @note 以下のメンバ関数のオーバーライドが必要
///       - `EntityGraphics`由来:
///         - `Cleanup`(public): VAO以外のOpenGLリソースを持つ場合
///         - `DrawImpl`(protected): 常にオーバーライドが必要
template<typename T, ShaderType ShaderType_,
         bool has_surfaces = false, typename = std::enable_if_t<
        std::is_base_of_v<entities::IEntityIdentifier, T>>>
class EntityGraphics : public IEntityGraphics {
 protected:
    /// @brief 描画オブジェクトのID
    ObjectID graphics_id_;

    /// @brief エンティティのポインタ
    std::shared_ptr<const T> entity_;

    /// @brief エンティティの描画用の頂点配列オブジェクト (VAO) のID
    GLuint vao_ = 0;
    /// @brief エンティティの描画モード (GL_LINE_STRIP, GL_LINE_LOOPなど)
    GLenum draw_mode_ = GL_LINE_STRIP;
    /// @brief テクスチャのID (サーフェスを持つ場合に使用)
    GLuint texture_id_ = 0;

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
              entity_(entity),
              graphics_id_(IDGenerator::Generate(
                    ObjectType::kEntityGraphics,
                    (entity != nullptr) ? static_cast<uint16_t>(entity->GetType()) : 0)) {
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
          graphics_id_(std::move(other.graphics_id_)) {
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
    void Draw(GLuint shader, const std::pair<float, float>& viewport) const override {
        if (!entity_) return;
        if (!IsDrawable()) return;

        gl_->LineWidth(GetLineWidth());

        // 全シェーダーで共通のuniform変数を設定
        igesio::Matrix4f model = GetWorldTransform();
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(shader, "model"),
                              1, GL_FALSE, model.data());
        gl_->Uniform4fv(gl_->GetUniformLocation(shader, "mainColor"),
                        1, GetColor().data());

        // エンティティが面を持っている場合は関連するパラメータを設定
        if constexpr (has_surfaces) {
            gl_->Uniform1f(gl_->GetUniformLocation(shader, "ambientStrength"),
                           material_property_.ambient_strength);
            gl_->Uniform1f(gl_->GetUniformLocation(shader, "specularStrength"),
                           material_property_.specular_strength);
            gl_->Uniform1i(gl_->GetUniformLocation(shader, "shininess"),
                           material_property_.shininess);

            // テクスチャの設定
            gl_->Uniform1i(gl_->GetUniformLocation(shader, "useTexture"),
                           material_property_.IsTextureUsable() ? 1 : 0);
            if (material_property_.IsTextureUsable() && texture_id_ != 0) {
                gl_->ActiveTexture(GL_TEXTURE0);
                gl_->BindTexture(GL_TEXTURE_2D, texture_id_);
                gl_->Uniform1i(gl_->GetUniformLocation(shader, "textureSampler"), 0);
            }
        }

        DrawImpl(shader, viewport);
    }

    /// @brief テクスチャの設定を行う
    void SyncTexture() override {
        if (!entity_ || !material_property_.IsTextureUsable()) return;
        if (!has_surfaces) return;

        gl_->GenTextures(1, &texture_id_);
        gl_->BindTexture(GL_TEXTURE_2D, texture_id_);

        // テクスチャのパラメータを設定
        gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLenum format = (material_property_.texture.HasAlpha()) ? GL_RGBA : GL_RGB;
        auto [width, height] = material_property_.texture.GetSize();
        gl_->TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                        format, GL_UNSIGNED_BYTE, material_property_.texture.GetData());
        gl_->GenerateMipmap(GL_TEXTURE_2D);
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
        if (texture_id_ != 0) {
            gl_->DeleteTextures(1, &texture_id_);
            texture_id_ = 0;
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
                // NOTE: cast<float>()は遅延評価式を返すため、autoで受けると
                //       GetTransformation()が返す一時Matrix4dへのダングリング参照
                //       となる (Release最適化時にUBで描画が破綻する)。具体型で
                //       受けて一時が生存中に即時評価・コピーさせる。
                const igesio::Matrix4f entity_transform = ptr->GetTransformationMatrix()
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
                        material_property_.opacity};
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



    /**
     * ピッキング（レイ交差判定）
     */

    /// @brief レイとの交差判定が可能か
    /// @return entity_がISurfaceまたはICurveを参照する場合はtrue
    bool CanIntersect() const override {
        if (!entity_) return false;
        const auto* p = entity_.get();
        return dynamic_cast<const entities::ISurface*>(p) != nullptr
            || dynamic_cast<const entities::ICurve*>(p) != nullptr;
    }

    /// @brief レイとエンティティの交差点を求める
    /// @param ray ワールド空間のレイ (kRayとして扱う)
    /// @param params 探索制御パラメータ
    /// @return 交差点のリスト (distance昇順). 交差判定不可の場合は空リスト
    std::vector<RayHit> Intersect(
            const Ray& ray,
            const RayIntersectionParams& params) const override {
        if (!entity_) return {};

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
    /// @return ワールド座標のサンプル. ISurface/ICurveでなければ空
    SelectionSamples GetSelectionSamples(
            const SelectionSampleParams& params) const override {
        if (!entity_) return {};

        // world_transform_ (親->ワールド) をdoubleへ昇格して適用する
        // (GetWorldBoundingBoxと同じ規約. TryGetPointAtは親空間を返すため、
        //  これを掛けるとワールド座標になる)
        const igesio::Matrix4d wt = world_transform_.cast<double>();

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

        // 点・直線状などs0/s1が0の退化したBBはGetBoundingBox()が例外を投げる
        // (3DのBBを構成できない)。深度推定には使えないためnulloptを返し、
        // 呼び出し側のフォールバック深度に委ねる。
        const auto defined = geom->GetDefinedBoundingBox();
        const auto def_sizes = defined.GetSizes();
        if (defined.IsEmpty() || def_sizes[0] <= 0.0 || def_sizes[1] <= 0.0) {
            return std::nullopt;
        }

        // GetBoundingBox()はエンティティ自身のDE変換を適用済み(親空間)。
        // world_transform_ (親→ワールド) を適用してワールド空間のBBを得る。
        auto bb = geom->GetBoundingBox();
        const igesio::Matrix4d wt = world_transform_.cast<double>();
        const igesio::Matrix3d r = wt.block<3, 3>(0, 0);
        const igesio::Vector3d t = wt.block<3, 1>(0, 3);
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
    /// @brief エンティティの描画を実装する関数
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note OpenGLの描画コマンドを実行する
    virtual void DrawImpl(GLuint, const std::pair<float, float>&) const = 0;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_ENTITY_GRAPHICS_H_
