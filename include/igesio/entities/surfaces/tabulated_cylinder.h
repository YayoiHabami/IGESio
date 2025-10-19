/**
 * @file entities/surfaces/tabulated_cylinder.h
 * @brief Tabulated Cylinder (Type 122): 平行曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-13
 * @copyright 2025 Yayoi Habami
 * @note 1つの曲線エンティティ（ICurveを継承したクラス）を
 *       ある方向に平行移動させて生成される曲面を表す.
 *       S(u, v) = C(t) + v(L - C(0)), u,v ∈ [0, 1]で表される.
 *       ここで、C(t) = C(a + u(b - a)), t ∈ [a, b]は平行移動させる曲線,
 *       母線はC(0)から位置ベクトルLまでのベクトルを表す.
 */
#ifndef IGESIO_ENTITIES_SURFACES_TABULATED_CYLINDER_H_
#define IGESIO_ENTITIES_SURFACES_TABULATED_CYLINDER_H_

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

/// @brief 平行曲面エンティティ (Entity Type 122)
/// @note 1つの曲線エンティティ（ICurveを継承したクラス）を
///       ある方向に平行移動させて生成される曲面を表す.
///       S(u, v) = C(t) + v(L - C(0)), u,v ∈ [0, 1]で表される.
///       ここで、C(t) = C(a + u(b - a)), t ∈ [a, b]は平行移動させる曲線,
///       母線はC(0)から位置ベクトルLまでのベクトルを表す.
class TabulatedCylinder : public EntityBase, public virtual ISurface {
 private:
    /// @brief 準線 C(t) (ICurveを継承したエンティティへのポインタ)
    PointerContainer<false, ICurve> directrix_;
    /// @brief 母線の位置ベクトル
    /// @note 母線を形成する方向ベクトルは、
    ///       direction = location_vector_ - C(0) で計算される.
    Vector3d location_vector_;



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
    TabulatedCylinder(const RawEntityDE&, const IGESParameterVector&,
                      const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit TabulatedCylinder(const IGESParameterVector&);

    /// @brief コンストラクタ
    /// @param directrix 準線 C(t) (ICurveを継承したエンティティへのポインタ)
    /// @param location_vector 母線の位置ベクトル
    /// @throw std::invalid_argument directrixがnullptrの場合
    TabulatedCylinder(const std::shared_ptr<ICurve>&, const Vector3d&);

    /// @brief コンストラクタ
    /// @param directrix 準線 C(t) (ICurveを継承したエンティティへのポインタ)
    /// @param direction 母線の方向ベクトル (単位ベクトル)
    /// @param length 母線の長さ (directionの大きさ)
    /// @throw std::invalid_argument directrixがnullptrの場合
    /// @throw std::invalid_argument length*directionがゼロベクトルの場合
    TabulatedCylinder(const std::shared_ptr<ICurve>&,
                      const Vector3d&, const double);



    /**
     * 要素の変更・取得
     */

    /// @brief 準線 C(t) を設定する
    /// @param directrix 準線 C(t) (ICurveを継承したエンティティへのポインタ)
    /// @throw std::invalid_argument directrixがnullptrの場合
    void SetDirectrix(const std::shared_ptr<ICurve>&);
    /// @brief 準線 C(t) を取得する
    /// @return 準線 C(t) (ICurveを継承したエンティティへのポインタ)
    /// @throw std::runtime_error 準線が設定されていない場合
    std::shared_ptr<const ICurve> GetDirectrix() const;

    /// @brief 母線の位置ベクトルを設定する
    /// @param location_vector 母線の位置ベクトル
    void SetLocationVector(const Vector3d&);
    /// @brief 母線の位置ベクトルを取得する
    /// @return 母線の位置ベクトル
    Vector3d GetLocationVector() const;
    /// @brief 母線の方向ベクトルを設定する
    /// @param direction 母線の方向ベクトル
    /// @param length 母線の長さ (directionの大きさ)
    /// @throw std::invalid_argument length*directionがゼロベクトルの場合
    /// @note lengthのデフォルト値は1.0であるため、directionに単位ベクトル以外を
    ///       指定した場合は、lengthを省略できる
    void SetDirection(const Vector3d&, const double = 1.0);
    /// @brief 母線の方向ベクトルを取得する
    /// @return 母線の方向ベクトル (非単位ベクトル)
    Vector3d GetDirection() const;




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



 private:
    /// @brief パラメータuから準線C(t)のパラメータtを計算する
    /// @param u パラメータ値 u
    /// @return 準線C(t)のパラメータ値 t
    double GetDirectrixParameterAtU(const double) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_TABULATED_CYLINDER_H_
