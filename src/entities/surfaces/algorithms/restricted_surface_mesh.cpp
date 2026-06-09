/**
 * @file entities/surfaces/algorithms/restricted_surface_mesh.cpp
 * @brief 制限付き曲面 (Type 143/144 等) の三角形メッシュ生成の実装
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/surfaces/algorithms/restricted_surface_mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/numerics/geometric/polygon.h"

namespace igesio::entities {

namespace {

namespace num = igesio::numerics;

/// @brief エッジ交差点の2分探索の反復回数
/// @note 14回で境界位置の誤差がエッジ長の 1/2^14 ≈ 6e-5 以下になる
constexpr int kCrossingSearchIter = 14;

/// @brief メモ化された格子点の評価結果
struct VertexInfo {
    /// @brief トリム領域内かどうか (評価に成功した場合のみtrue)
    bool valid = false;
    /// @brief validのときの頂点列インデックス (無効点は-1)
    int col = -1;
};

/// @brief (u,v) の頂点データ {x,y,z,nx,ny,nz,tu,tv} を計算する
/// @return トリム領域外または評価失敗の場合はstd::nullopt
std::optional<std::array<float, 8>> ComputeVertexData(
        const IRestrictedSurface& surface, const double u, const double v,
        const std::array<double, 2>& u_range,
        const std::array<double, 2>& v_range) {
    auto deriv_opt = surface.TryGetDerivatives(u, v, 1);
    if (!deriv_opt) return std::nullopt;

    // NOTE: cast<float>()は遅延評価式を返すため、autoで受けると一時オブジェクト
    //       へのダングリング参照となる。具体型で受けて即時評価・コピーさせる。
    const Vector3f pos = (*deriv_opt)(0, 0).cast<float>();
    const Vector3d su = (*deriv_opt)(1, 0);
    const Vector3d sv = (*deriv_opt)(0, 1);
    const Vector3d n_vec = su.cross(sv);
    const double len = n_vec.norm();
    const Vector3f normal =
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
/// @return 境界に最も近い有効点のUV座標
/// @note 内外判定のみ必要なため、点評価を伴わないIsInDomainを使用する
std::pair<double, double> FindEdgeCrossing(
        const IRestrictedSurface& surface,
        double u0, double v0, double u1, double v1) {
    for (int k = 0; k < kCrossingSearchIter; ++k) {
        const double um = (u0 + u1) * 0.5;
        const double vm = (v0 + v1) * 0.5;
        if (surface.IsInDomain(um, vm)) {
            u0 = um;
            v0 = vm;
        } else {
            u1 = um;
            v1 = vm;
        }
    }
    return {u0, v0};  // 有効端側の収束点を返す
}

/// @brief 境界駆動の制限付き四分木によるメッシュ生成器
/// @note アルゴリズムの詳細は restricted_surface_mesh.h を参照
class QuadtreeMesher {
 public:
    /// @brief コンストラクタ
    /// @param surface 対象の制限付き曲面
    /// @param base_div 基底グリッド分割数 (1以上にクランプ)
    /// @param max_depth 境界セル細分の最大深さ (0以上にクランプ)
    QuadtreeMesher(const IRestrictedSurface& surface, const int base_div,
                   const int max_depth)
            : surface_(surface),
              base_div_(std::max(1, base_div)),
              max_depth_(std::max(0, max_depth)),
              sub_(1 << max_depth_),
              nv_(base_div_ * sub_),
              depth_(static_cast<size_t>(base_div_) * base_div_, 0) {
        const auto range = surface.GetParameterRange();
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
    MatrixXf TakeVertices() {
        vertices_.conservativeResize(8, n_verts_);
        return std::move(vertices_);
    }

    /// @brief 構築したインデックス列を取り出す
    std::vector<std::uint32_t> TakeIndices() {
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

    /// @brief ルートセル (ri,rj) の深さを返す (範囲外は-1)
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
        const auto vd = ComputeVertexData(surface_, u, v, u_range_, v_range_);
        VertexInfo info;
        if (vd) { info.valid = true; info.col = AddVertexData(*vd); }
        vmemo_.emplace(key, info);
        return info;
    }

    /// @brief サブ辺上の境界交差点を評価し、メモ化して返す
    /// @param iv,jv 有効端の仮想格子座標
    /// @param ii,ji 無効端の仮想格子座標
    /// @return 交差頂点の列インデックス (評価失敗は-1)
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
        const auto [uc, vc] = FindEdgeCrossing(surface_, u0, v0, u1, v1);
        const auto vd = ComputeVertexData(surface_, uc, vc, u_range_, v_range_);
        const int col = vd ? AddVertexData(*vd) : -1;
        cmemo_.emplace(key, col);
        return col;
    }

    /// @brief トリム境界が通過するルートを最大深さでマークする
    /// @note 境界の包含多角形 (テッセレーション用) を流用するため、境界曲線の
    ///       例外・退化処理は不要 (構築失敗時は当該多角形が空となりスキップされる)
    void MarkBoundaryRoots() {
        // 外側境界 (制限あり時); パラメータ矩形と一致する場合はnullopt
        if (const auto& outer = surface_.GetOuterDomainPolygon()) {
            MarkPolygon(outer->approximate);
        }
        // 内側境界 (穴)
        for (const auto& inner : surface_.GetInnerDomainPolygons()) {
            MarkPolygon(inner.approximate);
        }
    }

    /// @brief 境界包含多角形 (UV空間の閉ループ) が通過するルートをマークする
    void MarkPolygon(const num::PolygonData& poly) {
        const double du = u_range_[1] - u_range_[0];
        const double dv = v_range_[1] - v_range_[0];
        if (du == 0.0 || dv == 0.0) return;
        const int n = poly.Count();
        if (n < 2) return;
        double pu = 0.0, pv = 0.0;
        for (int i = 0; i <= n; ++i) {
            const auto& p = poly.vertices[i % n];
            const double fu = (p[0] - u_range_[0]) / du * base_div_;
            const double fv = (p[1] - v_range_[0]) / dv * base_div_;
            if (i > 0) MarkSegment(pu, pv, fu, fv);
            pu = fu;
            pv = fv;
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
            // コーナー(n+1)^2 + 辺交差点・ハンギングノードの上界4n^2
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

        // 周回頂点をCCW順に列挙 (BL→bottom→BR→right→TR→top→TL→left)
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

        // 有効コーナーと境界交差点でCCW多角形を構築する
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
            indices_.push_back(static_cast<std::uint32_t>(poly[0]));
            indices_.push_back(static_cast<std::uint32_t>(poly[k]));
            indices_.push_back(static_cast<std::uint32_t>(poly[k + 1]));
        }
    }

    /// @brief 対象の制限付き曲面
    const IRestrictedSurface& surface_;
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
    MatrixXf vertices_;
    /// @brief 確定済み頂点数
    int n_verts_ = 0;
    /// @brief 三角形インデックス
    std::vector<std::uint32_t> indices_;
    /// @brief 仮想格子点キー → 評価結果 のメモ
    std::unordered_map<int64_t, VertexInfo> vmemo_;
    /// @brief サブ辺キー → 交差頂点列インデックス のメモ
    std::map<std::pair<int64_t, int64_t>, int> cmemo_;
};

}  // namespace



RestrictedSurfaceMesh TessellateRestrictedSurface(
        const IRestrictedSurface& surface,
        const RestrictedSurfaceMeshParams& params) {
    QuadtreeMesher mesher(surface, params.base_div, params.max_depth);
    mesher.Build();

    RestrictedSurfaceMesh mesh;
    mesh.vertices = mesher.TakeVertices();
    mesh.indices = mesher.TakeIndices();
    return mesh;
}

}  // namespace igesio::entities
