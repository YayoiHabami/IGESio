/**
 * @file entities/surfaces/plane.h
 * @brief Plane (Type 108): 平面エンティティの定義
 * @author Yayoi Habami
 * @date 2026-06-09
 * @copyright 2026 Yayoi Habami
 * @note Form 0 は無限平面 (Plane, ISurface)、Form 1/-1は有界平面
 *       (BoundedPlane, IRestrictedSurface) として扱う。平面は
 *       A·X+B·Y+C·Z=D で定義され、(A,B,C) の少なくとも1つは非ゼロ。
 */
#ifndef IGESIO_ENTITIES_SURFACES_PLANE_H_
#define IGESIO_ENTITIES_SURFACES_PLANE_H_

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/interfaces/i_restricted_surface.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/pointer_container.h"



namespace igesio::entities {

/// @brief 平面の定義空間フレーム (originと正規直交基底e_u, e_v, n̂)
/// @note S(u, v) = origin + u·e_u + v·e_v。係数 (A, B, C, D) から一意に定める
struct PlaneFrame {
    /// @brief 原点 (定義空間原点からの垂線の足 (D/|n|²)·(A,B,C))
    Vector3d origin;
    /// @brief U方向単位ベクトル (n̂に直交)
    Vector3d e_u;
    /// @brief V方向単位ベクトル (= n̂ × e_u)
    Vector3d e_v;
    /// @brief 単位法線 n̂ = (A,B,C)/|(A,B,C)|
    Vector3d normal;
};

/// @brief 係数 (A,B,C,D) から平面フレームを構築する
/// @param a,b,c,d 平面係数
/// @return 平面フレーム。e_u×e_v = n̂ となる右手系
/// @throw igesio::EntityValueError (a,b,c) がすべてゼロの場合
PlaneFrame MakePlaneFrame(double a, double b, double c, double d);



/// @brief 無限平面エンティティ (Type 108, Form 0)
/// @note A·X+B·Y+C·Z=Dで定義される、定義域が全平面のアフィン曲面。
///       パラメータ範囲は両方向とも (-∞, +∞)。X,Y,Z,SIZEは表示シンボル
///       専用パラメータで、幾何には影響しない (保持・往復のみ)。
class Plane : public EntityBase, public virtual ISurface {
 private:
    /// @brief 平面係数A, B, C, D
    std::array<double, 4> coefficients_ = {0.0, 0.0, 1.0, 0.0};
    /// @brief 表示シンボルの位置点 (X_T, Y_T, Z_T)。既定0
    Vector3d symbol_location_ = Vector3d::Zero();
    /// @brief 表示シンボルのサイズ。0 または既定で「表示シンボルなし」
    double symbol_size_ = 0.0;

 protected:
    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @return 設定した主パラメータの終了インデックス
    /// @throw igesio::EntityParameterError parametersの数が不足する場合
    size_t SetMainPDParameters(const pointer2ID&) override;

 public:
    /// @brief コンストラクタ (IGESファイル読み込み用)
    Plane(const RawEntityDE&, const IGESParameterVector&,
          const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ (直接生成用; 表示シンボルなし)
    /// @param a,b,c,d 平面係数
    /// @throw igesio::EntityValueError (a,b,c) がすべてゼロの場合
    Plane(double a, double b, double c, double d);



    /**
     * 平面パラメータの取得
     */

    /// @brief 平面係数 {A, B, C, D} を取得する
    const std::array<double, 4>& GetCoefficients() const { return coefficients_; }
    /// @brief 定義空間フレームを取得する
    /// @throw igesio::EntityValueError (A,B,C) がすべてゼロの場合
    PlaneFrame GetFrame() const;
    /// @brief 表示シンボルの位置点を取得する (表示専用)
    Vector3d GetSymbolLocation() const { return symbol_location_; }
    /// @brief 表示シンボルのサイズを取得する (0 = シンボルなし)
    double GetSymbolSize() const { return symbol_size_; }



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    /// @note (A,B,C) が非ゼロであること、Form 0であることを確認する
    ValidationResult ValidatePD() const override;



    /**
     * ISurface implementation
     */

    /// @brief U方向に閉じているか (常にfalse)
    bool IsUClosed() const override { return false; }
    /// @brief V方向に閉じているか (常にfalse)
    bool IsVClosed() const override { return false; }

    /// @brief パラメータ範囲 {-∞, +∞, -∞, +∞} を返す
    std::array<double, 4> GetParameterRange() const override;

    /// @brief 定義空間における偏導関数 S^(i,j)(u, v) を計算する
    /// @note S = origin + u·e_u + v·e_v, Su = e_u, Sv = e_v, 2階以上 = 0
    std::optional<SurfaceDerivatives>
    TryGetDefinedDerivatives(const double, const double,
                             const unsigned int) const override;

    /// @brief 定義空間のバウンディングボックスを取得する
    /// @note 無限平面のため、面内2方向 (e_u, e_v) は無限直線 (kLine)、
    ///       法線方向のサイズは0とする (厚みゼロの2次元無限スラブ)
    numerics::BoundingBox GetDefinedBoundingBox() const override;



 protected:
    std::optional<Vector3d> Transform(const std::optional<Vector3d>& input,
                                      const bool is_point) const override {
        return TransformImpl(input, is_point);
    }
};



/// @brief 有界平面エンティティ (Type 108, Form 1 / -1)
/// @note 平面S(u,v)の定義域を、平面上にある単純閉曲線 (PTR) で制限する。
///       Form 1は正領域、Form -1は負領域 (穴) を表すが、幾何は同一で
///       符号 (sense) のみが異なる。穴は持たない (内側境界数は常に0)。
/// @note 基底曲面は同じ係数から構築した定義空間の Plane (恒等変換)。自身の
///       変換行列MはTransformImplで後掛けされる (二重適用しない)。境界曲線は
///       モデル空間で平面に乗るため、UV境界はモデル空間フレームへの内積射影で構成し、
///       S(B(t)) = C(t)を厳密に満たす。
class BoundedPlane : public EntityBase, public virtual IRestrictedSurface {
 private:
    /// @brief 平面係数 A, B, C, D
    std::array<double, 4> coefficients_ = {0.0, 0.0, 1.0, 0.0};
    /// @brief 表示シンボルの位置点。既定0
    Vector3d symbol_location_ = Vector3d::Zero();
    /// @brief 表示シンボルのサイズ。既定0
    double symbol_size_ = 0.0;
    /// @brief 境界閉曲線 (PTR)。Physically Dependent。
    PointerContainer<false, ICurve> boundary_;

    /// @brief 基底曲面 (定義空間Plane)。遅延構築・キャッシュ
    mutable std::shared_ptr<Plane> base_surface_;
    /// @brief UV空間の外側境界曲線 (閉ポリライン)。遅延構築・キャッシュ
    mutable std::shared_ptr<const ICurve> uv_boundary_;
    /// @brief 有界領域の定義空間バウンディングボックス。遅延構築・キャッシュ
    mutable std::optional<numerics::BoundingBox> defined_bbox_;

    /// @brief UV境界・定義空間bboxのキャッシュを構築する (未構築かつ境界解決済み時のみ)
    /// @note 境界曲線をモデル空間で離散化し、モデル空間フレームへ内積射影して
    ///       (u, v) 化、閉LinearPathと定義空間点群bboxを生成する。境界が
    ///       未解決・退化の場合は何もしない (グレースフル劣化)。
    void EnsureBoundaryCache() const;

    /// @brief 基底曲面・UV境界・定義空間bbox・ドメイン判定キャッシュを無効化する
    void InvalidateGeometryCaches() const noexcept;

 protected:
    IGESParameterVector GetMainPDParameters() const override;
    size_t SetMainPDParameters(const pointer2ID&) override;
    std::unordered_set<ObjectID> GetUnresolvedPDReferences() const override;
    bool SetUnresolvedPDReferences(
            const std::shared_ptr<const EntityBase>&) override;

 public:
    /// @brief コンストラクタ (IGESファイル読み込み用)
    BoundedPlane(const RawEntityDE&, const IGESParameterVector&,
                 const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ (直接生成用)
    /// @param a,b,c,d 平面係数
    /// @param boundary 平面上の単純閉曲線 (PTR)
    /// @param negative trueでForm -1 (穴/負)、falseでForm 1 (正)
    /// @throw std::invalid_argument boundaryがnullptrの場合
    /// @throw igesio::EntityValueError (a,b,c) がすべてゼロの場合
    BoundedPlane(double a, double b, double c, double d,
                 const std::shared_ptr<ICurve>& boundary,
                 bool negative = false);



    /**
     * 平面パラメータ・境界の取得/設定
     */

    /// @brief 平面係数 {A, B, C, D} を取得する
    const std::array<double, 4>& GetCoefficients() const { return coefficients_; }
    /// @brief 定義空間フレームを取得する
    /// @throw igesio::EntityValueError (A,B,C) がすべてゼロの場合
    PlaneFrame GetFrame() const;
    /// @brief 表示シンボルの位置点を取得する (表示専用)
    Vector3d GetSymbolLocation() const { return symbol_location_; }
    /// @brief 表示シンボルのサイズを取得する (0 = シンボルなし)
    double GetSymbolSize() const { return symbol_size_; }

    /// @brief 負領域 (Form -1, 穴) かどうか
    bool IsNegative() const { return form_number_ < 0; }

    /// @brief 境界閉曲線を取得する
    /// @throw igesio::ReferenceError 境界が未設定の場合
    std::shared_ptr<const ICurve> GetBoundaryCurve() const;
    /// @brief 境界閉曲線を設定する
    /// @param boundary 平面上の単純閉曲線 (PTR)
    /// @throw std::invalid_argument boundaryがnullptrの場合
    /// @note boundaryをkPhysicallyDependentに設定し、キャッシュを無効化する
    void SetBoundaryCurve(const std::shared_ptr<ICurve>&);



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    /// @note (A,B,C) 非ゼロ・Form ∈ {1,-1}・境界設定済みを確認する。
    ///       境界が閉曲線でない場合は kWarning として報告する
    ValidationResult ValidatePD() const override;
    /// @brief 物理的に従属するエンティティのIDを取得する (境界曲線)
    std::vector<ObjectID> GetChildIDs() const override;
    /// @brief 物理的に従属するエンティティのポインタを取得する
    std::shared_ptr<const EntityBase> GetChildEntity(const ObjectID&) const override;
    /// @brief 領域判定キャッシュを事前構築する (BuildDomainCache へ委譲)
    void PrepareGeometryCache() const override;



    /**
     * IRestrictedSurface implementation
     */

    /// @brief 基底曲面 (定義空間 Plane) を取得する。遅延構築
    /// @return 基底曲面。係数が退化している場合は nullptr
    std::shared_ptr<const ISurface> GetBaseSurface() const override;
    /// @brief UV空間の外側境界曲線 (閉ポリライン) を取得する。遅延構築
    /// @return UV境界曲線。境界未解決・退化の場合は nullptr
    std::shared_ptr<const ICurve> GetOuterUVBoundary() const override;
    /// @brief 内側境界 (穴) の数。常に 0
    std::size_t GetInnerBoundaryCount() const override { return 0; }
    /// @brief i番目の内側境界。常に範囲外
    /// @throw std::out_of_range 常に送出 (内側境界を持たないため)
    std::shared_ptr<const ICurve>
    GetInnerUVBoundaryAt(std::size_t) const override;

    /// @brief 定義空間のバウンディングボックス (有界領域)
    /// @note UV境界サンプルを定義空間へ写した点群の軸平行bbox。境界未解決時は
    ///       基底曲面 (無限平面) のbboxにフォールバックする
    numerics::BoundingBox GetDefinedBoundingBox() const override;



 protected:
    std::optional<Vector3d> Transform(const std::optional<Vector3d>& input,
                                      const bool is_point) const override {
        return TransformImpl(input, is_point);
    }
};



/**
 * ファクトリ関数
 */

/// @brief 無限平面 (Form 0) を作成する
/// @param a,b,c,d 平面係数
/// @throw igesio::EntityValueError (a,b,c) がすべてゼロの場合
std::shared_ptr<Plane> MakePlane(double a, double b, double c, double d);

/// @brief 法線と通過点から無限平面 (Form 0) を作成する
/// @param normal 平面の法線 (非ゼロ)
/// @param point 平面が通る点
/// @throw igesio::EntityValueError normal がゼロベクトルの場合
std::shared_ptr<Plane> MakePlane(const Vector3d& normal, const Vector3d& point);

/// @brief 有界平面 (Form 1 / -1) を作成する
/// @param a,b,c,d 平面係数
/// @param boundary 平面上の単純閉曲線
/// @param negative true で Form -1、false で Form 1
/// @throw std::invalid_argument boundary が nullptr の場合
/// @throw igesio::EntityValueError (a,b,c) がすべてゼロの場合
std::shared_ptr<BoundedPlane> MakeBoundedPlane(
        double a, double b, double c, double d,
        const std::shared_ptr<ICurve>& boundary, bool negative = false);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_PLANE_H_
