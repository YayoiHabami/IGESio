/**
 * @file entities/surfaces/trimmed_surface.h
 * @brief TrimmedSurface (Type 144): トリム済みパラメトリック曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_SURFACES_TRIMMED_SURFACE_H_
#define IGESIO_ENTITIES_SURFACES_TRIMMED_SURFACE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/interfaces/i_restricted_surface.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/pointer_container.h"



namespace igesio::entities {

/// @brief トリム済みパラメトリック曲面エンティティ (Type 144)
/// @note パラメトリック曲面S(u,v)の定義域を外側境界と内側境界によって制限する。
///       パラメータ化S(u,v)自体は変わらず、有効な定義域のみが変化する。
///       境界曲線はすべてCurveOnAParametricSurface (Type 142) でなければならない。
class TrimmedSurface : public EntityBase, public virtual IRestrictedSurface {
 protected:
    /// @brief トリミング対象の曲面S(u,v). Physically Dependent.
    PointerContainer<false, ISurface> surface_;
    /// @brief 外側境界曲線 (Type 142)。outer_is_boundary_of_d_=trueのとき未使用。
    ///        Physically Dependent。
    PointerContainer<false, CurveOnAParametricSurface> outer_boundary_;
    /// @brief 内側境界曲線リスト (各 Type 142)。各要素は Physically Dependent。
    std::vector<PointerContainer<false, CurveOnAParametricSurface>> inner_boundaries_;

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
    /// @throw igesio::EntityValueError outerの参照曲面がsurfaceと異なる場合
    ///        (曲面参照が未解決の場合はチェック不能として許容)
    TrimmedSurface(const std::shared_ptr<ISurface>&,
                   const std::shared_ptr<CurveOnAParametricSurface>& = nullptr);



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータの有効性を確認する
    /// @note 境界曲線の参照曲面がトリム対象曲面と一致しない場合はkWarningとして
    ///       報告する (読み込みデータに寛容。構築・編集APIは同条件を
    ///       igesio::EntityValueErrorとして強制する)
    ValidationResult ValidatePD() const override;

    /// @brief 物理的に従属するエンティティのIDを取得する
    /// @note surface_、outer_boundary_、inner_boundaries_の全IDを返す
    std::vector<ObjectID> GetChildIDs() const override;

    /// @brief 物理的に従属するエンティティのポインタを取得する
    /// @note surface_、outer_boundary_、inner_boundaries_のいずれかを返す
    std::shared_ptr<const EntityBase> GetChildEntity(
            const ObjectID&) const override;



    /**
     * IRestrictedSurface implementation
     */

    /// @brief トリミング対象の基底曲面S(u,v)を取得する (未解決時nullptr)
    std::shared_ptr<const ISurface> GetBaseSurface() const override;

    /// @brief UV空間における外側境界曲線B(t)を取得する
    /// @return outer_boundary_(Type142)のベース曲線。
    ///         outer_is_boundary_of_d_=trueまたは未解決の場合はnullptr
    std::shared_ptr<const ICurve> GetOuterUVBoundary() const override;

    /// @brief UV空間におけるi番目の内側境界曲線B(t)を取得する
    /// @param index 内側境界のインデックス (0始まり)
    /// @return inner_boundaries_[index](Type142)のベース曲線。未解決時nullptr
    /// @throw std::out_of_range indexが範囲外の場合
    std::shared_ptr<const ICurve>
    GetInnerUVBoundaryAt(std::size_t index) const override;

    /// @brief 領域判定キャッシュを事前構築する (BuildDomainCacheに委譲)
    /// @note Assembly::PrepareGeometryCachesから並列に呼ばれることを想定する
    void PrepareGeometryCache() const override;



    /**
     * 構成要素の操作
     */

    /// @brief トリミング対象の曲面を取得する
    /// @throw igesio::ReferenceError 曲面が未設定の場合
    std::shared_ptr<const ISurface> GetSurface() const;

    /// @brief トリミング対象の曲面を設定する
    /// @throw std::invalid_argument surfaceがnullptrの場合
    /// @throw igesio::EntityValueError 解決済みの既存境界 (外側・内側) のいずれかが
    ///        surfaceと異なる曲面を参照している場合。境界を保持したまま曲面のみを
    ///        差し替えることはできない (境界142自身が曲面参照を持つため)。
    ///        曲面を差し替える場合は先に境界をクリアすること
    /// @note surface_のSubordinateEntitySwitchをkPhysicallyDependentに設定し、
    ///       domain_cache_を無効化する
    void SetSurface(const std::shared_ptr<ISurface>&);

    /// @brief 外側境界曲線を取得する
    /// @return outer_is_boundary_of_d_=trueの場合はnullptr
    std::shared_ptr<const CurveOnAParametricSurface> GetOuterBoundary() const;

    /// @brief 外側境界曲線を設定する
    /// @param outer 外側境界曲線 (Type 142)。
    ///        nullptrの場合はN1=0 (Dの境界を使用) に変更する
    /// @throw igesio::EntityValueError outerの参照曲面がトリム対象曲面と
    ///        異なる場合 (曲面参照が未解決の場合はチェック不能として許容)
    /// @note 非nullの場合: outer_is_boundary_of_d_=false、outerの
    ///       SubordinateEntitySwitchをkPhysicallyDependent に設定、
    ///       domain_cache_ を無効化する
    /// @note nullptrの場合: outer_is_boundary_of_d_=true、outer_boundary_をクリア、
    ///       domain_cache_を無効化する
    void SetOuterBoundary(const std::shared_ptr<CurveOnAParametricSurface>&);

    /// @brief 内側境界曲線の数を取得する
    std::size_t GetInnerBoundaryCount() const noexcept override {
        return inner_boundaries_.size();
    }

    /// @brief 内側境界曲線を取得する
    /// @throw std::out_of_range indexが範囲外の場合
    std::shared_ptr<const CurveOnAParametricSurface>
    GetInnerBoundaryAt(const size_t) const;

    /// @brief 内側境界曲線を追加する
    /// @param boundary 追加する内側境界曲線 (Type 142)
    /// @throw std::invalid_argument boundaryがnullptrの場合
    /// @throw igesio::EntityValueError boundaryの参照曲面がトリム対象曲面と
    ///        異なる場合 (曲面参照が未解決の場合はチェック不能として許容)
    /// @note boundaryのSubordinateEntitySwitchをkPhysicallyDependentに設定し、
    ///       domain_cache_を無効化する
    void AddInnerBoundary(const std::shared_ptr<CurveOnAParametricSurface>&);

    /// @brief 内側境界曲線を取り外す
    /// @param index 削除する内側境界のインデックス (0始まり)
    /// @return 取り外された境界曲線 (未解決参照だった場合はnullptr)
    /// @throw std::out_of_range indexが範囲外の場合
    /// @note 削除後、domain_cache_を無効化する
    /// @note 取り外された境界曲線のSubordinateEntitySwitchは変更しない
    ///       (TrimmedSurface 外から参照されている可能性があるため)
    std::shared_ptr<const CurveOnAParametricSurface> RemoveInnerBoundaryAt(size_t);

    /// @brief 内側境界曲線を置き換える
    /// @param index 置き換える内側境界のインデックス (0始まり)
    /// @param boundary 新しい内側境界曲線 (Type 142)
    /// @return 取り外された境界曲線 (未解決参照だった場合はnullptr)
    /// @throw std::invalid_argument boundaryがnullptrの場合
    /// @throw std::out_of_range indexが範囲外の場合
    /// @throw igesio::EntityValueError boundaryの参照曲面がトリム対象曲面と
    ///        異なる場合 (曲面参照が未解決の場合はチェック不能として許容)
    /// @note 検証がすべて通るまで状態は変更されない。boundaryの
    ///       SubordinateEntitySwitchをkPhysicallyDependentに設定し、
    ///       domain_cache_を無効化する。取り外された境界のスイッチは変更しない
    std::shared_ptr<const CurveOnAParametricSurface> SetInnerBoundaryAt(
            const size_t, const std::shared_ptr<CurveOnAParametricSurface>&);

    /// @brief すべての内側境界曲線を取り外す
    /// @note domain_cache_を無効化する。取り外された境界曲線の
    ///       SubordinateEntitySwitchは変更しない
    void ClearInnerBoundaries();



 protected:
    std::optional<Vector3d> Transform(const std::optional<Vector3d>& input,
                                      const bool is_point) const override {
        return TransformImpl(input, is_point);
    }
};



/**
 * ファクトリ関数
 */

/// @brief トリム済み曲面を作成する
/// @param surface トリミング対象の曲面
/// @param outer 外側境界曲線 (Type 142)。nullptrの場合はN1=0
///        (曲面Dの境界をそのまま外側境界として使用)
/// @param inner_boundaries 内側境界曲線のリスト (各 Type 142)
/// @return 作成されたTrimmedSurfaceのshared_ptr
/// @throw std::invalid_argument surfaceがnullptr、または
///        inner_boundariesにnullptrが含まれる場合
/// @throw igesio::EntityValueError outerまたはinner_boundariesの参照曲面が
///        surfaceと異なる場合 (曲面参照が未解決の境界はチェック不能として許容)
/// @note 検証がすべて通るまでエンティティは構築されない (途中失敗時に
///       入力エンティティへ副作用を残さない)。surfaceおよび各境界の
///       SubordinateEntitySwitchはkPhysicallyDependentに設定される
std::shared_ptr<TrimmedSurface> MakeTrimmedSurface(
        const std::shared_ptr<ISurface>& surface,
        const std::shared_ptr<CurveOnAParametricSurface>& outer = nullptr,
        const std::vector<std::shared_ptr<CurveOnAParametricSurface>>&
                inner_boundaries = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_TRIMMED_SURFACE_H_
