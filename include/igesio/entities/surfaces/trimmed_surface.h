/**
 * @file entities/surfaces/trimmed_surface.h
 * @brief TrimmedSurface (Type 144): トリム済みパラメトリック曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_SURFACES_TRIMMED_SURFACE_H_
#define IGESIO_ENTITIES_SURFACES_TRIMMED_SURFACE_H_

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/pointer_container.h"
#include "igesio/numerics/polygon.h"



namespace igesio::entities {

/// @brief トリム済みパラメトリック曲面エンティティ (Type 144)
/// @note パラメトリック曲面S(u,v)の定義域を外側境界と内側境界によって制限する。
///       パラメータ化S(u,v)自体は変わらず、有効な定義域のみが変化する。
///       境界曲線はすべてCurveOnAParametricSurface (Type 142) でなければならない。
class TrimmedSurface : public EntityBase, public virtual ISurface {
 private:
    /// @brief 内外判定キャッシュ
    struct DomainCache {
        /// @brief 外側境界のUVパラメータ空間における包含多角形 (N1=1の場合のみ有効)
        std::optional<numerics::CurveContainmentPolygons> outer;
        /// @brief 内側境界の包含多角形リスト
        std::vector<numerics::CurveContainmentPolygons> inner;
    };

    /// @brief ComputeContainmentPolygons に渡す初期分割数
    static constexpr int kContainmentPolygonDivisions = 32;

 protected:
    /// @brief トリミング対象の曲面S(u,v). Physically Dependent.
    PointerContainer<false, ISurface> surface_;
    /// @brief 外側境界がパラメータ矩形D全体の境界かどうか (N1=0 → true)
    bool outer_is_boundary_of_d_ = true;
    /// @brief 外側境界曲線 (Type 142)。outer_is_boundary_of_d_=trueのとき未使用。
    ///        Physically Dependent。
    PointerContainer<false, CurveOnAParametricSurface> outer_boundary_;
    /// @brief 内側境界曲線リスト (各 Type 142)。各要素は Physically Dependent。
    std::vector<PointerContainer<false, CurveOnAParametricSurface>> inner_boundaries_;
    /// @brief 内外判定キャッシュ。遅延構築・変更時無効化。
    mutable std::optional<DomainCache> domain_cache_;

    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @return 設定したパラメータの終了インデックス
    size_t SetMainPDParameters(const pointer2ID&) override;

    /// @brief PDレコードの未設定の参照のIDを取得する
    std::unordered_set<ObjectID> GetUnresolvedPDReferences() const override;

    /// @brief PDレコード内の参照先エンティティのポインタを設定する
    /// @note surface_はISurface、境界曲線はCurveOnAParametricSurfaceとして
    ///       dynamic_castで振り分ける
    bool SetUnresolvedPDReferences(
            const std::shared_ptr<const EntityBase>&) override;

 public:
    /// @brief コンストラクタ (IGESファイル読み込み用)
    TrimmedSurface(const RawEntityDE&, const IGESParameterVector&,
                   const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ (直接生成用)
    /// @param surface トリミング対象の曲面
    /// @param outer 外側境界曲線 (Type 142)。nullptrの場合はN1=0 (Dの境界を使用)
    /// @throw std::invalid_argument surfaceがnullptrの場合
    TrimmedSurface(const std::shared_ptr<ISurface>&,
                   const std::shared_ptr<CurveOnAParametricSurface>& = nullptr);



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータの有効性を確認する
    ValidationResult ValidatePD() const override;

    /// @brief 物理的に従属するエンティティのIDを取得する
    /// @note surface_、outer_boundary_、inner_boundaries_の全IDを返す
    std::vector<ObjectID> GetChildIDs() const override;

    /// @brief 物理的に従属するエンティティのポインタを取得する
    /// @note surface_、outer_boundary_、inner_boundaries_のいずれかを返す
    std::shared_ptr<const EntityBase> GetChildEntity(
            const ObjectID&) const override;



    /**
     * ISurface implementation
     */

    /// @brief サーフェスがU方向に閉じているかどうか (surface_に委譲)
    bool IsUClosed() const override;
    /// @brief サーフェスがV方向に閉じているかどうか (surface_に委譲)
    bool IsVClosed() const override;

    /// @brief サーフェスのパラメータ範囲を取得する (surface_に委譲)
    std::array<double, 4> GetParameterRange() const override;

    /// @brief 定義空間におけるサーフェスの偏導関数を計算する
    /// @note トリム領域外の (u, v) に対してはstd::nulloptを返す
    std::optional<SurfaceDerivatives>
    TryGetDerivatives(const double, const double,
                      const unsigned int) const override;

    /// @brief 定義空間における曲面のバウンディングボックスを取得する
    /// @note surface_->GetDefinedBoundingBox()に委譲する (保守的な上界)
    numerics::BoundingBox GetDefinedBoundingBox() const override;



    /**
     * 構成要素の操作
     */

    /// @brief トリミング対象の曲面を取得する
    /// @throw std::runtime_error 曲面が未設定の場合
    std::shared_ptr<const ISurface> GetSurface() const;

    /// @brief トリミング対象の曲面を設定する
    /// @throw std::invalid_argument surfaceがnullptrの場合
    /// @note surface_のSubordinateEntitySwitchをkPhysicallyDependentに設定し、
    ///       domain_cache_を無効化する
    void SetSurface(const std::shared_ptr<ISurface>&);

    /// @brief 外側境界がパラメータ矩形Dの境界かどうかを返す (N1=0 → true)
    bool IsOuterBoundaryOfD() const noexcept { return outer_is_boundary_of_d_; }

    /// @brief 外側境界曲線を取得する
    /// @return outer_is_boundary_of_d_=trueの場合はnullptr
    std::shared_ptr<const CurveOnAParametricSurface> GetOuterBoundary() const;

    /// @brief 外側境界曲線を設定する
    /// @param outer 外側境界曲線 (Type 142)。
    ///        nullptrの場合はN1=0 (Dの境界を使用) に変更する
    /// @note 非nullの場合: outer_is_boundary_of_d_=false、outerの
    ///       SubordinateEntitySwitchをkPhysicallyDependent に設定、
    ///       domain_cache_ を無効化する
    /// @note nullptrの場合: outer_is_boundary_of_d_=true、outer_boundary_をクリア、
    ///       domain_cache_を無効化する
    void SetOuterBoundary(const std::shared_ptr<CurveOnAParametricSurface>&);

    /// @brief 内側境界曲線の数を取得する
    size_t GetInnerBoundaryCount() const noexcept { return inner_boundaries_.size(); }

    /// @brief 内側境界曲線を取得する
    /// @throw std::out_of_range indexが範囲外の場合
    std::shared_ptr<const CurveOnAParametricSurface>
    GetInnerBoundaryAt(const size_t) const;

    /// @brief 内側境界曲線を追加する
    /// @param boundary 追加する内側境界曲線 (Type 142)
    /// @throw std::invalid_argument boundaryがnullptrの場合
    /// @note boundaryのSubordinateEntitySwitchをkPhysicallyDependentに設定し、
    ///       domain_cache_を無効化する
    void AddInnerBoundary(const std::shared_ptr<CurveOnAParametricSurface>&);

    /// @brief 内側境界曲線を削除する
    /// @param index 削除する内側境界のインデックス (0始まり)
    /// @throw std::out_of_range indexが範囲外の場合
    /// @note 削除後、domain_cache_を無効化する
    /// @note 削除された境界曲線のSubordinateEntitySwitchは変更しない
    ///       (TrimmedSurface 外から参照されている可能性があるため)
    void RemoveInnerBoundaryAt(size_t);



 protected:
    std::optional<Vector3d> Transform(const std::optional<Vector3d>& input,
                                      const bool is_point) const override {
        return TransformImpl(input, is_point);
    }



 private:
    /// @brief 包含多角形キャッシュを構築する
    /// @note domain_cache_が有効な場合は何もしない
    void BuildDomainCache() const;

    /// @brief (u, v) がトリム領域内かどうかを判定する
    /// @note キャッシュが未構築の場合はBuildDomainCache()を呼び出す
    bool IsInTrimmedDomain(const double u, const double v) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_TRIMMED_SURFACE_H_
