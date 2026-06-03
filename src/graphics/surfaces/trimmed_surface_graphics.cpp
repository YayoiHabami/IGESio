/**
 * @file graphics/surfaces/trimmed_surface_graphics.cpp
 * @brief TrimmedSurfaceの描画用クラスの実装
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/surfaces/trimmed_surface_graphics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/entities/interfaces/i_curve.h"

namespace {

namespace i_ent = igesio::entities;
namespace gl = igesio::graphics::gl;
using TrimmedSurf = i_ent::TrimmedSurface;
using igesio::Vector3d;

/// @brief 基底グリッド分割数 (u, v 各方向)
/// @note トリム境界が通らない領域はこの粗さで三角分割される
constexpr int kDefaultDiv = 32;
/// @brief 境界セルを細分する四分木の最大深さ
/// @note 局所的に実効分割数が kDefaultDiv * 2^kMaxQuadtreeDepth まで上がる
///       (既定では 32 * 4 = 128 相当)。細い形状・欠け角の脱落を救済する
constexpr int kMaxQuadtreeDepth = 2;
/// @brief エッジ交差点の2分探索の反復回数
/// @note 14回で境界位置の誤差がエッジ長の 1/2^14 ≈ 6e-5 以下になる
constexpr int kCrossingSearchIter = 14;

/// @brief メモ化された格子点の評価結果
struct VertexInfo {
    /// @brief トリム領域内かどうか (評価に成功した場合のみ true)
    bool valid = false;
    /// @brief valid のときの頂点列インデックス (無効点は -1)
    int col = -1;
};

/// @brief (u,v) の頂点データ {x,y,z,nx,ny,nz,tu,tv} を計算する
/// @return トリム領域外または評価失敗の場合は std::nullopt
std::optional<std::array<float, 8>> ComputeVertexData(
        const TrimmedSurf& entity, const double u, const double v,
        const std::array<double, 2>& u_range,
        const std::array<double, 2>& v_range) {
    auto deriv_opt = entity.TryGetDerivatives(u, v, 1);
    if (!deriv_opt) return std::nullopt;

    // NOTE: cast<float>()は遅延評価式を返すため、autoで受けると一時オブジェクト
    //       へのダングリング参照となる。具体型で受けて即時評価・コピーさせる。
    const igesio::Vector3f pos = (*deriv_opt)(0, 0).cast<float>();
    const Vector3d su = (*deriv_opt)(1, 0);
    const Vector3d sv = (*deriv_opt)(0, 1);
    const Vector3d n_vec = su.cross(sv);
    const double len = n_vec.norm();
    const igesio::Vector3f normal =
            ((len > 1e-10) ? (n_vec / len) : Vector3d(0, 0, 1)).cast<float>();

    const float tu = static_cast<float>((u - u_range[0]) /
                                        (u_range[1] - u_range[0]));
    const float tv = static_cast<float>((v - v_range[0]) /
                                        (v_range[1] - v_range[0]));
    return std::array<float, 8>{
        pos[0], pos[1], pos[2],
        normal[0], normal[1], normal[2],
        tu, tv
    };
}

/// @brief エッジ上のトリム境界交差点を2分探索で求める
/// @param u0, v0 有効端 (トリム領域内)
/// @param u1, v1 無効端 (トリム領域外)
/// @return 境界に最も近い有効点の UV 座標
std::pair<double, double> FindEdgeCrossing(
        const TrimmedSurf& entity,
        double u0, double v0, double u1, double v1) {
    for (int k = 0; k < kCrossingSearchIter; ++k) {
        const double um = (u0 + u1) * 0.5;
        const double vm = (v0 + v1) * 0.5;
        // order=0 で評価コストを抑えつつ有効性のみ確認する
        if (entity.TryGetDerivatives(um, vm, 0).has_value()) {
            u0 = um;
            v0 = vm;
        } else {
            u1 = um;
            v1 = vm;
        }
    }
    return {u0, v0};  // 有効端側の収束点を返す
}

/// @brief 境界駆動の制限付き四分木によるトリム面メッシュ生成器
///
/// 基底 kDefaultDiv x kDefaultDiv グリッドを四分木のルートとし、トリム境界曲線が
/// 通過するルートのみを深さ kMaxQuadtreeDepth まで細分する。隣接ルートの深さ差を
/// 1以下に均すこと(2:1バランス)で、各辺のハンギングノードを高々1個に抑え、
/// ハンギングノードを周回に含めることでクラックフリーな三角分割を得る。
///
/// 全頂点・交差点を「仮想最細格子」(kDefaultDiv * 2^kMaxQuadtreeDepth 解像度)の
/// 整数座標でキー化して重複排除するため、隣接する葉は境界カットを含め必ず同一の
/// 頂点を共有する。各葉では Marching-Squares と同じく有効コーナー+辺交差点で
/// CCW多角形を作りファン三角分割するので、穴の過剰被覆・3Dキャップ・格子退化と
/// いった点群Delaunay方式の失敗モードを構造的に回避する。
class QuadtreeMesher {
 public:
    /// @brief コンストラクタ
    /// @param entity 対象のトリム面
    /// @param base_div 基底グリッド分割数
    /// @param max_depth 境界セル細分の最大深さ
    QuadtreeMesher(const TrimmedSurf& entity, const int base_div,
                   const int max_depth)
            : entity_(entity), base_div_(base_div), max_depth_(max_depth),
              sub_(1 << max_depth), nv_(base_div * (1 << max_depth)),
              depth_(static_cast<size_t>(base_div) * base_div, 0) {
        const auto range = entity.GetParameterRange();
        u_range_ = {range[0], range[1]};
        v_range_ = {range[2], range[3]};
    }

    /// @brief メッシュを構築する
    void Build() {
        MarkBoundaryRoots();
        SmoothDepths();
        AllocateVertices();
        EmitAllLeaves();
    }

    /// @brief 構築した頂点行列を取り出す (使用列数に切り詰める)
    igesio::MatrixXf TakeVertices() {
        vertices_.conservativeResize(8, n_verts_);
        return std::move(vertices_);
    }

    /// @brief 構築したインデックス列を取り出す
    std::vector<gl::Uint> TakeIndices() {
        return std::move(indices_);
    }

 private:
    /// @brief 仮想最細格子点 (I,J) のキーを返す
    int64_t VKey(const int i, const int j) const {
        return static_cast<int64_t>(i) * (nv_ + 1) + j;
    }

    /// @brief 仮想最細格子座標 (I,J) を実パラメータ (u,v) に変換する
    std::pair<double, double> VirtToUV(const int i, const int j) const {
        const double u = u_range_[0]
                + (u_range_[1] - u_range_[0]) * i / static_cast<double>(nv_);
        const double v = v_range_[0]
                + (v_range_[1] - v_range_[0]) * j / static_cast<double>(nv_);
        return {u, v};
    }

    /// @brief ルートセル (ri,rj) の深さを返す (範囲外は -1)
    int RootDepth(const int ri, const int rj) const {
        if (ri < 0 || ri >= base_div_ || rj < 0 || rj >= base_div_) return -1;
        return depth_[static_cast<size_t>(ri) * base_div_ + rj];
    }

    /// @brief 頂点データを次の列に書き込む
    /// @return 書き込んだ列インデックス
    int AddVertexData(const std::array<float, 8>& vdata) {
        const int col = n_verts_;
        for (int r = 0; r < 8; ++r) vertices_(r, col) = vdata[r];
        ++n_verts_;
        return col;
    }

    /// @brief 仮想格子点 (I,J) を評価し、結果をメモ化して返す
    VertexInfo GetVertex(const int i, const int j) {
        const int64_t key = VKey(i, j);
        const auto it = vmemo_.find(key);
        if (it != vmemo_.end()) return it->second;

        const auto [u, v] = VirtToUV(i, j);
        const auto vd = ComputeVertexData(entity_, u, v, u_range_, v_range_);
        VertexInfo info;
        if (vd) {
            info.valid = true;
            info.col = AddVertexData(*vd);
        }
        vmemo_.emplace(key, info);
        return info;
    }

    /// @brief サブ辺上の境界交差点を評価し、メモ化して返す
    /// @param iv,jv 有効端の仮想格子座標
    /// @param ii,ji 無効端の仮想格子座標
    /// @return 交差頂点の列インデックス (評価失敗は -1)
    /// @note キーをサブ辺端点の (min,max) で正規化するため、辺を共有する隣接葉は
    ///       走査方向によらず同一の交差頂点を共有する
    int GetCrossing(const int iv, const int jv, const int ii, const int ji) {
        const int64_t kv = VKey(iv, jv);
        const int64_t ki = VKey(ii, ji);
        const std::pair<int64_t, int64_t> key =
                (kv < ki) ? std::pair<int64_t, int64_t>{kv, ki}
                          : std::pair<int64_t, int64_t>{ki, kv};
        const auto it = cmemo_.find(key);
        if (it != cmemo_.end()) return it->second;

        const auto [u0, v0] = VirtToUV(iv, jv);
        const auto [u1, v1] = VirtToUV(ii, ji);
        const auto [uc, vc] = FindEdgeCrossing(entity_, u0, v0, u1, v1);
        const auto vd = ComputeVertexData(entity_, uc, vc, u_range_, v_range_);
        const int col = vd ? AddVertexData(*vd) : -1;
        cmemo_.emplace(key, col);
        return col;
    }

    /// @brief トリム境界曲線が通過するルートを最大深さでマークする
    void MarkBoundaryRoots() {
        // 外側境界 (N1=1 のとき); パラメータ矩形と一致する場合 (N1=0) は通らない
        if (!entity_.IsOuterBoundaryOfD()) {
            MarkBoundary(entity_.GetOuterBoundary());
        }
        // 内側境界 (穴)
        const size_t n_inner = entity_.GetInnerBoundaryCount();
        for (size_t i = 0; i < n_inner; ++i) {
            MarkBoundary(entity_.GetInnerBoundaryAt(i));
        }
    }

    /// @brief 1本のトリム境界(142)の基底曲線 B(t)=(u,v) を標本化してルートを
    ///        マークする
    /// @note GetBaseCurve は未設定時に例外を投げ、退化曲線は評価に失敗しうるため、
    ///       これらをまとめて捕捉し、失敗時は当該境界をスキップする
    ///       (細分されず粗い MS にフォールバック)
    void MarkBoundary(
            const std::shared_ptr<const i_ent::CurveOnAParametricSurface>& bnd) {
        if (!bnd) return;
        const double du = u_range_[1] - u_range_[0];
        const double dv = v_range_[1] - v_range_[0];
        if (du == 0.0 || dv == 0.0) return;
        try {
            const auto base = bnd->GetBaseCurve();
            if (!base) return;
            const auto range = base->GetParameterRange();
            const double t0 = range[0], t1 = range[1];
            if (!std::isfinite(t0) || !std::isfinite(t1) || t1 <= t0) return;

            // 仮想最細格子の2倍密度で標本化し、線分ごとに通過セルを埋める
            const int n_samples = nv_ * 2;
            bool has_prev = false;
            double pu = 0.0, pv = 0.0;
            for (int s = 0; s <= n_samples; ++s) {
                const double t = t0 + (t1 - t0) * s / n_samples;
                const auto p = base->TryGetPointAt(t);
                if (!p) {
                    has_prev = false;
                    continue;
                }
                const double fu = ((*p)[0] - u_range_[0]) / du * base_div_;
                const double fv = ((*p)[1] - v_range_[0]) / dv * base_div_;
                if (has_prev) MarkSegment(pu, pv, fu, fv);
                else          MarkSegment(fu, fv, fu, fv);
                pu = fu;
                pv = fv;
                has_prev = true;
            }
        } catch (const std::exception&) {
            // 当該境界はスキップ
        }
    }

    /// @brief ルートセル座標 (浮動小数) の線分が通過するセルをマークする
    void MarkSegment(const double fu0, const double fv0,
                     const double fu1, const double fv1) {
        const int steps = std::max(1, static_cast<int>(std::ceil(
                std::max(std::abs(fu1 - fu0), std::abs(fv1 - fv0)) * 2.0)));
        for (int s = 0; s <= steps; ++s) {
            const double t = static_cast<double>(s) / steps;
            const double fu = fu0 + (fu1 - fu0) * t;
            const double fv = fv0 + (fv1 - fv0) * t;
            const int ri = std::clamp(
                    static_cast<int>(std::floor(fu)), 0, base_div_ - 1);
            const int rj = std::clamp(
                    static_cast<int>(std::floor(fv)), 0, base_div_ - 1);
            depth_[static_cast<size_t>(ri) * base_div_ + rj] = max_depth_;
        }
    }

    /// @brief 隣接ルートの深さ差が1以下になるよう深さ場を緩和する (2:1バランス)
    void SmoothDepths() {
        bool changed = true;
        while (changed) {
            changed = false;
            for (int ri = 0; ri < base_div_; ++ri) {
                for (int rj = 0; rj < base_div_; ++rj) {
                    const int max_n = std::max({
                            RootDepth(ri - 1, rj), RootDepth(ri + 1, rj),
                            RootDepth(ri, rj - 1), RootDepth(ri, rj + 1)});
                    int& d = depth_[static_cast<size_t>(ri) * base_div_ + rj];
                    if (d < max_n - 1) {
                        d = max_n - 1;
                        changed = true;
                    }
                }
            }
        }
    }

    /// @brief 全葉から見た頂点数の上界で頂点行列を事前確保する
    void AllocateVertices() {
        int64_t est = 0;
        for (int d : depth_) {
            const int n = 1 << d;
            // コーナー (n+1)^2 + 辺交差点・ハンギングノードの上界 4n^2
            est += static_cast<int64_t>(n + 1) * (n + 1)
                 + static_cast<int64_t>(4) * n * n;
        }
        vertices_.resize(8, static_cast<int>(est));
        indices_.reserve(static_cast<size_t>(est) * 2);
    }

    /// @brief 全ルートの葉を走査して三角形を生成する
    void EmitAllLeaves() {
        for (int ri = 0; ri < base_div_; ++ri) {
            for (int rj = 0; rj < base_div_; ++rj) {
                const int level = RootDepth(ri, rj);
                const int n = 1 << level;
                const int leaf_size = sub_ >> level;
                for (int li = 0; li < n; ++li) {
                    for (int lj = 0; lj < n; ++lj) {
                        EmitLeaf(ri * sub_ + li * leaf_size,
                                 rj * sub_ + lj * leaf_size, leaf_size,
                                 level, ri, rj, li, lj, n);
                    }
                }
            }
        }
    }

    /// @brief 1つの葉セルを三角分割してインデックスに追加する
    /// @param i0,j0 葉の左下コーナーの仮想格子座標
    /// @param size 葉の一辺の仮想格子ステップ数
    /// @param level 葉が属するルートの深さ
    /// @param ri,rj ルートセル座標
    /// @param li,lj ルート内の葉インデックス
    /// @param n ルート内の一辺あたりの葉数 (= 2^level)
    void EmitLeaf(const int i0, const int j0, const int size,
                  const int level, const int ri, const int rj,
                  const int li, const int lj, const int n) {
        // 各辺のハンギングノード: 当該辺がルート境界上にあり、かつ隣接ルートが
        // ちょうど1段細かい (level+1) ときのみ中点ノードを挿入する
        const bool h_bot = (lj == 0)     && RootDepth(ri, rj - 1) == level + 1;
        const bool h_rgt = (li == n - 1) && RootDepth(ri + 1, rj) == level + 1;
        const bool h_top = (lj == n - 1) && RootDepth(ri, rj + 1) == level + 1;
        const bool h_lft = (li == 0)     && RootDepth(ri - 1, rj) == level + 1;
        const int hs = size / 2;

        // 周回頂点を CCW 順に列挙 (BL→bottom→BR→right→TR→top→TL→left)
        std::array<std::pair<int, int>, 8> perim;
        int m = 0;
        perim[m++] = {i0, j0};
        if (h_bot) perim[m++] = {i0 + hs, j0};
        perim[m++] = {i0 + size, j0};
        if (h_rgt) perim[m++] = {i0 + size, j0 + hs};
        perim[m++] = {i0 + size, j0 + size};
        if (h_top) perim[m++] = {i0 + hs, j0 + size};
        perim[m++] = {i0, j0 + size};
        if (h_lft) perim[m++] = {i0, j0 + hs};

        // 有効コーナーと境界交差点で CCW 多角形を構築する
        // (最大: コーナー8 + サブ辺交差点8 = 16)
        std::array<int, 18> poly;
        int np = 0;
        for (int k = 0; k < m; ++k) {
            const auto [ia, ja] = perim[k];
            const auto [ib, jb] = perim[(k + 1) % m];
            const VertexInfo va = GetVertex(ia, ja);
            const VertexInfo vb = GetVertex(ib, jb);
            if (va.valid) poly[np++] = va.col;
            if (va.valid != vb.valid) {
                const int cross = va.valid ? GetCrossing(ia, ja, ib, jb)
                                           : GetCrossing(ib, jb, ia, ja);
                if (cross >= 0) poly[np++] = cross;
            }
        }

        // poly[0] 基点でファン三角分割
        for (int k = 1; k + 1 < np; ++k) {
            indices_.push_back(static_cast<gl::Uint>(poly[0]));
            indices_.push_back(static_cast<gl::Uint>(poly[k]));
            indices_.push_back(static_cast<gl::Uint>(poly[k + 1]));
        }
    }

    /// @brief 対象のトリム面
    const TrimmedSurf& entity_;
    /// @brief 基底グリッド分割数
    const int base_div_;
    /// @brief 境界セル細分の最大深さ
    const int max_depth_;
    /// @brief ルート1辺あたりの仮想格子ステップ数 (= 2^max_depth_)
    const int sub_;
    /// @brief 仮想最細格子の1軸あたり分割数 (= base_div_ * sub_)
    const int nv_;
    /// @brief パラメータ範囲 {u_min, u_max}
    std::array<double, 2> u_range_;
    /// @brief パラメータ範囲 {v_min, v_max}
    std::array<double, 2> v_range_;
    /// @brief 各ルートセルの四分木深さ
    std::vector<int> depth_;
    /// @brief 頂点データ {x,y,z,nx,ny,nz,tu,tv} (各列が1頂点)
    igesio::MatrixXf vertices_;
    /// @brief 確定済み頂点数
    int n_verts_ = 0;
    /// @brief 三角形インデックス
    std::vector<gl::Uint> indices_;
    /// @brief 仮想格子点キー → 評価結果 のメモ
    std::unordered_map<int64_t, VertexInfo> vmemo_;
    /// @brief サブ辺キー → 交差頂点列インデックス のメモ
    std::map<std::pair<int64_t, int64_t>, int> cmemo_;
};

}  // namespace



/**
 * constructor / destructor
 */

igesio::graphics::TrimmedSurfaceGraphics::TrimmedSurfaceGraphics(
        const std::shared_ptr<const entities::TrimmedSurface> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, ShaderType::kGeneralSurface, true) {
    Synchronize();
}

igesio::graphics::TrimmedSurfaceGraphics::~TrimmedSurfaceGraphics() {
    Cleanup();
}



/**
 * protected
 */

void igesio::graphics::TrimmedSurfaceGraphics::DrawImpl(
        gl::Uint /*shader*/,
        const std::pair<float, float>& /*viewport*/) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawElements(gl::kTriangles, indices_.size(), gl::kUnsignedInt, 0);
    gl_->BindVertexArray(0);
}



/**
 * public
 */

void igesio::graphics::TrimmedSurfaceGraphics::Synchronize() {
    Cleanup();
    SyncTexture();
    GenerateSurfaceData();

    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);
    gl_->GenBuffers(1, &ebo_);

    gl_->BindVertexArray(vao_);

    gl_->BindBuffer(gl::kArrayBuffer, vbo_);
    gl_->BufferData(gl::kArrayBuffer,
                    vertices_.size() * sizeof(float),
                    vertices_.data(), gl::kStaticDraw);
    // 位置 (location=0), 法線 (location=1), テクスチャ座標 (location=2)
    gl_->VertexAttribPointer(0, 3, gl::kFloat, gl::kFalse, 8 * sizeof(float),
                             nullptr);
    gl_->EnableVertexAttribArray(0);
    gl_->VertexAttribPointer(1, 3, gl::kFloat, gl::kFalse, 8 * sizeof(float),
                             reinterpret_cast<const void*>(3 * sizeof(float)));
    gl_->EnableVertexAttribArray(1);
    gl_->VertexAttribPointer(2, 2, gl::kFloat, gl::kFalse, 8 * sizeof(float),
                             reinterpret_cast<const void*>(6 * sizeof(float)));
    gl_->EnableVertexAttribArray(2);

    gl_->BindBuffer(gl::kElementArrayBuffer, ebo_);
    gl_->BufferData(gl::kElementArrayBuffer,
                    indices_.size() * sizeof(gl::Uint),
                    indices_.data(), gl::kStaticDraw);

    gl_->BindVertexArray(0);
}

void igesio::graphics::TrimmedSurfaceGraphics::Cleanup() {
    EntityGraphics::Cleanup();

    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (ebo_ != 0) {
        gl_->DeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }

    vertices_.resize(0, 0);
    indices_.clear();
}

igesio::graphics::SelectionSamples
igesio::graphics::TrimmedSurfaceGraphics::GetSelectionSamples(
        const SelectionSampleParams& params) const {
    if (!entity_) return {};

    const igesio::Matrix4d wt = world_transform_.cast<double>();
    SelectionSamples result;

    // 外周ループ: N1=0 ならパラメータ矩形、N1=1 ならトリム境界曲線(142)を使用
    if (entity_->IsOuterBoundaryOfD()) {
        SelectionSamples base = SampleSurfaceBoundary(*entity_, params, wt);
        for (auto& pl : base.polylines) {
            result.polylines.push_back(std::move(pl));
        }
    } else if (auto outer = entity_->GetOuterBoundary()) {
        // CurveOnAParametricSurface(142) は ICurve
        SelectionSamples outer_s = SampleCurve(*outer, params, wt);
        for (auto& pl : outer_s.polylines) {
            result.polylines.push_back(std::move(pl));
        }
    }

    // 内周 (穴) ループ
    const size_t n_inner = entity_->GetInnerBoundaryCount();
    for (size_t i = 0; i < n_inner; ++i) {
        auto inner = entity_->GetInnerBoundaryAt(i);
        if (!inner) continue;
        SelectionSamples inner_s = SampleCurve(*inner, params, wt);
        for (auto& pl : inner_s.polylines) {
            result.polylines.push_back(std::move(pl));
        }
    }

    // トリム領域内の内部グリッド点を追加する (IsInDomainで穴/外側を除外)
    SampleSurfaceInteriorGrid(*entity_, params, wt, result);

    // 各ループを閉じた境界とみなし偶奇規則で内外判定する (穴対応はrenderer側)
    result.polylines_closed = !result.polylines.empty();
    return result;
}



/**
 * private
 */

void igesio::graphics::TrimmedSurfaceGraphics::GenerateSurfaceData() {
    QuadtreeMesher mesher(*entity_, kDefaultDiv, kMaxQuadtreeDepth);
    mesher.Build();
    vertices_ = mesher.TakeVertices();
    indices_ = mesher.TakeIndices();
}
