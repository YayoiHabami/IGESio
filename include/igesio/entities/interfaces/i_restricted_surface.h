/**
 * @file entities/interfaces/i_restricted_surface.h
 * @brief パラメータ範囲の一部が領域外となりうる曲面のインターフェース定義
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_INTERFACES_I_RESTRICTED_SURFACE_H_
#define IGESIO_ENTITIES_INTERFACES_I_RESTRICTED_SURFACE_H_

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "igesio/numerics/polygon.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"



namespace igesio::entities {

/// @brief パラメータ範囲の一部が領域外となりうる曲面の共通インターフェース
/// @note 基底曲面S(u,v)の矩形ドメインDを、UV空間の外側境界ループと任意個の
///       内側境界ループ(穴)で制限する曲面 (Type 143/144等) を抽象化する。
///       境界曲線はUV空間のICurveとして扱い、一切の具象エンティティ型に依存しない。
/// @note 具象クラスにおいては、以下を実装・遵守すること:
///       - `GetBaseSurface` / `GetOuterUVBoundary` / `GetInnerBoundaryCount`
///         / `GetInnerUVBoundaryAt` (純粋仮想)
///       - `outer_is_boundary_of_d_` を構築・編集時に設定する
///       - 境界編集 (Set/Add/Remove系) で `InvalidateDomainCache()` を呼ぶ
///       - `EntityBase::PrepareGeometryCache` をオーバーライドし
///         `BuildDomainCache()` へ委譲する
class IRestrictedSurface : public virtual ISurface {
 private:
    /// @brief 内外判定キャッシュ
    struct DomainCache {
        /// @brief 外側境界のUVパラメータ空間における包含多角形
        /// @note outer_is_boundary_of_d_=false かつ構築成功時のみ有効
        std::optional<numerics::CurveContainmentPolygons> outer;
        /// @brief 内側境界(穴)の包含多角形リスト
        std::vector<numerics::CurveContainmentPolygons> inner;
    };

 protected:
    /// @brief ComputeContainmentPolygonsに渡す初期分割数
    static constexpr int kContainmentPolygonDivisions = 32;

    /// @brief 外側境界がパラメータ矩形D全体の境界かどうか (制限なし → true)
    /// @note 具象クラスが構築・編集時に設定する (144ではN1フラグに対応)
    bool outer_is_boundary_of_d_ = true;
    /// @brief 内外判定キャッシュ。遅延構築・変更時無効化。
    mutable std::optional<DomainCache> domain_cache_;

    /// @brief 包含多角形キャッシュを構築する
    /// @note domain_cache_が有効な場合は何もしない。同一インスタンスに対して同時に
    ///       呼び出してはならない (内部のdomain_cache_を非同期に書き込むため)。
    ///       境界曲線が未解決/退化/非閉でテッセレーションが例外を投げても、当該境界を
    ///       スキップして処理全体を止めない (グレースフル劣化)。
    void BuildDomainCache() const;

    /// @brief 領域判定キャッシュを無効化する
    /// @note 具象クラスの境界編集 (Set/Add/Remove系) から呼ぶこと
    void InvalidateDomainCache() const noexcept { domain_cache_ = std::nullopt; }

 public:
    /// @brief デストラクタ
    ~IRestrictedSurface() override = default;



    /**
     * 具象クラスが実装するフック (具象エンティティ型に依存しない供給口)
     */

    /// @brief トリミング対象の基底曲面S(u,v)を取得する
    /// @return 基底曲面。未解決の場合はnullptr
    virtual std::shared_ptr<const ISurface> GetBaseSurface() const = 0;

    /// @brief UV空間における外側境界曲線B(t)を取得する
    /// @return 外側境界曲線 (ICurve)。outer_is_boundary_of_d_=trueまたは
    ///         未解決の場合はnullptr
    /// @note 144はCurveOnAParametricSurfaceのBaseCurveを、143はパラメータ空間で
    ///       再構築した曲線(折れ線等)を返すことを想定する
    virtual std::shared_ptr<const ICurve> GetOuterUVBoundary() const = 0;

    /// @brief 内側境界(穴)の数を取得する
    /// @note 高速パス・ループ境界に使用するため、安価に取得できること
    virtual std::size_t GetInnerBoundaryCount() const = 0;

    /// @brief UV空間におけるi番目の内側境界曲線B(t)を取得する
    /// @param index 内側境界のインデックス (0始まり)
    /// @return 内側境界曲線 (ICurve)。未解決の場合はnullptr
    /// @throw std::out_of_range indexが範囲外の場合
    virtual std::shared_ptr<const ICurve>
    GetInnerUVBoundaryAt(std::size_t index) const = 0;



    /**
     * ISurface実装 (基底曲面への委譲＋ドメイン制限)
     */

    /// @brief サーフェスがU方向に閉じているかどうか
    /// @note 外側境界が明示指定 (outer_is_boundary_of_d_=false) の場合は保守的に
    ///       falseを返す。そうでなければ基底曲面に委譲する。
    bool IsUClosed() const override;
    /// @brief サーフェスがV方向に閉じているかどうか
    /// @note 外側境界が明示指定 (outer_is_boundary_of_d_=false) の場合は保守的に
    ///       falseを返す。そうでなければ基底曲面に委譲する。
    bool IsVClosed() const override;

    /// @brief サーフェスのパラメータ範囲を取得する (基底曲面に委譲)
    std::array<double, 4> GetParameterRange() const override;

    /// @brief 定義空間におけるサーフェスの偏導関数を計算する
    /// @note トリム領域外の (u, v) に対してはstd::nulloptを返す。領域内では
    ///       基底曲面のモデル空間 (M_base適用済み) の偏導関数を返す。
    std::optional<SurfaceDerivatives>
    TryGetDefinedDerivatives(const double, const double,
                             const unsigned int) const override;

    /// @brief 定義空間における曲面のバウンディングボックスを取得する
    /// @note 基底曲面に委譲する (保守的な上界)
    numerics::BoundingBox GetDefinedBoundingBox() const override;

    /// @brief (u, v) がトリム後の有効なドメイン内かどうかを判定する
    /// @note キャッシュが未構築の場合はBuildDomainCache()を呼び出す
    bool IsInDomain(const double u, const double v) const override;



    /**
     * 領域情報のアクセサ
     */

    /// @brief 外側境界がパラメータ矩形Dの境界かどうかを返す (制限なし → true)
    bool IsOuterBoundaryOfD() const noexcept { return outer_is_boundary_of_d_; }

    /// @brief トリム領域の外側包含多角形を取得する (テッセレーション用)
    /// @return 外側境界の包含多角形。outer_is_boundary_of_d_=trueまたは構築失敗時は
    ///         std::nullopt。キャッシュ未構築の場合は構築する。
    const std::optional<numerics::CurveContainmentPolygons>&
    GetOuterDomainPolygon() const;
    /// @brief トリム領域の内側包含多角形(穴)を取得する (テッセレーション用)
    /// @return 内側境界(穴)の包含多角形リスト。キャッシュ未構築の場合は構築する。
    const std::vector<numerics::CurveContainmentPolygons>&
    GetInnerDomainPolygons() const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_RESTRICTED_SURFACE_H_
