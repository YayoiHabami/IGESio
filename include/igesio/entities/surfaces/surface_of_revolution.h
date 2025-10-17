/**
 * @file entities/surfaces/surface_of_revolution.h
 * @brief Surface of Revolution (Type 120): 回転曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-12
 * @copyright 2025 Yayoi Habami
 * @note 1つのLine (Type 110) エンティティを回転軸として、
 *       任意の曲線エンティティ（ICurveを継承したクラス）を
 *       回転させて生成される曲面を表す。
 */
#ifndef IGESIO_ENTITIES_SURFACES_SURFACE_OF_REVOLUTION_H_
#define IGESIO_ENTITIES_SURFACES_SURFACE_OF_REVOLUTION_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/matrix.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/pointer_container.h"



namespace igesio::entities {

/// @brief 回転曲面エンティティ (Entity Type 120)
/// @note 回転軸として1つのLine (Type 110) エンティティを使用し、
///       任意の曲線エンティティ（ICurveを継承したクラス）を回転させて
///       生成される曲面を表す。S(t,θ) = R(C(t), θ), θ ∈ [θstart, θend]
///       で表される。ここで、C(t)は回転させる曲線、R(P, θ)は点Pを
///       回転軸の周りに角度θだけ回転させた点を表す。
class SurfaceOfRevolution : public EntityBase, public virtual ISurface {
 private:
    /// @brief 回転軸 (Lineエンティティへのポインタ)
    PointerContainer<false, Line> axis_;
    /// @brief 回転させる曲線 (ICurveを継承したエンティティへのポインタ)
    PointerContainer<false, ICurve> generatrix_;
    /// @brief 回転の開始角度 [rad]
    double start_angle_ = 0.0;
    /// @brief 回転の終了角度 [rad]
    double end_angle_ = 0.0;



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
    std::unordered_set<uint64_t> GetUnresolvedPDReferences() const override;

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
    /// @throw std::invalid_argument iges_idがkUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    SurfaceOfRevolution(const RawEntityDE&, const IGESParameterVector&,
                        const pointer2ID& = {}, const uint64_t = kUnsetID);

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがkUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit SurfaceOfRevolution(const IGESParameterVector&);

    /// @brief コンストラクタ
    /// @param axis 回転軸 (Lineエンティティへのポインタ)
    /// @param generatrix 回転させる曲線 (ICurveを継承したエンティティへのポインタ)
    /// @param start_angle 回転の開始角度 [rad]
    /// @param end_angle 回転の終了角度 [rad]
    /// @throw std::invalid_argument axisまたはgeneratrixがnullptrの場合
    SurfaceOfRevolution(const std::shared_ptr<Line>&,
                        const std::shared_ptr<ICurve>&,
                        const double = 0.0, const double = 2.0 * kPi);



    /**
     * 要素の変更・取得
     */

    /// @brief 回転軸を変更する
    /// @param axis 回転軸 (Lineエンティティへのポインタ)
    /// @throw std::invalid_argument axisがnullptrの場合
    void SetAxis(const std::shared_ptr<Line>&);
    /// @brief 回転させる曲線を変更する
    /// @param generatrix 回転させる曲線 (ICurveを継承したエンティティへのポインタ)
    /// @throw std::invalid_argument generatrixがnullptrの場合
    void SetGeneratrix(const std::shared_ptr<ICurve>&);
    /// @brief 回転範囲を変更する
    /// @param start_angle 回転の開始角度 [rad]
    /// @param end_angle 回転の終了角度 [rad]
    void SetAngleRange(const double = 0.0, const double = 2.0 * kPi);

    /// @brief 回転軸を取得する
    /// @return 回転軸 (Lineエンティティへのポインタ)
    /// @throw std::runtime_error 回転軸が未設定の場合、ポインタが未設定の場合
    std::shared_ptr<const Line> GetAxis() const;
    /// @brief 回転させる曲線 (母線) を取得する
    /// @return 回転させる曲線 (ICurveを継承したエンティティへのポインタ)
    /// @throw std::runtime_error 回転させる曲線が未設定の場合、ポインタが未設定の場合
    std::shared_ptr<const ICurve> GetGeneratrix() const;
    /// @brief 回転の開始/終了角度を取得する
    /// @return `{start_angle, end_angle}`の形式で回転の開始/終了角度 [rad] を返す
    std::array<double, 2> GetAngleRange() const {
        return {start_angle_, end_angle_};
    }




    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;

    /// @brief 物理的に従属するエンティティを取得する
    /// @return 物理的に従属するエンティティのID
    std::vector<uint64_t> GetChildIDs() const override;

    /// @brief 物理的に従属するエンティティのポインタを取得する
    /// @param id 物理的に従属するエンティティのID
    /// @return 物理的に従属するエンティティのポインタ
    /// @note 指定されたIDの、物理的に従属するエンティティが存在しない場合は
    ///       nullptrを返す
    std::shared_ptr<const EntityBase> GetChildEntity(const uint64_t) const override;



    /**
     * ISurface implementation
     */

    /// @brief サーフェスがU方向に閉じているかどうか
    /// @return u_minの座標値とu_maxの座標値が一致する場合は`true`、そうでない場合は`false`
    bool IsUClosed() const override;
    /// @brief サーフェスがV方向に閉じているかどうか
    /// @return v_minの座標値とv_maxの座標値が一致する場合は`true`、そうでない場合は`false`
    bool IsVClosed() const override;

    /// @brief サーフェスのパラメータ範囲を取得する
    /// @return `{u_start, u_end, v_start, v_end}`の形式でパラメータ範囲を返す
    /// @note 平面の一つの辺が無限に伸びている場合は、対応するパラメータが
    ///       `std::numeric_limits<double>::infinity()`となる
    std::array<double, 4> GetParameterRange() const override;

    /// @brief 定義空間におけるサーフェス上の点 P(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d>
    TryGetDefinedPointAt(const double, const double) const override;

    /// @brief 定義空間におけるサーフェス上の法線ベクトル N(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d>
    TryGetDefinedNormalAt(const double, const double) const override;

    /// @brief サーフェス上の点 P(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の点の座標値 (x, y, z).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d>
    TryGetPointAt(const double, const double) const override;

    /// @brief サーフェス上の法線ベクトル N(u, v) を取得する
    /// @param u パラメータ値 u
    /// @param v パラメータ値 v
    /// @return サーフェス上の法線ベクトル (nx, ny, nz).
    ///         指定されたパラメータ値がパラメータ範囲外の場合は`std::nullopt`
    std::optional<Vector3d>
    TryGetNormalAt(const double, const double) const override;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_SURFACE_OF_REVOLUTION_H_
