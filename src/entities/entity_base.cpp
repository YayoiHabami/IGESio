/**
 * @file entities/entity_base.cpp
 * @brief 個別エンティティクラスの基底クラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/entity_base.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

namespace i_ent = igesio::entities;
using EntityBase = i_ent::EntityBase;

using Vector3d = igesio::Vector3d;

}  // namespace



/**
 * privateメンバ関数の実装
 */

void EntityBase::SetDERecord(const RawEntityDE& de_record, const pointer2ID& de2id) {
    try {
        // 3rd parameter (Structure)
        de_structure_ = CreateDEFieldWrapper<DEStructure>(
            de_record.structure, de2id);
        // 4th parameter (Line Font Pattern)
        de_line_font_pattern_ = CreateDEFieldWrapper<DELineFontPattern>(
            de_record.line_font_pattern, de2id);
        // 5th parameter (Level)
        de_level_ = CreateDEFieldWrapper<DELevel>(de_record.level, de2id);
        // 6th parameter (View)
        de_view_ = CreateDEFieldWrapper<DEView>(de_record.view, de2id);
        // 7th parameter (Transformation Matrix)
        de_transformation_matrix_ = CreateDEFieldWrapper<DETransformationMatrix>(
            de_record.transformation_matrix, de2id);
        // 8th parameter (Label Display Associativity)
        de_label_display_associativity_ = CreateDEFieldWrapper<DELabelDisplayAssociativity>(
                de_record.label_display_associativity, de2id);
        // 13th parameter (Color Number)
        de_color_ = CreateDEFieldWrapper<DEColor>(de_record.color_number, de2id);
    } catch (const std::out_of_range& e) {
        // de_record側で指定されているDEポインターが、de2idで指定されていない場合
        throw std::out_of_range(
            "DE pointer not found in ID mapping: "
            + std::to_string(de_record.transformation_matrix));
    }

    // 9th parameter (Status Number)
    de_status_ = de_record.status;
    // 12th parameter (Line Weight)
    de_line_weight_ = de_record.line_weight_number;
    // 18th parameter (Entity Label)
    de_entity_label_ = de_record.entity_label;
    // 19th parameter (Entity Subscript Number)
    de_entity_subscript_number_ = de_record.entity_subscript_number;
}



/**
 * protectedメンバ関数の実装
 */

void EntityBase::SetAdditionalPointers(const IGESParameterVector& additional_parameters,
                                       const pointer2ID& de2id) {
    const auto& parameters = additional_parameters;
    // 追加のパラメータがない場合は何もしない
    if (parameters.size() == 0) return;

    size_t former_length = static_cast<size_t>(parameters.get<int>(0));
    if (former_length > 0) {
        // 追加のパラメータ1を設定
        former_additional_pointers_.reserve(former_length);
        for (size_t i = 0; i < former_length; ++i) {
            try {
                former_additional_pointers_.emplace_back(
                        GetObjectIDFromParameters(parameters, 1 + i, de2id, true));
            } catch (const std::exception& e) {
                throw std::out_of_range(std::to_string(i+1) + "th former additional pointer"
                        " not found in de2id: " + std::string(e.what()));
            }
        }
    }

    size_t latter_length = static_cast<size_t>(
            parameters.get<int>(1 + former_length));
    if (latter_length > 0) {
        // 追加のパラメータ2を設定
        latter_additional_pointers_.reserve(latter_length);
        for (size_t i = 0; i < latter_length; ++i) {
            try {
                latter_additional_pointers_.emplace_back(
                        GetObjectIDFromParameters(
                            parameters, 2 + former_length + i, de2id, true));
            } catch (const std::exception& e) {
                throw std::out_of_range(std::to_string(i+1) + "th latter additional pointer"
                        " not found in de2id: " + std::string(e.what()));
            }
        }
    }
}

void EntityBase::InitializePD(const pointer2ID& de2id) {
    try {
        // メインのパラメータの設定 (エンティティの種類に応じて異なる)
        auto end_index = SetMainPDParameters(de2id);

        // 追加のポインタが存在すれば、それを設定する
        if (pd_parameters_.size() > end_index) {
            auto additional_parameters = pd_parameters_.copy(
                    end_index, pd_parameters_.size() - end_index);
            SetAdditionalPointers(additional_parameters, de2id);

            // PDレコードのパラメータを切り詰める
            pd_parameters_ = pd_parameters_.copy(0, end_index);
        }
    } catch (const std::bad_variant_access& e) {
        // RawEntityPDを受け取るInitializePDに合わせるため、
        // std::bad_variant_accessをigesio::TypeConversionErrorに変換
        throw igesio::TypeConversionError(
            "Failed to retrieve value from the 'parameters' variable. "
            "Some of the stored values did not match the expected types for this entity.");
    }
}

std::optional<Vector3d> EntityBase::TransformImpl(
        const std::optional<Vector3d>& input, const bool is_point) const {
    // pointがstd::nulloptの場合はそのまま返す
    if (!input) return input;

    auto& trans = de_transformation_matrix_;
    if (trans.GetValueType() != DEFieldValueType::kPointer) {
        // 変換行列を参照していない場合はそのまま返す
        return input;
    }

    // 変換して返す
    if (is_point) {
        // 座標の変換は、回転と平行移動の両方を適用する
        return trans.GetRotation() * (*input) + trans.GetTranslation();
    }

    // 方向ベクトルの変換は、回転のみを適用する
    return trans.GetRotation() * (*input);
}



/**
 * publicメンバ関数の実装
 */

EntityBase::EntityBase(const RawEntityDE& de_record,
                       const IGESParameterVector& parameters,
                       const pointer2ID& de2id,
                       const ObjectID& iges_id)
        : pd_parameters_(parameters),
            id_((!iges_id.IsSet())
                ? IDGenerator::Generate(ObjectType::kEntityNew,
                                        static_cast<uint16_t>(de_record.entity_type))
                : IDGenerator::GetReservedID(iges_id, de_record.sequence_number)),
            type_(de_record.entity_type),
            form_number_(de_record.form_number) {
    // DEレコードのパラメータを設定
    SetDERecord(de_record, de2id);
}



/**
 * IEntityIdentifier implementation
 */

i_ent::RawEntityDE EntityBase::GetRawEntityDE(const id2pointer& id2de) const {
    // DEレコードのパラメータを取得
    RawEntityDE de;

    std::string field_name;
    try {
        // 1st field (Entity Type)
        de.entity_type = type_;
        // 2nd field (Parameter Data) -> Ignored in this context
        // 3rd field (Structure)
        field_name = "Structure (3rd field)";
        de.structure = de_structure_.GetValue(id2de);
        // 4th field (Line Font Pattern)
        field_name = "Line Font Pattern (4th field)";
        de.line_font_pattern = de_line_font_pattern_.GetValue(id2de);
        // 5th field (Level)
        field_name = "Level (5th field)";
        de.level = de_level_.GetValue(id2de);
        // 6th field (View)
        field_name = "View (6th field)";
        de.view = de_view_.GetValue(id2de);
        // 7th field (Transformation Matrix)
        field_name = "Transformation Matrix (7th field)";
        de.transformation_matrix = de_transformation_matrix_.GetValue(id2de);
        // 8th field (Label Display Associativity)
        field_name = "Label Display Associativity (8th field)";
        de.label_display_associativity =
            de_label_display_associativity_.GetValue(id2de);
        // 9th field (Status Number)
        de.status = de_status_;
        // 10th field (Sequence Number) -> Ignored in this context

        // 11th field (Entity Type) -> Already set in 1st field
        // 12th field (Line Weight)
        de.line_weight_number = de_line_weight_;
        // 13th field (Color Number)
        field_name = "Color Number (13th field)";
        de.color_number = de_color_.GetValue(id2de);
        // 14th field (Parameter Line Count) -> Ignored in this context
        // 15th field (Form Number)
        de.form_number = form_number_;
        // 16-17th fields (Reserved) -> Ignored in this context
        // 18th field (Entity Label)
        de.entity_label = de_entity_label_;
        // 19th field (Entity Subscript Number)
        de.entity_subscript_number = de_entity_subscript_number_;
        // 20th field (Sequence Number) -> Ignored in this context
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Failed to convert ID to pointer in DE field '"
                                + field_name + "': " + e.what());
    }

    return de;
}



/**
 * パラメータの検証
 */

igesio::ValidationResult EntityBase::Validate() const {
    // DEレコードのパラメータの有効性を確認
    ValidationResult result;
    try {
        igesio::entities::IsValid(GetRawEntityDE());
    } catch (const DataFormatError& e) {
        // エラーが発生した場合、そのエラーメッセージを結果に設定
        result = ValidationResult::Failure(
                "Invalid DE parameters: " + std::string(e.what()));
    }

    // PDレコードのパラメータの有効性を確認
    result.Merge(ValidatePD());
    return result;
}



/**
 * Directory Entry (DE) フィールドの操作 (EntityType, FormNumberを除く)
 */

bool EntityBase::OverwriteStructure(
        const std::shared_ptr<const IStructure>& structure) {
    if (!structure) return false;
    de_structure_.OverwriteID(structure->GetID());
    de_structure_.SetPointer(structure);
    return true;
}

bool EntityBase::OverwriteLineFontPattern(
        const std::shared_ptr<const ILineFontDefinition>& line_font_definition) {
    if (!line_font_definition) return false;
    de_line_font_pattern_.OverwriteID(line_font_definition->GetID());
    de_line_font_pattern_.SetPointer(line_font_definition);
    return true;
}

bool EntityBase::OverwriteLineFontPattern(const LineFontPattern& line_font_pattern) {
    de_line_font_pattern_.SetPattern(line_font_pattern);
    return true;
}

bool EntityBase::OverwriteLevel(const std::shared_ptr<const IDefinitionLevelsProperty>& level) {
    if (!level) return false;
    de_level_.OverwriteID(level->GetID());
    de_level_.SetPointer(level);
    return true;
}

bool EntityBase::OverwriteLevel(const int level) {
    de_level_.SetLevelNumber(level);
    return true;
}

bool EntityBase::OverwriteView(const std::shared_ptr<const IView>& view) {
    if (!view) return false;
    de_view_.OverwriteID(view->GetID());
    de_view_.SetPointer(view);
    return true;
}

bool EntityBase::OverwriteView(
        const std::shared_ptr<const IViewsVisibleAssociativity>& view) {
    if (!view) return false;
    de_view_.OverwriteID(view->GetID());
    de_view_.SetPointer(view);
    return true;
}

bool EntityBase::OverwriteTransformationMatrix(
        const std::shared_ptr<const ITransformation>& transformation_matrix) {
    if (!transformation_matrix) return false;
    de_transformation_matrix_.OverwriteID(transformation_matrix->GetID());
    de_transformation_matrix_.SetPointer(transformation_matrix);
    return true;
}

bool EntityBase::OverwriteLabelDisplayAssociativity(
        const std::shared_ptr<const ILabelDisplayAssociativity>& label_display_associativity) {
    if (!label_display_associativity) return false;
    de_label_display_associativity_.OverwriteID(label_display_associativity->GetID());
    de_label_display_associativity_.SetPointer(label_display_associativity);
    return true;
}

void EntityBase::SetBlankStatus(const bool blank_status) {
    de_status_.blank_status = blank_status;
}

void EntityBase::SetSubordinateEntitySwitch(
        const SubordinateEntitySwitch subordinate_entity_switch) {
    de_status_.subordinate_entity_switch = subordinate_entity_switch;
}

void EntityBase::SetEntityUseFlag(const EntityUseFlag entity_use_flag) {
    de_status_.entity_use_flag = entity_use_flag;
}

void EntityBase::SetHierarchyType(const HierarchyType hierarchy) {
    de_status_.hierarchy = hierarchy;
}

bool EntityBase::SetLineWeightNumber(const int line_weight) {
    if (line_weight < 0) return false;
    de_line_weight_ = line_weight;
    return true;
}

bool EntityBase::OverwriteColor(const std::shared_ptr<const IColorDefinition>& color) {
    if (!color) return false;
    de_color_.OverwriteID(color->GetID());
    de_color_.SetPointer(color);
    return true;
}

bool EntityBase::OverwriteColor(const ColorNumber& color) {
    de_color_.SetColor(color);
    return true;
}

bool EntityBase::SetEntityLabel(const std::string& entity_label) {
    if (entity_label.size() > 8) {
        return false;  // エンティティラベルは最大8文字
    }
    de_entity_label_ = entity_label;
    return true;
}

bool EntityBase::SetEntitySubscript(const int entity_subscript) {
    if (entity_subscript > 99999999 || entity_subscript < -9999999) {
        return false;  // エンティティサブスクリプト番号は (符号含め) 最大8桁
    }
    de_entity_subscript_number_ = entity_subscript;
    return true;
}

igesio::IGESParameterVector EntityBase::GetParameters() const {
    // 追加のポインタを除いたパラメータデータを取得
    auto parameters = GetMainPDParameters();

    if (former_additional_pointers_.empty() &&
        latter_additional_pointers_.empty()) {
        // 追加のポインタがどちらもない場合は終了
        return parameters;
    }

    // 追加のポインタ1をparametersに追加
    parameters.push_back(static_cast<int>(former_additional_pointers_.size()));
    for (const auto& ptr : former_additional_pointers_) {
        parameters.push_back(ptr.GetID());
    }
    // 追加のポインタ2をparametersに追加
    parameters.push_back(static_cast<int>(latter_additional_pointers_.size()));
    for (const auto& ptr : latter_additional_pointers_) {
        parameters.push_back(ptr.GetID());
    }
    return parameters;
}



/**
 * Parameter Data (PD) フィールドの操作
 */

std::vector<igesio::ObjectID>
EntityBase::GetReferencedEntityIDs() const {
    std::vector<ObjectID> referenced_ids;

    // Directory Entry フィールドからの参照を追加
    if (de_structure_.GetValueType() == DEFieldValueType::kPointer) {
        referenced_ids.push_back(de_structure_.GetID());
    }
    if (de_line_font_pattern_.GetValueType() == DEFieldValueType::kPointer) {
        referenced_ids.push_back(de_line_font_pattern_.GetID());
    }
    if (de_level_.GetValueType() == DEFieldValueType::kPointer) {
        referenced_ids.push_back(de_level_.GetID());
    }
    if (de_view_.GetValueType() == DEFieldValueType::kPointer) {
        referenced_ids.push_back(de_view_.GetID());
    }
    if (de_transformation_matrix_.GetValueType() == DEFieldValueType::kPointer) {
        referenced_ids.push_back(de_transformation_matrix_.GetID());
    }
    if (de_label_display_associativity_.GetValueType() == DEFieldValueType::kPointer) {
        referenced_ids.push_back(de_label_display_associativity_.GetID());
    }
    if (de_color_.GetValueType() == DEFieldValueType::kPointer) {
        referenced_ids.push_back(de_color_.GetID());
    }

    // PDレコードの参照を追加
    auto pd_referenced_ids = GetChildIDs();
    referenced_ids.insert(referenced_ids.end(), pd_referenced_ids.begin(),
                            pd_referenced_ids.end());

    // Additional pointersのIDを追加
    for (const auto& ptr : former_additional_pointers_) {
        referenced_ids.push_back(ptr.GetID());
    }
    for (const auto& ptr : latter_additional_pointers_) {
        referenced_ids.push_back(ptr.GetID());
    }

    return referenced_ids;
}

bool EntityBase::AreAllReferencesSet() const {
    auto unresolved_ids = GetUnresolvedReferences();
    if (unresolved_ids.empty()) {
        return true;  // 参照がない場合は全て設定済みとみなす
    }
    return false;
}

std::unordered_set<igesio::ObjectID>
EntityBase::GetUnresolvedReferences() const {
    std::unordered_set<ObjectID> referenced_ids;

    // Directory Entry フィールドからの参照を追加
    if (auto id = de_structure_.GetUnsetID(); id.has_value()) {
        referenced_ids.insert(id.value());
    }
    if (auto id = de_line_font_pattern_.GetUnsetID(); id.has_value()) {
        referenced_ids.insert(id.value());
    }
    if (auto id = de_level_.GetUnsetID(); id.has_value()) {
        referenced_ids.insert(id.value());
    }
    if (auto id = de_view_.GetUnsetID(); id.has_value()) {
        referenced_ids.insert(id.value());
    }
    if (auto id = de_transformation_matrix_.GetUnsetID(); id.has_value()) {
        referenced_ids.insert(id.value());
    }
    if (auto id = de_label_display_associativity_.GetUnsetID(); id.has_value()) {
        referenced_ids.insert(id.value());
    }
    if (auto id = de_color_.GetUnsetID(); id.has_value()) {
        referenced_ids.insert(id.value());
    }

    // PDレコードの未設定の参照のIDを取得
    auto unresolved_ids = GetUnresolvedPDReferences();
    referenced_ids.insert(unresolved_ids.begin(), unresolved_ids.end());

    // Additional pointersのIDを追加
    for (const auto& ptr : former_additional_pointers_) {
        if (!ptr.IsPointerSet()) referenced_ids.insert(ptr.GetID());
    }
    for (const auto& ptr : latter_additional_pointers_) {
        if (!ptr.IsPointerSet()) referenced_ids.insert(ptr.GetID());
    }

    return referenced_ids;
}

bool EntityBase::SetUnresolvedReference(const std::shared_ptr<const EntityBase>& entity) {
    // Directory Entry フィールドの参照を設定
    if (de_structure_.GetID() == entity->GetID()) {
        if (auto ptr = std::dynamic_pointer_cast<const IStructure>(entity)) {
            de_structure_.SetPointer(ptr);
            return true;
        }
    } else if (de_line_font_pattern_.GetID() == entity->GetID()) {
        if (auto ptr = std::dynamic_pointer_cast<const ILineFontDefinition>(entity)) {
            de_line_font_pattern_.SetPointer(ptr);
            return true;
        }
    } else if (de_level_.GetID() == entity->GetID()) {
        if (auto ptr = std::dynamic_pointer_cast<const IDefinitionLevelsProperty>(entity)) {
            de_level_.SetPointer(ptr);
            return true;
        }
    } else if (de_view_.GetID() == entity->GetID()) {
        if (auto ptr = std::dynamic_pointer_cast<const IView>(entity)) {
            de_view_.SetPointer(ptr);
            return true;
        }
        if (auto ptr = std::dynamic_pointer_cast<
                                const IViewsVisibleAssociativity>(entity)) {
            de_view_.SetPointer(ptr);
            return true;
        }
    } else if (de_transformation_matrix_.GetID() == entity->GetID()) {
        if (auto ptr = std::dynamic_pointer_cast<const ITransformation>(entity)) {
            de_transformation_matrix_.SetPointer(ptr);
            return true;
        }
    } else if (de_label_display_associativity_.GetID() == entity->GetID()) {
        if (auto ptr = std::dynamic_pointer_cast<const ILabelDisplayAssociativity>(entity)) {
            de_label_display_associativity_.SetPointer(ptr);
            return true;
        }
    } else if (de_color_.GetID() == entity->GetID()) {
        if (auto ptr = std::dynamic_pointer_cast<const IColorDefinition>(entity)) {
            de_color_.SetPointer(ptr);
            return true;
        }
    } else {
        // PDレコードの参照を設定
        if (SetUnresolvedPDReferences(entity)) {
            return true;  // 指定されたエンティティと同一のIDを持つ参照がある場合
        }

        // 追加ポインタの参照を設定
        for (auto& ptr : former_additional_pointers_) {
            if ((ptr.GetID() == entity->GetID()) && ptr.SetPointer(entity)) return true;
        }
        for (auto& ptr : latter_additional_pointers_) {
            if ((ptr.GetID() == entity->GetID()) && ptr.SetPointer(entity)) return true;
        }
    }

    // どの参照にも一致しなかった場合は失敗
    return false;
}
