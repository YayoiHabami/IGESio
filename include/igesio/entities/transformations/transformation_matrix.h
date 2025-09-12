/**
 * @file entities/transformations/transformation_matrix.h
 * @brief TransformationMatrix (Type 124): 変換行列エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_TRANSFORMATIONS_TRANSFORMATION_MATRIX_H_
#define IGESIO_ENTITIES_TRANSFORMATIONS_TRANSFORMATION_MATRIX_H_

#include <memory>

#include "igesio/common/matrix.h"
#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief Transformation Matrix (Type 124) エンティティの種類
/// @note フォーム番号により決まる変換行列の種類
enum class MatrixType {
    /// @brief Default form - Orthonormal matrix with determinant = +1 (right-handed)
    kDefault = 0,
    /// @brief Orthonormal matrix with determinant = -1 (left-handed)
    kLeftHanded = 1,
    /// @brief Finite Element Application - Cartesian coordinate system
    kCartesianOffset = 10,
    /// @brief Finite Element Application - Cylindrical coordinate system
    kCylindricalCoordinates = 11,
    /// @brief Finite Element Application - Spherical coordinate system
    kSphericalCoordinates = 12
};



/// @brief Transformation Matrixエンティティの実装クラス
class TransformationMatrix : public EntityBase, public virtual ITransformation {
 protected:
    /// @brief 3x3の回転行列
    Matrix3d rotation_;
    /// @brief 平行移動ベクトル (x, y, z)
    Vector3d translation_;

    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersの数が12でない場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters([[maybe_unused]] const pointer2ID&) override;

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
    TransformationMatrix(const RawEntityDE&, const IGESParameterVector&,
                         const pointer2ID& = {}, const uint64_t = kUnsetID);

    /// @brief 回転行列と平行移動ベクトルを指定するコンストラクタ
    /// @param rotation 回転行列 (3x3)
    /// @param translation 平行移動ベクトル (x, y, z)
    /// @param form_number 行列の種類を示すフォーム番号
    /// @throw igesio::DataFormatError form_numberと指定された行列/ベクトルが合致しない場合
    TransformationMatrix(const Matrix3d&, const Vector3d& = Vector3d::Zero(),
                         const int = 0);



    /// @brief 行列の種類を取得する
    /// @return MatrixType 列挙型の値
    /// @note form_number_は0, 1, 10, 11, 12のいずれかであることを前提とする
    MatrixType GetMatrixType() const;

    /// @brief 行列の種類を変更する
    /// @param type 新しい行列の種類
    /// @return 変更に成功した場合は`true`、失敗した場合は`false`
    bool SetMatrixType(const MatrixType);



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;



    /**
     * ITransformation implementation
     */

    /// @brief 回転行列を取得する
    Matrix3d GetRotation() const override;
    /// @brief 平行移動ベクトルを取得する
    Vector3d GetTranslation() const override;
    /// @brief 同次変換行列を取得する
    Matrix4d GetTransformation() const override;

    /// @brief 他の変換行列への参照を設定する
    /// @param transformation 参照先の変換行列
    /// @return 参照の設定に成功した場合は`true`、失敗した場合は`false`.
    ///         循環参照の生じる参照の場合は常に`false`を返す.
    bool SetReference(const std::shared_ptr<ITransformation>&) override;

    /// @brief 他の変換行列への参照を設定する
    /// @param transformation 参照先の変換行列
    /// @return 参照の設定に成功した場合は`true`、失敗した場合は`false`.
    bool SetReference(const std::shared_ptr<const ITransformation>&) override;

    /// @brief 他の変換行列への参照を取得する
    /// @return 参照先の変換行列. 参照が設定されていない場合は`nullptr`を返す.
    std::shared_ptr<const ITransformation> GetRefTransformation() const override;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_TRANSFORMATIONS_TRANSFORMATION_MATRIX_H_
