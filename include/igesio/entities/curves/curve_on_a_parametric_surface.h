/**
 * @file entities/curves/curve_on_a_parametric_surface.h
 * @brief CurveOnAParametricSurface (Type 142): パラメトリックサーフェス上の曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-11-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_H_
#define IGESIO_ENTITIES_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/pointer_container.h"



namespace igesio::entities {

/// @brief 曲面上の曲線がどのように作成されたかを示す
enum class CurveCreationType : int {
    /// @brief 未指定
    kUnspecified = 0,
    /// @brief 与えられた曲線の曲面上への射影
    kProjection = 1,
    /// @brief 2つの曲面の交線
    kIntersection = 2,
    /// @brief 曲面のアイソパラメトリック曲線
    /// @note u-パラメトリック曲線またはv-パラメトリック曲線
    kIsoparametric = 3
};

/// @brief 送信システムにおける優先表現を示す
enum class PreferredRepresentation : int {
    /// @brief 未指定
    kUnspecified = 0,
    /// @brief S(B(t)) が優先
    kSofB = 1,
    /// @brief C(t) が優先
    kC = 2,
    /// @brief C(t) と S(B(t)) を同等に優先
    kEquallyPreferred = 3
};

/// @brief パラメトリック曲面 S(u,v) 上の曲線 C(t) を表すエンティティクラス
/// @note 長方形領域 D = {(u,v) | u_min <= u <= u_max, v_min <= v <= v_max} 上で
///       定義されたパラメトリック曲面 S(u,v) と、その定義域内で定義された
///       曲線 B(t) = (u(t), v(t)) によって定義される曲線 C(t) = S(u(t), v(t)) を表す.
class CurveOnAParametricSurface : public EntityBase, public virtual ICurve3D {
 protected:
    /// @brief 曲面 S(u,v)
    PointerContainer<false, ISurface> surface_;
    /// @brief 曲線 B(t) = (u(t), v(t))
    PointerContainer<false, ICurve> base_curve_;
    /// @brief 曲線 C(t) = S(u(t), v(t))
    PointerContainer<false, ICurve> curve_;

    /// @brief 曲面上の曲線の生成方法
    CurveCreationType creation_type_;
    /// @brief 送信システムにおける優先表現
    PreferredRepresentation preferred_representation_;


    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersの数が不整合な場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters(const pointer2ID&) override;

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
    CurveOnAParametricSurface(
            const RawEntityDE&, const IGESParameterVector&,
            const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param surface 曲面 S(u,v)
    /// @param base_curve 曲線 B(t)
    /// @param curve 曲線 C(t)
    /// @throw std::invalid_argument surface, base_curve, curveのいずれかがnullptrの場合
    CurveOnAParametricSurface(const std::shared_ptr<ISurface>&,
            const std::shared_ptr<ICurve>&, const std::shared_ptr<ICurve>&);



    /**
     * EntityBase implementation
     */

    /// @brief 物理的に従属するエンティティを取得する
    /// @return 物理的に従属するエンティティのID
    std::vector<ObjectID> GetChildIDs() const override;

    /// @brief 物理的に従属するエンティティのポインタを取得する
    /// @param id 物理的に従属するエンティティのID
    /// @return 物理的に従属するエンティティのポインタ
    /// @note 指定されたIDの、物理的に従属するエンティティが存在しない場合は
    ///       nullptrを返す
    std::shared_ptr<const EntityBase> GetChildEntity(const ObjectID&) const override;

    /// @brief PDレコードのパラメータの有効性を確認する
    /// @return 全パラメータが適合しているか否か
    /// @note 参照するエンティティの有効性も確認する
    ValidationResult ValidatePD() const override;



    /**
     * ICurve implementation
     */

    /// @brief 曲線が閉じているかどうか
    /// @return 始点と終点が一致する場合は`true`、そうでない場合は`false`
    /// @note 最初の曲線の始点と、最後の曲線の終点を比較して閉じているかを判断する
    bool IsClosed() const override;

    /// @brief 曲線のパラメータ範囲を取得する
    /// @return `{t_start, t_end}`の形式でパラメータ範囲を返す
    /// @note 複合曲線のパラメータ範囲は、各構成曲線のパラメータ長`t_end - t_start`
    ///       を累積して再計算される. 開始パラメータは常に0、終端パラメータは
    ///       全エンティティのパラメータ長の合計値となる
    /// @note B(t)が設定されていない場合などは {0, 0} を返す
    std::array<double, 2> GetParameterRange() const override;

    /// @brief 定義空間における曲線のn階導関数 C^n(t) を計算する
    /// @param t パラメータ値
    /// @param n 何階まで計算するか; 例えば2を指定した場合、0階 C(t) から2階 C''(t) まで計算
    /// @return 導関数 C'(t), C''(t)、計算できない場合は`std::nullopt`
    std::optional<CurveDerivatives>
    TryGetDerivatives(const double, const unsigned int) const override;

    /// @brief 曲線の [t_start, t_end] 間の長さを取得する
    /// @param start パラメータ範囲の開始値
    /// @param end パラメータ範囲の終了値
    /// @return 曲線の t ∈ [start, end] 間の長さ
    /// @throw std::invalid_argument start >= endの場合、
    ///        startまたはendがパラメータ範囲外の場合
    double Length(const double, const double) const override;

    /// @brief 定義空間における曲線のバウンディングボックスを取得する
    numerics::BoundingBox GetDefinedBoundingBox() const override;



    /**
     * 構成要素の操作
     */

    /// @brief 曲面 S(u,v) を取得する
    /// @throw std::runtime_error 曲面が未設定の場合、ポインタが未設定の場合
    std::shared_ptr<const ISurface> GetSurface() const;
    /// @brief ベース曲線 B(t) を取得する
    /// @throw std::runtime_error 曲線が未設定の場合、ポインタが未設定の場合
    std::shared_ptr<const ICurve> GetBaseCurve() const;
    /// @brief 曲線 C(t) を取得する
    /// @throw std::runtime_error 曲線が未設定の場合、ポインタが未設定の場合
    std::shared_ptr<const ICurve> GetCurve() const;

    /// @brief 曲面 S(u,v) を設定する
    /// @param surface 曲面 S(u,v)
    /// @throw std::invalid_argument surfaceがnullptrの場合
    void SetSurface(const std::shared_ptr<ISurface>&);
    /// @brief ベース曲線 B(t) (および C(t)) を設定する
    /// @param base_curve ベース曲線 B(t)
    /// @param curve 曲線 C(t)、指定しない場合は自動生成される
    /// @return 自動生成された曲線 C(t) のポインタ、curveが指定された場合はnullptrを返す
    /// @throw std::invalid_argument base_curveがnullptrの場合、
    ///        curveがnullptrかつsurfaceがnullptrの場合
    /// @throw std::runtime_error curveがnullptrかつ自動生成に失敗した場合
    std::shared_ptr<ICurve> SetCurves(const std::shared_ptr<ICurve>&,
                                      const std::shared_ptr<ICurve>& = nullptr);

    /// @brief 曲面上の曲線 C(t) の作成方法
    CurveCreationType GetCreationType() const { return creation_type_; }
    /// @brief 送信システムにおける優先表現
    PreferredRepresentation GetPreferredRepresentation() const {
        return preferred_representation_;
    }
    /// @brief 送信システムにおける優先表現を設定する
    /// @param pref 送信システムにおける優先表現
    void SetPreferredRepresentation(const PreferredRepresentation pref) {
        preferred_representation_ = pref;
    }



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

    /// @brief base_curveを設定する
    /// @param base_curve ベース曲線 B(t)
    /// @return 成功した場合は`true`を返す
    /// @throw std::invalid_argument base_curveがnullptrの場合、
    ///        S(u,v)の定義領域D内で定義されていない場合
    bool SetBaseCurve(const std::shared_ptr<const ICurve>& base_curve);
};

/// @brief CurveOnAParametricSurfaceのエイリアス
using CurveOnSurface = CurveOnAParametricSurface;

/// @brief CurveOnAParametricSurfaceを作成する
/// @param surface 曲面 S(u,v)
/// @param base_curve 曲線 B(t)
/// @return 作成されたCurveOnAParametricSurfaceのshared_ptrと
///         曲線 C(t) のshared_ptrのペア
/// @throw std::invalid_argument surface, base_curveのいずれかがnullptrの場合
/// @throw std::runtime_error 曲線 C(t) の自動生成に失敗した場合
std::pair<std::shared_ptr<CurveOnAParametricSurface>, std::shared_ptr<ICurve>>
MakeCurveOnAParametricSurface(const std::shared_ptr<ISurface>&,
                              const std::shared_ptr<ICurve>&);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_H_
