/**
 * @file entities/surfaces/ruled_surface.h
 * @brief Ruled Surface (Type 118): ルールド曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-14
 * @copyright 2025 Yayoi Habami
 * @note 2つの曲線エンティティ（ICurveを継承したクラス）C1(t), C2(s)を
 *      直線で結んで生成される曲面を表す。S(u,v) = (1-v)C1(t) + vC2(s),
 *      u,v ∈ [0,1]で表される。ここで、t ∈ [tmin, tmax], s ∈ [smin, smax]は
 *      それぞれC1, C2のパラメータ範囲である。t = tmin + u(tmax - tmin) であり、
 *      sはDIRFLGが0の場合は s = smin + u(smax - smin)、1の場合は
 *      s = smax - u(smax - smin) である。
 */
#ifndef IGESIO_ENTITIES_SURFACES_RULED_SURFACE_H_
#define IGESIO_ENTITIES_SURFACES_RULED_SURFACE_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/matrix.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/pointer_container.h"



namespace igesio::entities {

/// @brief ルールド曲面エンティティ (Entity Type 118)
/// @note 2つの曲線エンティティ（ICurveを継承したクラス）C1(t), C2(s)を
///       直線で結んで生成される曲面を表す。S(u,v) = (1-v)C1(t) + vC2(s),
///       u,v ∈ [0,1]で表される。ここで、t ∈ [tmin, tmax], s ∈ [smin, smax]は
///       それぞれC1, C2のパラメータ範囲である。t = tmin + u(tmax - tmin) であり、
///       sはDIRFLGが0の場合は s = smin + u(smax - smin)、1の場合は
///       s = smax - u(smax - smin) である。
class RuledSurface : public EntityBase, public virtual ISurface {
 private:
    /// @brief 1つ目の曲線 C1(t) (ICurveを継承したエンティティへのポインタ)
    PointerContainer<false, ICurve> curve1_;
    /// @brief 2つ目の曲線 C2(s) (ICurveを継承したエンティティへのポインタ)
    PointerContainer<false, ICurve> curve2_;
    /// @brief 2つの曲線のパラメータ範囲を反転させるか (DIRFLG)
    /// @note falseの場合、s = smin + u(smax - smin)、trueの場合、
    ///       s = smax - u(smax - smin) である. tについては常に
    ///       t = tmin + u(tmax - tmin) である.
    bool is_reversed_ = false;
    /// @brief 可展面（developable surface）かどうか (DEVFLG)
    bool is_developable_ = false;



 protected:
    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersの数が不正な場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters(const pointer2ID& de2id) override;

    /// @brief PDレコードの未設定の参照のIDを取得する
    /// @return ポインタが未設定のエンティティのIDのリスト
    /// @note 追加ポインタは除く (EntityBase側で取得するため)
    std::unordered_set<ObjectID> GetUnresolvedPDReferences() const override;

    /// @brief PDレコード内の参照先エンティティのポインタを設定する
    /// @param entity ポインタを設定するエンティティ
    /// @return 指定されたエンティティと同一のIDを持つ参照がない場合は`false`を返す
    /// @note ポインタが設定済みの場合、上書きは行わない
    /// @note 追加ポインタは除く (EntityBase側で設定するため)
    bool SetUnresolvedPDReferences(const std::shared_ptr<const EntityBase>&) override;



 public:
    /// @brief コンストラクタ
    /// @param de_record DEレコードのパラメータ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    RuledSurface(const RawEntityDE&, const IGESParameterVector&,
                 const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit RuledSurface(const IGESParameterVector&);

    /// @brief コンストラクタ
    /// @param curve1 1つ目の曲線 C1(t) (ICurveを継承したエンティティへのポインタ)
    /// @param curve2 2つ目の曲線 C2(s) (ICurveを継承したエンティティへのポインタ)
    /// @param is_reversed 2つの曲線のパラメータ範囲を反転させるか (DIRFLG)
    /// @param is_developable 可展面（developable surface）かどうか (DEVFLG)
    /// @throw std::invalid_argument curve1またはcurve2がnullptrの場合
    RuledSurface(const std::shared_ptr<ICurve>&, const std::shared_ptr<ICurve>&,
                 const bool = false, const bool = false);



    /**
     * 要素の変更・取得
     */

    /// @brief 1つ目の曲線 C1(t) を取得する
    /// @return ICurveを継承したエンティティへのポインタ
    std::shared_ptr<const ICurve> GetCurve1() const;
    /// @brief 1つ目の曲線 C1(t) を設定する
    /// @param curve ICurveを継承したエンティティへのポインタ
    /// @throw std::invalid_argument curveがnullptrの場合
    void SetCurve1(const std::shared_ptr<ICurve>&);
    /// @brief 2つ目の曲線 C2(s) を取得する
    /// @return ICurveを継承したエンティティへのポインタ
    std::shared_ptr<const ICurve> GetCurve2() const;
    /// @brief 2つ目の曲線 C2(s) を設定する
    /// @param curve ICurveを継承したエンティティへのポインタ
    /// @throw std::invalid_argument curveがnullptrの場合
    void SetCurve2(const std::shared_ptr<ICurve>&);

    /// @brief 2つの曲線のパラメータ範囲を反転させるか (DIRFLG) を取得する
    /// @return falseの場合、s = smin + u(smax - smin)、trueの場合、
    ///         s = smax - u(smax - smin) である. tについては常に
    ///         t = tmin + u(tmax - tmin) である.
    bool IsReversed() const noexcept { return is_reversed_; }
    /// @brief 2つの曲線のパラメータ範囲を
    ///        反転させるか (DIRFLG) を設定する
    /// @param is_reversed falseの場合、s = smin + u(smax - smin)、trueの場合、
    ///                    s = smax - u(smax - smin) である. tについては常に
    ///                    t = tmin + u(tmax - tmin) である.
    void SetIsReversed(const bool is_reversed) noexcept { is_reversed_ = is_reversed; }

    /// @brief 可展面（developable surface）かどうか (DEVFLG) を取得する
    /// @return 可展面の場合はtrue、そうでない場合はfalse
    bool IsDevelopable() const noexcept { return is_developable_; }
    /// @brief 可展面（developable surface）かどうか (DEVFLG) を設定する
    /// @param is_developable 可展面の場合はtrue、そうでない場合はfalse
    void SetIsDevelopable(const bool is_developable) noexcept {
        is_developable_ = is_developable;
    }




    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;

    /// @brief 物理的に従属するエンティティを取得する
    /// @return 物理的に従属するエンティティのID
    std::vector<ObjectID> GetChildIDs() const override;

    /// @brief 物理的に従属するエンティティのポインタを取得する
    /// @param id 物理的に従属するエンティティのID
    /// @return 物理的に従属するエンティティのポインタ
    /// @note 指定されたIDの、物理的に従属するエンティティが存在しない場合は
    ///       nullptrを返す
    std::shared_ptr<const EntityBase> GetChildEntity(const ObjectID&) const override;



    /**
     * ISurface implementation
     */

    /// @brief サーフェスがU方向に閉じているかどうか
    /// @return u_minの座標値とu_maxの座標値が一致する場合は`true`、そうでない場合は`false`
    bool IsUClosed() const override { return false; }
    /// @brief サーフェスがV方向に閉じているかどうか
    /// @return v_minの座標値とv_maxの座標値が一致する場合は`true`、そうでない場合は`false`
    bool IsVClosed() const override { return false; }

    /// @brief サーフェスのパラメータ範囲を取得する
    /// @return `{u_start, u_end, v_start, v_end}`の形式でパラメータ範囲を返す
    /// @note 平面の一つの辺が無限に伸びている場合は、対応するパラメータが
    ///       `std::numeric_limits<double>::infinity()`となる
    std::array<double, 4> GetParameterRange() const override {
        return {0.0, 1.0, 0.0, 1.0};
    }

    /// @brief 定義空間におけるサーフェスの偏導関数 S^(i,j)(u, v) を計算する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @param order 何階まで計算するか; 例えば2を指定した場合、0階 S(u, v) から
    ///              2階 S^(2,0)(u, v), S^(1,1)(u, v), S^(0,2)(u, v) まで計算
    /// @return 偏導関数 S, Su, Sv, ...、計算できない場合は`std::nullopt`
    std::optional<SurfaceDerivatives>
    TryGetDerivatives(const double, const double, const unsigned int) const override;



 protected:
    /// @brief エンティティ自身が参照する変換行列に従い、座標orベクトルを変換する
    /// @param input 変換前の座標orベクトル v
    /// @param is_point 座標を変換する場合は`true`、ベクトルを変換する場合は`false`
    /// @return 変換後の座標orベクトル. 回転行列 R、平行移動ベクトル T に対し、
    ///         座標値の場合は v' = Rv + T、ベクトルの場合は v' = Rv
    /// @note inputがstd::nulloptの場合はそのまま返す
    ///       としてオーバライドすること
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input, const bool is_point) const override {
        return TransformImpl(input, is_point);
    }



 private:
    /// @brief 曲面C1(t), C2(s)のパラメータs,tを取得する
    /// @param u パラメータ値 u
    /// @return {t, s}の形式でパラメータ値を返す
    /// @note 正しい値が取得できない場合は{0.0, 0.0}を返す
    std::pair<double, double> GetParametersTS(const double) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_RULED_SURFACE_H_
