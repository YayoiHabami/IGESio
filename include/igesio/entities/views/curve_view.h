/**
 * @file entities/views/curve_view.h
 * @brief 曲線エンティティの変換ビュー(CurveView)
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_VIEWS_CURVE_VIEW_H_
#define IGESIO_ENTITIES_VIEWS_CURVE_VIEW_H_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

/// @brief 曲線エンティティの変換ビュー(Decoratorパターン)
/// @note 元の`ICurve`を書き換えずに、基準階層までのAssembly変換`placement_`を後掛けして
///       座標を返す読み取り専用ビュー. 同一エンティティを複数フレームから同時に別座標で
///       取得するために用いる. `EntityBase`は継承せず`ICurve`のみを継承する.
/// @note 固有の`ObjectID`(kEntityView)を持つが、型・フォーム番号は元エンティティへ委譲する.
///       IGESファイルへは出力されない.
/// @note 座標系の意味は以下の通り. ビューにおける「定義空間」は元エンティティのM_entityを
///       適用した後の座標(所有Assemblyのローカルフレーム)を指すことに注意する.
///       - `TryGetDefined*`: 元エンティティのM_entity適用後の座標
///       - `TryGet*`: `placement_`適用後の座標. すなわち`placement_·(M_entity·P_def)`
/// @note スナップショット意味論とする. 生成時点の累積変換を固定して保持し、後でAssembly変換を
///       変えても追従しない. 読み取り専用であり形状編集APIを持たない.
/// @note 固有IDの二重解放を避けるため、コピー不可・ムーブ可とする.
class CurveView : public ICurve {
    /// @brief 元曲線(共有・不変). `TryGet*`はM_entity適用済みの値を返す
    std::shared_ptr<const ICurve> base_;
    /// @brief 基準階層までのAssembly変換の積. M_entityは含まない
    Matrix4d placement_;
    /// @brief ビュー固有のID(kEntityView). 型・フォームは元へ委譲する
    ObjectID id_;

 public:
    /// @brief コンストラクタ
    /// @param base 元曲線(nullptr不可)
    /// @param placement 基準階層までの累積変換(既定: 単位行列)
    /// @throw std::invalid_argument baseがnullptrの場合
    /// @note baseが`CurveView`の場合は変換を畳み込み、元の`ICurve`へ繋ぎ直す
    explicit CurveView(std::shared_ptr<const ICurve> base,
                       const Matrix4d& placement = Matrix4d::Identity());

    /// @brief デストラクタ
    /// @note 固有のint型IDを解放する
    ~CurveView() override;

    /// @brief コピーコンストラクタ(削除)
    CurveView(const CurveView&) = delete;
    /// @brief コピー代入演算子(削除)
    CurveView& operator=(const CurveView&) = delete;
    /// @brief ムーブコンストラクタ
    CurveView(CurveView&&) noexcept = default;
    /// @brief ムーブ代入演算子
    CurveView& operator=(CurveView&&) noexcept = default;



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
    /// @brief ジオメトリリビジョンを取得する(元へ委譲)
    /// @note 転送しないと元エンティティの形状編集がビュー越しに検知されない
    uint64_t GeometryRevision() const override {
        return base_->GeometryRevision();
    }



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
     * ICurve
     */

    /// @brief 曲線が閉じているかどうか(元へ委譲)
    bool IsClosed() const override { return base_->IsClosed(); }
    /// @brief パラメータ範囲を取得する(元へ委譲)
    std::array<double, 2> GetParameterRange() const override {
        return base_->GetParameterRange();
    }
    /// @brief 基底のモデル空間版/配置適用版`TryGetDerivatives`を可視化する
    /// @note 継承された非virtualの`TryGetDerivatives(t, n)`および配置適用版
    ///       `TryGetDerivatives(t, n, placement)`を具象型のまま呼べるよう
    ///       `using`で明示的に再公開する
    using ICurve::TryGetDerivatives;
    /// @brief ビューの定義空間における導関数を取得する
    /// @param t パラメータ値
    /// @param n 何階まで計算するか
    /// @return 元エンティティのM_entity適用後(モデル空間)の導関数
    /// @note 継承された`TryGet*`系はこれに`Transform`(=placement_)を適用するため、
    ///       `placement_·(M_entity·C_def)`が得られる
    std::optional<CurveDerivatives>
    TryGetDefinedDerivatives(const double t, const unsigned int n) const override {
        return base_->TryGetDerivatives(t, n, Matrix4d::Identity());
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

#endif  // IGESIO_ENTITIES_VIEWS_CURVE_VIEW_H_
