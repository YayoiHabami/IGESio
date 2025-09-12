/**
 * @file entities/transformations/transformation_matrix.cpp
 * @brief TransformationMatrix (Type 124): 変換行列エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/transformations/transformation_matrix.h"

#include <memory>
#include <utility>
#include <vector>

#include "igesio/common/tolerance.h"

namespace {

namespace i_ent = igesio::entities;
using TransMatrix = i_ent::TransformationMatrix;
using Vector3d = igesio::Vector3d;
using Matrix3d = igesio::Matrix3d;

}  // namespace



/**
 * コンストラクタ
 */

TransMatrix::TransformationMatrix(const RawEntityDE& de_record,
                                  const IGESParameterVector& parameters,
                                  const pointer2ID& de2id,
                                  const uint64_t iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

TransMatrix::TransformationMatrix(const Matrix3d& rotation, const Vector3d& translation,
                                  const int form_number)
        : TransformationMatrix(
            RawEntityDE::ByDefault(EntityType::kTransformationMatrix, form_number),
            IGESParameterVector{rotation(0, 0), rotation(0, 1), rotation(0, 2),
                                translation[0], rotation(1, 0), rotation(1, 1),
                                rotation(1, 2), translation[1], rotation(2, 0),
                                rotation(2, 1), rotation(2, 2), translation[2]}) {
    // 値の検証
    auto errors = ValidatePD();
    if (!errors.is_valid) {
        throw igesio::DataFormatError("Invalid parameters for TransformationMatrix: " +
                errors.Message());
    }
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector TransMatrix::GetMainPDParameters() const {
    // データをIGESParameterVectorに変換
    IGESParameterVector params{
        rotation_(0, 0), rotation_(0, 1), rotation_(0, 2), translation_[0],
        rotation_(1, 0), rotation_(1, 1), rotation_(1, 2), translation_[1],
        rotation_(2, 0), rotation_(2, 1), rotation_(2, 2), translation_[2]
    };

    // pd_parameters_のフォーマットを適用
    // TransformationMatrixの場合はPD部の要素数は常に同じためそのまま適用
    for (size_t i = 0; i < pd_parameters_.size(); ++i) {
        try {
            params.set_format(i, pd_parameters_.get_format(i));
        } catch (const std::invalid_argument&) {
            // 変換元のフォーマットが正しくない場合は更新しない
        }
    }
    return params;
}

size_t TransMatrix::SetMainPDParameters(const pointer2ID& de2id) {
    // パラメータの数が12であることを確認
    auto& pd = pd_parameters_;
    if (pd.size() != 12) {
        throw igesio::DataFormatError("TransformationMatrix requires exactly 12 parameters");
    }
    auto form_number = GetFormNumber();
    // フォーム番号が0, 1, 10, 11, 12のいずれかであることを確認
    if (form_number != 0 && form_number != 1 &&
        form_number != 10 && form_number != 11 && form_number != 12) {
        throw igesio::DataFormatError("Invalid TransformationMatrix form number."
                                        " Must be 0, 1, 10, 11, or 12.");
    }

    // 回転行列の要素を取得
    rotation_ = {
        {pd.access_as<double>(0), pd.access_as<double>(1), pd.access_as<double>(2)},
        {pd.access_as<double>(4), pd.access_as<double>(5), pd.access_as<double>(6)},
        {pd.access_as<double>(8), pd.access_as<double>(9), pd.access_as<double>(10)}};
    // 平行移動ベクトルを取得
    translation_ =
        {pd.access_as<double>(3), pd.access_as<double>(7), pd.access_as<double>(11)};
    return 12;  // 成功した場合は常に12を返す
}

igesio::ValidationResult TransMatrix::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 回転行列が直交正規であることを確認（全フォーム共通）
    const auto& rot = rotation_;

    // 各列が単位ベクトルであることを確認
    if (!IsApproxOne(rot.col(0).norm())) {
        errors.emplace_back("First column of rotation matrix is not a unit vector.");
    }
    if (!IsApproxOne(rot.col(1).norm())) {
        errors.emplace_back("Second column of rotation matrix is not a unit vector.");
    }
    if (!IsApproxOne(rot.col(2).norm())) {
        errors.emplace_back("Third column of rotation matrix is not a unit vector.");
    }

    // 列同士が直交していることを確認
    if (!IsApproxZero(rot.col(0).dot(rot.col(1)))) {
        errors.emplace_back("Column 1 and column 2 of rotation matrix are not orthogonal.");
    }
    if (!IsApproxZero(rot.col(0).dot(rot.col(2)))) {
        errors.emplace_back("Column 1 and column 3 of rotation matrix are not orthogonal.");
    }
    if (!IsApproxZero(rot.col(1).dot(rot.col(2)))) {
        errors.emplace_back("Column 2 and column 3 of rotation matrix are not orthogonal.");
    }

    // 行列式を計算
    double det = rotation_.determinant();

    // フォーム番号ごとの検証
    switch (form_number_) {
        case 0:  // デフォルトフォーム - 右手系直交正規行列
            if (std::abs(det - 1.0) > kGeometryTolerance) {
                errors.emplace_back("For form 0, determinant of "
                                    "rotation matrix must be +1 (right-handed).");
            }
            break;

        case 1:  // 左手系直交正規行列
            if (std::abs(det + 1.0) > kGeometryTolerance) {
                errors.emplace_back("For form 1, determinant of "
                                    "rotation matrix must be -1 (left-handed).");
            }
            break;

        case 10:  // 直交座標系 - Rは単位行列であるべき
            // 各要素が単位行列と一致するか確認
            if (IsApproxEqual(rot, Matrix3d::Identity(), kGeometryTolerance)) {
                errors.emplace_back("For form 10, rotation matrix must be identity.");
            }
            break;

        case 11:  // 円筒座標系
            // 第3列が[0, 0, 1]であること
            if (IsApproxEqual(rot.col(2), Vector3d::UnitZ(), kGeometryTolerance)) {
                errors.emplace_back("For form 11, third column must be [0, 0, 1].");
            }

            // 最初の2列がxy平面上にあること
            if (IsApproxZero(rot(2, 0)) || IsApproxZero(rot(2, 1))) {
                errors.emplace_back("For form 11, first two columns must be in the xy-plane.");
            }

            // 第1列が[cos(θ), sin(θ), 0]、第2列が[-sin(θ), cos(θ), 0]であること
            if (std::abs(rot(0, 0) - rot(1, 1)) > kGeometryTolerance ||
                std::abs(rot(1, 0) + rot(0, 1)) > kGeometryTolerance) {
                errors.emplace_back("For form 11, rotation matrix "
                                    "must follow cylindrical coordinate structure.");
            }
            break;

        case 12:  // 球座標系
            // 第3列の第3成分が0であること
            if (IsApproxZero(rot(2, 2))) {
                errors.emplace_back("For form 12, third component of third column must be 0.");
            }

            // 球座標系に対する追加チェックは複雑なため省略
            break;

        default:
            errors.emplace_back("Invalid form number for Transformation Matrix.");
            break;
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * ITransformation implementation
 */

Matrix3d TransMatrix::GetRotation() const {
    return rotation_;
}

Vector3d TransMatrix::GetTranslation() const {
    return translation_;
}

igesio::Matrix4d TransMatrix::GetTransformation() const {
    // 同次変換行列を生成
    Matrix4d transformation = Matrix4d::Identity();
    transformation.block<3, 3>(0, 0) = rotation_;
    transformation.block<3, 1>(0, 3) = translation_;
    return transformation;
}

bool TransMatrix::SetReference(
        const std::shared_ptr<ITransformation>& transformation) {
    return SetReference(std::static_pointer_cast<
            const ITransformation>(transformation));
}

bool TransMatrix::SetReference(
        const std::shared_ptr<const ITransformation>& transformation) {
    if (!transformation) {
        // 参照を解除
        de_transformation_matrix_.Reset();
        return true;
    }
    // 循環参照のチェック (子要素を見ていって循環参照がないか確認)
    auto current = de_transformation_matrix_.GetPointer();
    while (current) {
        if (current->GetID() == transformation->GetID()) {
            return false;  // 循環参照がある場合は設定しない
        }
        current = current->GetRefTransformation();
    }
    // 循環参照がない場合は設定
    de_transformation_matrix_.SetPointer(transformation);
    return true;
}

std::shared_ptr<const i_ent::ITransformation>
TransMatrix::GetRefTransformation() const {
    return de_transformation_matrix_.GetPointer();
}



/**
 * TransformationMatrix: 新規実装
 */

i_ent::MatrixType TransMatrix::GetMatrixType() const {
    return static_cast<MatrixType>(form_number_);
}

bool TransMatrix::SetMatrixType(const MatrixType type) {
    if (type == MatrixType::kDefault || type == MatrixType::kLeftHanded ||
        type == MatrixType::kCartesianOffset || type == MatrixType::kCylindricalCoordinates ||
        type == MatrixType::kSphericalCoordinates) {
        form_number_ = static_cast<int>(type);
        return true;
    }
    return false;  // 無効なタイプ
}
