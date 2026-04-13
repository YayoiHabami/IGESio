/**
 * @file graphics/surfaces/trimmed_surface_graphics.cpp
 * @brief TrimmedSurfaceの描画用クラスの実装
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/surfaces/trimmed_surface_graphics.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

namespace {

namespace i_ent = igesio::entities;
using TrimmedSurf = i_ent::TrimmedSurface;
using igesio::Vector3d;

/// @brief グリッド分割数 (u, v 各方向)
constexpr int kDefaultDiv = 32;
/// @brief エッジ交差点の2分探索の反復回数
/// @note 14回で境界位置の誤差がエッジ長の 1/2^14 ≈ 6e-5 以下になる
constexpr int kCrossingSearchIter = 14;

/// @brief パラメータ値を計算する
double ComputeParam(
        const int i, const int div,
        const std::array<double, 2>& range) {
    return range[0] + (range[1] - range[0]) * i / static_cast<double>(div);
}

/// @brief グリッド頂点のフラット化インデックスを返す
int GridIdx(const int v_div, const int i, const int j) {
    return i * (v_div + 1) + j;
}

/// @brief 水平エッジ交差配列のフラット化インデックスを返す
/// @note 水平エッジ (i,j)-(i+1,j) のインデックス; サイズは u_div*(v_div+1)
int HCrossIdx(const int v_div, const int i, const int j) {
    return i * (v_div + 1) + j;
}

/// @brief 垂直エッジ交差配列のフラット化インデックスを返す
/// @note 垂直エッジ (i,j)-(i,j+1) のインデックス; サイズは (u_div+1)*v_div
int VCrossIdx(const int v_div, const int i, const int j) {
    return i * v_div + j;
}

/// @brief (u,v) の頂点データ {x,y,z,nx,ny,nz,tu,tv} を計算する
/// @return トリム領域外または評価失敗の場合は std::nullopt
std::optional<std::array<float, 8>> ComputeVertexData(
        const TrimmedSurf& entity, const double u, const double v,
        const std::array<double, 2>& u_range,
        const std::array<double, 2>& v_range) {
    auto deriv_opt = entity.TryGetDerivatives(u, v, 1);
    if (!deriv_opt) return std::nullopt;

    auto pos = (*deriv_opt)(0, 0).cast<float>();
    const Vector3d su = (*deriv_opt)(1, 0);
    const Vector3d sv = (*deriv_opt)(0, 1);
    const Vector3d n_vec = su.cross(sv);
    const double len = n_vec.norm();
    auto normal = ((len > 1e-10) ? (n_vec / len) : Vector3d(0, 0, 1)).cast<float>();

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
            u0 = um; v0 = vm;
        } else {
            u1 = um; v1 = vm;
        }
    }
    return {u0, v0};  // 有効端側の収束点を返す
}

/// @brief 頂点データを MatrixXf の次の列に書き込む
/// @return 書き込んだ列インデックス (= 書き込み前の n_verts)
int AddVertex(
        const std::array<float, 8>& vdata,
        igesio::MatrixXf& vertices,
        int& n_verts) {
    const int col = n_verts;
    for (int r = 0; r < 8; ++r) {
        vertices(r, col) = vdata[r];
    }
    ++n_verts;
    return col;
}

/// @brief グリッド頂点と有効フラグを構築する
void BuildGridVertices(
        const TrimmedSurf& entity,
        const int u_div, const int v_div,
        const std::array<double, 2>& u_range,
        const std::array<double, 2>& v_range,
        std::vector<bool>& is_valid,
        igesio::MatrixXf& vertices,
        int& n_verts) {
    for (int i = 0; i <= u_div; ++i) {
        const double u = ComputeParam(i, u_div, u_range);
        for (int j = 0; j <= v_div; ++j) {
            const double v = ComputeParam(j, v_div, v_range);
            const auto vdata_opt =
                    ComputeVertexData(entity, u, v, u_range, v_range);
            is_valid[GridIdx(v_div, i, j)] = vdata_opt.has_value();
            // 無効点もダミー頂点として追加し、インデックスの整合性を保つ
            const std::array<float, 8> vdata = vdata_opt.value_or(
                std::array<float, 8>{
                    0.f, 0.f, 0.f, 0.f, 0.f, 1.f,
                    static_cast<float>(i) / u_div,
                    static_cast<float>(j) / v_div
                });
            AddVertex(vdata, vertices, n_verts);
        }
    }
}

/// @brief 水平エッジ (i,j)-(i+1,j) の交差頂点を構築する
void BuildHCrossings(
        const TrimmedSurf& entity,
        const int u_div, const int v_div,
        const std::array<double, 2>& u_range,
        const std::array<double, 2>& v_range,
        const std::vector<bool>& is_valid,
        std::vector<int>& h_cross,
        igesio::MatrixXf& vertices,
        int& n_verts) {
    for (int i = 0; i < u_div; ++i) {
        const double u0 = ComputeParam(i, u_div, u_range);
        const double u1 = ComputeParam(i + 1, u_div, u_range);
        for (int j = 0; j <= v_div; ++j) {
            const double v = ComputeParam(j, v_div, v_range);
            const bool v0 = is_valid[GridIdx(v_div, i, j)];
            const bool v1 = is_valid[GridIdx(v_div, i + 1, j)];
            if (v0 == v1) continue;

            // 有効端から無効端へ向けて2分探索
            auto [uc, vc] = v0
                ? FindEdgeCrossing(entity, u0, v, u1, v)
                : FindEdgeCrossing(entity, u1, v, u0, v);
            const auto vdata_opt =
                    ComputeVertexData(entity, uc, vc, u_range, v_range);
            if (!vdata_opt) continue;

            h_cross[HCrossIdx(v_div, i, j)] =
                    AddVertex(*vdata_opt, vertices, n_verts);
        }
    }
}

/// @brief 垂直エッジ (i,j)-(i,j+1) の交差頂点を構築する
void BuildVCrossings(
        const TrimmedSurf& entity,
        const int u_div, const int v_div,
        const std::array<double, 2>& u_range,
        const std::array<double, 2>& v_range,
        const std::vector<bool>& is_valid,
        std::vector<int>& v_cross,
        igesio::MatrixXf& vertices,
        int& n_verts) {
    for (int i = 0; i <= u_div; ++i) {
        const double u = ComputeParam(i, u_div, u_range);
        for (int j = 0; j < v_div; ++j) {
            const double v0 = ComputeParam(j, v_div, v_range);
            const double v1 = ComputeParam(j + 1, v_div, v_range);
            const bool val0 = is_valid[GridIdx(v_div, i, j)];
            const bool val1 = is_valid[GridIdx(v_div, i, j + 1)];
            if (val0 == val1) continue;

            auto [uc, vc] = val0
                ? FindEdgeCrossing(entity, u, v0, u, v1)
                : FindEdgeCrossing(entity, u, v1, u, v0);
            const auto vdata_opt =
                    ComputeVertexData(entity, uc, vc, u_range, v_range);
            if (!vdata_opt) continue;

            v_cross[VCrossIdx(v_div, i, j)] =
                    AddVertex(*vdata_opt, vertices, n_verts);
        }
    }
}

/// @brief セルのトリム有効ポリゴンをCCW順の頂点インデックス列で返す
/// @note CCW走査順: BL → bottom_cross → BR → right_cross →
///                  TR → top_cross → TL → left_cross
/// @param bl,br,tr,tl 各コーナーの有効フラグ
/// @param idx_* 各コーナーの頂点インデックス
/// @param h_bot,v_right,h_top,v_left エッジ交差頂点インデックス (-1 = なし)
std::vector<int> BuildCellPolygon(
        const bool bl, const bool br,
        const bool tr, const bool tl,
        const int idx_bl, const int idx_br,
        const int idx_tr, const int idx_tl,
        const int h_bot, const int v_right,
        const int h_top, const int v_left) {
    std::vector<int> poly;
    poly.reserve(8);
    if (bl)       poly.push_back(idx_bl);
    if (h_bot >= 0) poly.push_back(h_bot);
    if (br)       poly.push_back(idx_br);
    if (v_right >= 0) poly.push_back(v_right);
    if (tr)       poly.push_back(idx_tr);
    if (h_top >= 0)  poly.push_back(h_top);
    if (tl)       poly.push_back(idx_tl);
    if (v_left >= 0) poly.push_back(v_left);
    return poly;
}

/// @brief 各セルの三角形インデックスを構築する
void BuildTriangleIndices(
        const int u_div, const int v_div,
        const std::vector<bool>& is_valid,
        const std::vector<int>& h_cross,
        const std::vector<int>& v_cross,
        std::vector<GLuint>& indices) {
    for (int i = 0; i < u_div; ++i) {
        for (int j = 0; j < v_div; ++j) {
            const int idx_bl = GridIdx(v_div, i, j);
            const int idx_br = GridIdx(v_div, i + 1, j);
            const int idx_tr = GridIdx(v_div, i + 1, j + 1);
            const int idx_tl = GridIdx(v_div, i, j + 1);
            const bool bl = is_valid[idx_bl];
            const bool br = is_valid[idx_br];
            const bool tr = is_valid[idx_tr];
            const bool tl = is_valid[idx_tl];

            if (!bl && !br && !tr && !tl) continue;

            if (bl && br && tr && tl) {
                // 全有効 → 標準2三角形
                indices.push_back(static_cast<GLuint>(idx_bl));
                indices.push_back(static_cast<GLuint>(idx_br));
                indices.push_back(static_cast<GLuint>(idx_tr));
                indices.push_back(static_cast<GLuint>(idx_bl));
                indices.push_back(static_cast<GLuint>(idx_tr));
                indices.push_back(static_cast<GLuint>(idx_tl));
                continue;
            }

            // 境界セル: 有効コーナー + 交差点でCCW多角形を構築してファン三角分割
            const auto poly = BuildCellPolygon(
                    bl, br, tr, tl, idx_bl, idx_br, idx_tr, idx_tl,
                    h_cross[HCrossIdx(v_div, i, j)],
                    v_cross[VCrossIdx(v_div, i + 1, j)],
                    h_cross[HCrossIdx(v_div, i, j + 1)],
                    v_cross[VCrossIdx(v_div, i, j)]);

            for (size_t k = 1; k + 1 < poly.size(); ++k) {
                indices.push_back(static_cast<GLuint>(poly[0]));
                indices.push_back(static_cast<GLuint>(poly[k]));
                indices.push_back(static_cast<GLuint>(poly[k + 1]));
            }
        }
    }
}

}  // namespace



/**
 * constructor / destructor
 */

igesio::graphics::TrimmedSurfaceGraphics::TrimmedSurfaceGraphics(
        const std::shared_ptr<const entities::TrimmedSurface> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

igesio::graphics::TrimmedSurfaceGraphics::~TrimmedSurfaceGraphics() {
    Cleanup();
}



/**
 * protected
 */

void igesio::graphics::TrimmedSurfaceGraphics::DrawImpl(
        GLuint /*shader*/,
        const std::pair<float, float>& /*viewport*/) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_INT, 0);
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

    gl_->BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_->BufferData(GL_ARRAY_BUFFER,
                    vertices_.size() * sizeof(float),
                    vertices_.data(), GL_STATIC_DRAW);
    // 位置 (location=0), 法線 (location=1), テクスチャ座標 (location=2)
    gl_->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                             nullptr);
    gl_->EnableVertexAttribArray(0);
    gl_->VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                             reinterpret_cast<const void*>(3 * sizeof(float)));
    gl_->EnableVertexAttribArray(1);
    gl_->VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                             reinterpret_cast<const void*>(6 * sizeof(float)));
    gl_->EnableVertexAttribArray(2);

    gl_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    gl_->BufferData(GL_ELEMENT_ARRAY_BUFFER,
                    indices_.size() * sizeof(GLuint),
                    indices_.data(), GL_STATIC_DRAW);

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



/**
 * private
 */

void igesio::graphics::TrimmedSurfaceGraphics::GenerateSurfaceData() {
    constexpr int u_div = kDefaultDiv;
    constexpr int v_div = kDefaultDiv;

    const auto range = entity_->GetParameterRange();
    const std::array<double, 2> u_range = {range[0], range[1]};
    const std::array<double, 2> v_range = {range[2], range[3]};

    // 最大頂点数を事前確保: グリッド + 水平エッジ交差 + 垂直エッジ交差
    const int max_verts = (u_div + 1) * (v_div + 1)
                        + u_div * (v_div + 1)
                        + (u_div + 1) * v_div;
    vertices_.resize(8, max_verts);

    // グリッド頂点の構築
    std::vector<bool> is_valid((u_div + 1) * (v_div + 1), false);
    int n_verts = 0;
    BuildGridVertices(*entity_, u_div, v_div, u_range, v_range,
                      is_valid, vertices_, n_verts);

    // エッジ交差頂点の構築
    std::vector<int> h_cross(u_div * (v_div + 1), -1);
    std::vector<int> v_cross((u_div + 1) * v_div, -1);
    BuildHCrossings(*entity_, u_div, v_div, u_range, v_range,
                    is_valid, h_cross, vertices_, n_verts);
    BuildVCrossings(*entity_, u_div, v_div, u_range, v_range,
                    is_valid, v_cross, vertices_, n_verts);

    // 三角形インデックスの構築
    indices_.clear();
    indices_.reserve(u_div * v_div * 6);
    BuildTriangleIndices(u_div, v_div, is_valid, h_cross, v_cross, indices_);

    // 未使用の事前確保分を切り捨てる
    vertices_.conservativeResize(8, n_verts);
}
