/**
 * @file graphics/surfaces/surface_mesh.cpp
 * @brief 汎用曲面 (掃引系含む) の描画用三角形メッシュを生成する
 * @author Yayoi Habami
 * @date 2026-06-08
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/surfaces/surface_mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "igesio/graphics/core/i_entity_graphics.h"  // kInfiniteParamClamp



namespace igesio::graphics {

namespace {

using igesio::MatrixXf;
using igesio::Vector3d;
using igesio::entities::ISurface;

/// @brief u/vパラメータの値を計算する
/// @param i インデックス
/// @param div 分割数
/// @param range パラメータ範囲 {start, end}
double ComputeParam(const int i, const int div, const std::array<double, 2>& range) {
    return range[0] + (range[1] - range[0]) * i / static_cast<double>(div);
}

/// @brief uサンプル行の記述子
/// @note 折れ目では同一uに法線評価位置の異なる2行を生成し、稜線で
///       法線が補間されないようにする (ハードエッジ化)
struct USampleRow {
    /// @brief 頂点位置を評価するパラメータu
    double u = 0.0;
    /// @brief 法線を評価するパラメータu (折れ目では片側へεずらす)
    double normal_u = 0.0;
    /// @brief 次行との間に三角形ストリップを張るか
    /// @note 折れ目で二重化した同一u行ペアの間は退化するためfalse
    bool bridge_to_next = true;
};

/// @brief 一様グリッドと折れ目を統合したuサンプル行を構築する
/// @param entity 対象の曲面
/// @param u_div u方向の一様分割数
/// @return uサンプル行のリスト (u昇順)
std::vector<USampleRow> BuildUSampleRows(const ISurface& entity,
                                         const std::array<double, 2>& u_range,
                                         const int u_div) {
    const double umin = u_range[0], umax = u_range[1];

    // 無限・退化範囲は従来通り一様サンプルのみ (折れ目挿入は行わない)
    std::vector<USampleRow> rows;
    if (!std::isfinite(umin) || !std::isfinite(umax) || umax <= umin) {
        for (int i = 0; i <= u_div; ++i) {
            const double u = ComputeParam(i, u_div, u_range);
            rows.push_back({u, u, true});
        }
        return rows;
    }

    const double span = umax - umin;
    const double merge_tol = span * 1e-6;

    // 内部折れ目 (開区間内) を収集・ソート・近接重複除去
    std::vector<double> creases;
    for (const double uc : entity.GetUCreaseParameters()) {
        if (uc > umin + merge_tol && uc < umax - merge_tol) creases.push_back(uc);
    }
    std::sort(creases.begin(), creases.end());
    creases.erase(std::unique(creases.begin(), creases.end(),
            [merge_tol](double a, double b) { return std::abs(a - b) <= merge_tol; }),
            creases.end());

    // 一様グリッド点 ∪ 折れ目のステーションを構築 (折れ目近傍の格子点は吸収)
    struct Station {
        double u;
        bool is_crease;
    };
    std::vector<Station> stations;
    for (int i = 0; i <= u_div; ++i) {
        const double u = umin + span * i / static_cast<double>(u_div);
        bool near_crease = false;
        for (const double uc : creases) {
            if (std::abs(u - uc) <= merge_tol) { near_crease = true; break; }
        }
        if (!near_crease) stations.push_back({u, false});
    }
    for (const double uc : creases) stations.push_back({uc, true});
    std::sort(stations.begin(), stations.end(),
              [](const Station& a, const Station& b) { return a.u < b.u; });

    // ステーションから行を生成する (折れ目は法線片側評価で二重化)
    for (size_t k = 0; k < stations.size(); ++k) {
        if (!stations[k].is_crease) {
            rows.push_back({stations[k].u, stations[k].u, true});
            continue;
        }
        // 近接ステーションとの間隔から片側評価のオフセットεを決める
        const double u = stations[k].u;
        double gap = span;
        if (k > 0) gap = std::min(gap, u - stations[k - 1].u);
        if (k + 1 < stations.size()) gap = std::min(gap, stations[k + 1].u - u);
        const double eps = gap * 0.01;
        rows.push_back({u, u - eps, false});  // 下側ストリップ用 (左側法線)
        rows.push_back({u, u + eps, true});   // 上側ストリップ用 (右側法線)
    }
    return rows;
}

/// @brief 退化点(点は有効だが法線が取得できない)の法線を隣接頂点から補完する
/// @param vertices 頂点・法線データ (8行N列; 行3-5が法線)
/// @param valid 各頂点の有効フラグ (点が取得できたか)
/// @param has_normal 各頂点の法線取得フラグ
/// @param n_rows uサンプル行数
/// @param v_div v方向分割数
/// @note apex(母線末端が軸上)等で法線が定義できない頂点の穴抜けを防ぐ
void FillMissingNormals(MatrixXf& vertices, const MatrixXf& valid,
                        const MatrixXf& has_normal, const int n_rows,
                        const int v_div) {
    const int stride = v_div + 1;
    const std::array<std::pair<int, int>, 4> offsets = {{
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
    for (int i = 0; i < n_rows; ++i) {
        for (int j = 0; j <= v_div; ++j) {
            if (valid(i, j) < 0.5f || has_normal(i, j) > 0.5f) continue;
            // 隣接(同列の前後行→同行の前後列)から有効法線を借用する
            for (const auto& [di, dj] : offsets) {
                const int ni = i + di, nj = j + dj;
                if (ni < 0 || ni >= n_rows || nj < 0 || nj > v_div) continue;
                if (has_normal(ni, nj) < 0.5f) continue;
                const int dst = i * stride + j, src = ni * stride + nj;
                vertices(3, dst) = vertices(3, src);
                vertices(4, dst) = vertices(4, src);
                vertices(5, dst) = vertices(5, src);
                break;
            }
        }
    }
}

/// @brief uサンプル行と頂点有効フラグから三角形インデックスを生成する
/// @param rows uサンプル行
/// @param valid 各頂点の有効フラグ (点が取得できたか)
/// @param v_div v方向分割数
/// @param indices 生成したインデックスの格納先 (追記する)
void BuildSurfaceIndices(const std::vector<USampleRow>& rows,
                         const MatrixXf& valid, const int v_div,
                         std::vector<gl::Uint>& indices) {
    const int stride = v_div + 1;
    auto idx = [stride](int i, int j) {
        return static_cast<gl::Uint>(i * stride + j);
    };
    const int n_rows = static_cast<int>(rows.size());
    // 全頂点が有効 or 左上/右下の三角形が有効なら左上/右下の三角形を描画
    // 1つ以上の頂点が無効 and 左下/右上の三角形が有効なら左下/右上の三角形を描画
    for (int i = 0; i + 1 < n_rows; ++i) {
        // 折れ目で二重化した同一u行ペアの間は退化するため張らない
        if (!rows[i].bridge_to_next) continue;
        for (int j = 0; j < v_div; ++j) {
            const bool ij = valid(i, j) > 0.5f;
            const bool i1j = valid(i + 1, j) > 0.5f;
            const bool ij1 = valid(i, j + 1) > 0.5f;
            const bool i1j1 = valid(i + 1, j + 1) > 0.5f;
            const bool all = ij && i1j && ij1 && i1j1;

            if (all || (ij && i1j && ij1)) {
                // 左上の三角形
                indices.push_back(idx(i, j));
                indices.push_back(idx(i + 1, j));
                indices.push_back(idx(i, j + 1));
            }
            if (all || (i1j && i1j1 && ij1)) {
                // 右下の三角形
                indices.push_back(idx(i + 1, j));
                indices.push_back(idx(i + 1, j + 1));
                indices.push_back(idx(i, j + 1));
            }
            if (!all && (ij && i1j1 && ij1)) {
                // 左下の三角形
                indices.push_back(idx(i, j));
                indices.push_back(idx(i + 1, j + 1));
                indices.push_back(idx(i, j + 1));
            }
            if (!all && (ij && i1j && i1j1)) {
                // 右上の三角形
                indices.push_back(idx(i, j));
                indices.push_back(idx(i + 1, j));
                indices.push_back(idx(i + 1, j + 1));
            }
        }
    }
}

}  // namespace



GeneralSurfaceMesh BuildGeneralSurfaceMesh(
        const entities::ISurface& surface, const int u_div, const int v_div) {
    GeneralSurfaceMesh mesh;
    mesh.v_div = v_div;

    auto u_range = surface.GetURange();
    auto v_range = surface.GetVRange();
    // 無限範囲はエッジ生成 (ComputeParametricSurfaceEdges) と同じ±kInfiniteParamClamp
    // へクランプし、有限な可視メッシュを生成する (有限範囲はそのまま素通り)
    if (std::isinf(u_range[0])) u_range[0] = -kInfiniteParamClamp;
    if (std::isinf(u_range[1])) u_range[1] =  kInfiniteParamClamp;
    if (std::isinf(v_range[0])) v_range[0] = -kInfiniteParamClamp;
    if (std::isinf(v_range[1])) v_range[1] =  kInfiniteParamClamp;

    // uサンプル行 (折れ目二重化済み) を構築する
    const auto rows = BuildUSampleRows(surface, u_range, u_div);
    const int n_rows = static_cast<int>(rows.size());
    mesh.u_row_count = n_rows;

    // 頂点・法線データとインデックスデータの初期化
    mesh.vertices.resize(8, n_rows * (v_div + 1));
    mesh.indices.reserve(n_rows * v_div * 6);
    // 各頂点が有効か (点が取得できたか) と、法線が取得できたか
    // NOTE: IGESioのMatrixがbool型をサポートしていないため、float型で代用
    MatrixXf is_vertex_valid(n_rows, v_div + 1);
    MatrixXf has_normal(n_rows, v_div + 1);

    const double u_span = u_range[1] - u_range[0];
    const double v_span = v_range[1] - v_range[0];

    // 頂点・法線データを生成
    for (int i = 0; i < n_rows; ++i) {
        for (int j = 0; j <= v_div; ++j) {
            const double v = ComputeParam(j, v_div, v_range);
            auto pos_opt = surface.TryGetPointAt(rows[i].u, v);
            // 法線は折れ目対応のため片側評価のnormal_uで取得する
            auto normal_opt = surface.TryGetNormalAt(rows[i].normal_u, v);

            Vector3d pos = Vector3d::Zero();
            Vector3d normal = Vector3d::UnitZ();
            is_vertex_valid(i, j) = 0.0f;
            has_normal(i, j) = 0.0f;
            if (pos_opt) {
                // 点が有効なら頂点を採用する (法線の有無では落とさない)
                pos = *pos_opt;
                is_vertex_valid(i, j) = 1.0f;
                if (normal_opt) {
                    normal = *normal_opt;
                    has_normal(i, j) = 1.0f;
                }
            }
            const int col = i * (v_div + 1) + j;
            mesh.vertices.block<3, 1>(0, col) = pos.cast<float>();
            mesh.vertices.block<3, 1>(3, col) = normal.cast<float>();
            // 0-1に正規化した(u, v)をテクスチャ座標として設定
            mesh.vertices(6, col) = (u_span != 0.0)
                    ? static_cast<float>((rows[i].u - u_range[0]) / u_span) : 0.0f;
            mesh.vertices(7, col) = (v_span != 0.0)
                    ? static_cast<float>((v - v_range[0]) / v_span) : 0.0f;
        }
    }

    // 退化点 (apex等) の法線を隣接頂点から補完し、穴の発生を防ぐ
    FillMissingNormals(mesh.vertices, is_vertex_valid, has_normal, n_rows, v_div);

    // インデックスデータを生成
    BuildSurfaceIndices(rows, is_vertex_valid, v_div, mesh.indices);
    return mesh;
}

}  // namespace igesio::graphics
