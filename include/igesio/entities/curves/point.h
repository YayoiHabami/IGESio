/**
 * @file entities/curves/point.h
 * @brief Point (Type 116): 点エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-15
 * @copyright 2025 Yayoi Habami
 * @note Curve and Surface Geometry entities (see Section 3.2) のうちの
 *       一つであるため、curveモジュールの一つとして定義した.
 */
#ifndef IGESIO_ENTITIES_CURVES_POINT_H_
#define IGESIO_ENTITIES_CURVES_POINT_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_subfigure_definition.h"
#include "igesio/entities/interfaces/i_geometry.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/pointer_container.h"



namespace igesio::entities {

/// @brief 点エンティティ (Entity Type 116)
class Point : public EntityBase, public virtual IGeometry {
    /// @brief 点の位置ベクトル
    Vector3d position_ = {0.0, 0.0, 0.0};

    /// @brief 点の描画用サブフィギュア
    PointerContainer<false, ISubfigureDefinition> subfigure_;



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
    Point(const RawEntityDE&, const IGESParameterVector&,
          const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief コンストラクタ
    /// @param parameters PDレコードのパラメータ
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    explicit Point(const IGESParameterVector&);

    /// @brief コンストラクタ
    /// @param position 点の位置ベクトル
    explicit Point(const Vector3d&);



    /**
     * 要素の変更・取得
     */

    /// @brief (定義空間における) 点の位置ベクトルを設定する
    /// @param position 点の位置ベクトル
    void SetDefinedPosition(const Vector3d& position) { position_ = position; }
    /// @brief (定義空間における) 点の位置ベクトルを取得する
    /// @return 点の位置ベクトル
    const Vector3d& GetDefinedPosition() const { return position_; }

    /// @brief 点の描画用サブフィギュアを設定する
    /// @param subfigure 点の描画用サブフィギュア
    void SetSubfigure(const std::shared_ptr<ISubfigureDefinition>&);
    /// @brief 点の描画用サブフィギュアを取得する
    /// @return 点の描画用サブフィギュア
    std::shared_ptr<const ISubfigureDefinition> GetSubfigure() const;

    /// @brief (親の空間における) 点の位置ベクトルを取得する
    /// @return 点の位置ベクトル
    Vector3d GetPosition() const { return Transform(position_, true).value(); }



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
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_POINT_H_
