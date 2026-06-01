/**
 * @file entities/views/surface_view.h
 * @brief 曲面エンティティの変換ビュー(SurfaceView)
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_VIEWS_SURFACE_VIEW_H_
#define IGESIO_ENTITIES_VIEWS_SURFACE_VIEW_H_

#include <array>
#include <memory>
#include <optional>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/matrix.h"
#include "igesio/numerics/bounding_box.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/interfaces/i_surface.h"



namespace igesio::entities {

/// @brief 曲面エンティティの変換ビュー(Decoratorパターン)
/// @note 元の`ISurface`を書き換えずに、基準階層までのAssembly変換`placement_`を後掛けして
///       座標を返す読み取り専用ビュー. 同一エンティティを複数フレームから同時に別座標で
///       取得するために用いる. `EntityBase`は継承せず`ISurface`のみを継承する.
/// @note 固有の`ObjectID`(kEntityView)を持つが、型・フォーム番号は元エンティティへ委譲する.
///       IGESファイルへは出力されない.
/// @note 座標系の意味は以下の通り. ビューにおける「定義空間」は元エンティティのM_entityを
///       適用した後の座標(所有Assemblyのローカルフレーム)を指すことに注意する.
///       - `TryGetDefined*`: 元エンティティのM_entity適用後の座標
///       - `TryGet*`: `placement_`適用後の座標. すなわち`placement_·(M_entity·P_def)`
/// @note スナップショット意味論とする. 生成時点の累積変換を固定して保持し、後でAssembly変換を
///       変えても追従しない. 読み取り専用であり形状編集APIを持たない.
/// @note 固有IDの二重解放を避けるため、コピー不可・ムーブ可とする.
class SurfaceView : public ISurface {
    /// @brief 元曲面(共有・不変). `TryGet*`はM_entity適用済みの値を返す
    std::shared_ptr<const ISurface> base_;
    /// @brief 基準階層までのAssembly変換の積. M_entityは含まない
    Matrix4d placement_;
    /// @brief ビュー固有のID(kEntityView). 型・フォームは元へ委譲する
    ObjectID id_;

 public:
    /// @brief コンストラクタ
    /// @param base 元曲面(nullptr不可)
    /// @param placement 基準階層までの累積変換(既定: 単位行列)
    /// @throw std::invalid_argument baseがnullptrの場合
    /// @note baseが`SurfaceView`の場合は変換を畳み込み、元の`ISurface`へ繋ぎ直す
    explicit SurfaceView(std::shared_ptr<const ISurface> base,
                         const Matrix4d& placement = Matrix4d::Identity());

    /// @brief デストラクタ
    /// @note 固有のint型IDを解放する
    ~SurfaceView() override;

    /// @brief コピーコンストラクタ(削除)
    SurfaceView(const SurfaceView&) = delete;
    /// @brief コピー代入演算子(削除)
    SurfaceView& operator=(const SurfaceView&) = delete;
    /// @brief ムーブコンストラクタ
    SurfaceView(SurfaceView&&) noexcept = default;
    /// @brief ムーブ代入演算子
    SurfaceView& operator=(SurfaceView&&) noexcept = default;



    /**
     * IEntityIdentifier (委譲)
     */

    /// @brief ビュー固有のIDを取得する
    const ObjectID& GetID() const override { return id_; }
    /// @brief 元エンティティのIDを取得する
    const ObjectID& GetSourceID() const override { return base_->GetID(); }
    /// @brief エンティティタイプを取得する(元へ委譲)
    EntityType GetType() const override { return base_->GetType(); }
    /// @brief フォーム番号を取得する(元へ委譲)
    int GetFormNumber() const override { return base_->GetFormNumber(); }
    /// @brief サポート対象かどうかを取得する(元へ委譲)
    bool IsSupported() const override { return base_->IsSupported(); }



    /**
     * IGeometry
     */

    /// @brief 幾何形状の次元数を取得する(元へ委譲)
    unsigned int NDim() const noexcept override { return base_->NDim(); }
    /// @brief ビューの定義空間におけるバウンディングボックスを取得する
    /// @return 元エンティティのM_entity適用後のバウンディングボックス
    /// @note 継承された`GetBoundingBox()`はこれに`Transform`(=placement_)を適用するため、
    ///       `placement_·M_entity·BB_def`が得られ、二重適用にはならない
    numerics::BoundingBox GetDefinedBoundingBox() const override {
        return base_->GetBoundingBox();
    }



    /**
     * ISurface
     */

    /// @brief U方向に閉じているかどうか(元へ委譲)
    bool IsUClosed() const override { return base_->IsUClosed(); }
    /// @brief V方向に閉じているかどうか(元へ委譲)
    bool IsVClosed() const override { return base_->IsVClosed(); }
    /// @brief パラメータ範囲を取得する(元へ委譲)
    std::array<double, 4> GetParameterRange() const override {
        return base_->GetParameterRange();
    }
    /// @brief 基底のモデル空間版/配置適用版`TryGetDerivatives`を可視化する
    /// @note 継承された非virtualの`TryGetDerivatives(u, v, order)`および配置適用版
    ///       `TryGetDerivatives(u, v, order, placement)`を具象型のまま呼べるよう
    ///       `using`で明示的に再公開する
    using ISurface::TryGetDerivatives;
    /// @brief ビューの定義空間における偏導関数を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @param order 何階まで計算するか
    /// @return 元エンティティのM_entity適用後(モデル空間)の偏導関数
    /// @note 継承された`TryGet*`系はこれに`Transform`(=placement_)を適用するため、
    ///       `placement_·(M_entity·S_def)`が得られる
    std::optional<SurfaceDerivatives>
    TryGetDefinedDerivatives(const double u, const double v,
                             const unsigned int order) const override {
        return base_->TryGetDerivatives(u, v, order, Matrix4d::Identity());
    }

 protected:
    /// @brief placement_に従い座標orベクトルを変換する
    /// @param input 変換前の座標orベクトル
    /// @param is_point 座標の場合true、ベクトルの場合false
    /// @return placement_適用後の値. 点はR·v+T、ベクトルはR·v
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>&, const bool) const override;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_VIEWS_SURFACE_VIEW_H_
