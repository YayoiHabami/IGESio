/**
 * @file entities/entity_base.h
 * @brief 個別エンティティクラスの基底クラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_ENTITY_BASE_H_
#define IGESIO_ENTITIES_ENTITY_BASE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/validation_result.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/entities/de.h"
#include "igesio/entities/pointer_container.h"



namespace igesio::entities {

/// @brief 全てのエンティティクラスの基底クラス
/// @note このクラスは、全てのエンティティクラスが備えるべき基本的な
///       機能を提供する. 特に、Directory Entryセクションの関連の処理については、
///       全て本クラスで実装される.
/// @note 個別エンティティクラスにおいては、以下のメンバ関数をオーバーライドすること:
///       - コンストラクタ: 常にオーバーライドが必要 (`EntityBase`&`InitializePD`を呼びだすこと)
///       - `GetMainPDParameters`(protected): 常にオーバーライドが必要
///       - `SetMainPDParameters`(protected): 常にオーバーライドが必要
///       - `ValidatePD`(public): 常にオーバーライドが必要
///       - `GetChildIDs`(public): 物理的に従属する要素を持つ場合
///       - `GetChildEntity`(public): 物理的に従属する要素を持つ場合
///       - `GetUnresolvedPDReferences`(public): PDセクションで他のエンティティを参照する場合
///       - `SetUnresolvedPDReferences`(public): PDセクションで他のエンティティを参照する場合
class EntityBase : public virtual IEntityIdentifier {
 private:
    /// @brief DEレコードを一括設定する
    /// @param de_record DEレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング (デフォルトは空)
    /// @throw std::out_of_range de2idが空でなく、かつde_record側で指定されている
    ///        DEポインターがde2idに存在しない場合
    /// @throw igesio::DataFormatError 4, 5, 13番目以外のフィールドで
    ///        正の値を指定した場合
    void SetDERecord(const RawEntityDE&, const pointer2ID& = {});

 protected:
    /// @brief Structure (3rd field of DE)
    DEStructure de_structure_;
    /// @brief Line Font Pattern (4th field of DE)
    DELineFontPattern de_line_font_pattern_;
    /// @brief Level (5th field of DE)
    DELevel de_level_;
    /// @brief View (6th field of DE)
    DEView de_view_;
    /// @brief Transformation Matrix (7th field of DE)
    DETransformationMatrix de_transformation_matrix_;
    /// @brief Label Display Associativity (8th field of DE)
    DELabelDisplayAssociativity de_label_display_associativity_;
    /// @brief Color Number (13th field of DE)
    DEColor de_color_;

    /// @brief Status Number (9th field of DE)
    EntityStatus de_status_;

    /// @brief Line Weight (12th field of DE)
    /// @note この値が直接エンティティの線幅を決定するわけではなく、
    ///       グローバルセクションの16/17番目のフィールドと併せて計算される.
    ///       計算式: 線幅 = (Line Weight * 17th-GS / 16th-GS)
    /// @note 値が0の場合、描画システムのデフォルト線幅が使用される.
    int de_line_weight_ = 0;

    /// @brief Entity Label (18th field of DE)
    /// @note Entity Subscript Numberと併せて、アプリケーション上での
    ///       エンティティの識別子として使用される最大8文字の文字列.
    std::string de_entity_label_;
    /// @brief Entity Subscript Number (19th field of DE)
    /// @note Entity Labelの数値修飾子
    int de_entity_subscript_number_ = 0;

    /// @brief エンティティのPDレコードのパラメータ
    /// @note 追加ポインタを除いたパラメータのベクトル
    IGESParameterVector pd_parameters_;

    /// @brief 追加のポインタ1 (Parameter Data Section)
    /// @note 各エンティティで指定されたパラメータの末尾に続く (場合のある)、
    ///       Associativity Instance Entity (Type 402), General Note Entity
    ///       (Type 212), Text Template Entity (Type 312) へのポインタ用
    ///       パラメータグループ
    std::vector<PointerContainer<true, EntityBase>> former_additional_pointers_;
    /// @brief 追加のポインタ2 (Parameter Data Section)
    /// @note 追加のポインタ1に続く (場合のある)、プロパティまたは属性テーブルへの
    ///       ポインタ用パラメータグループ
    std::vector<PointerContainer<true, EntityBase>> latter_additional_pointers_;

    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    virtual IGESParameterVector GetMainPDParameters() const = 0;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw std::bat_variant_access parametersの型が不正な場合
    /// @note 例えばCircular Arc Entity (Type 100) では、計7つのパラメータ
    ///       (インデックス0から6) がエンティティを定義するためのパラメータとして
    ///       使用される. この場合、戻り値は7となる. EntityBase側では、その
    ///       インデックスから追加パラメータが定義されている場合に、
    ///       `former_additional_pointers_`と`latter_additional_pointers_`に
    ///       参照を設定する. 一方、`parameters`の長さがちょうど7である場合は、
    ///       追加パラメータは存在しないとみなされ、これらのベクターは空のままとなる.
    virtual size_t SetMainPDParameters(const pointer2ID& de2id) = 0;

    /// @brief PDレコードの追加のパラメータグループを設定する
    /// @param additional_parameters 追加のポインタ部分のIGESParameterVector
    /// @param de2id DEポインターとIDのマッピング
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::out_of_range 追加のパラメータが存在するが、その冒頭で指定されている
    ///        追加のパラメータの数が、parametersの長さを超えている場合
    /// @throw std::bad_variant_access 追加のパラメータグループに相当する部分の
    ///        parametersの型がintではない場合 (Integer型かPointer型である必要がある)
    void SetAdditionalPointers(const IGESParameterVector&, const pointer2ID&);

    /// @brief PDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @note 内部で子クラスのメンバを初期化しているため、EntityBase側のコンストラクタで
    ///       SetMainPDParametersを呼び出すことができない。このため、この関数を
    ///       継承クラス側のコンストラクタで呼び出す必要がある。
    void InitializePD(const pointer2ID&);

    /// @brief 自身が参照する変換行列に従い、座標を変換する
    /// @param point 変換前の座標
    /// @return 変換後の座標
    /// @note pointがstd::nulloptの場合はそのまま返す
    std::optional<Vector3d> TransformPoint(const std::optional<Vector3d>&) const;

    /// @brief コンストラクタ (デフォルトのDEレコードを使用)
    /// @param entity_type エンティティのタイプ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @note de2idを空のままにした場合、parameters側で指定されている
    ///       ポインター (int型) はそのままIDとして使用される.
    /// @note 継承コンストラクタでは、必ず`InitializePD`を呼び出すこと.
    ///       これにより、PDレコードのパラメータが設定され、必要な追加のポインタが設定される.
    EntityBase(const EntityType entity_type,
               const IGESParameterVector& parameters,
               const pointer2ID& de2id = {})
        : EntityBase(RawEntityDE::ByDefault(entity_type),
                     parameters, de2id) {}

 public:
    /// @brief プログラム上でエンティティを一意に識別するためのID
    /// @note IDはIDGeneratorクラスを使用して生成される.
    /// @note プログラムの起動から終了までの間での一意性のみを保証するため、
    ///       プログラムの再起動後はIDがリセットされることに注意.
    ///       したがって、同じIGESの同じエンティティであっても、プログラムの再起動後は
    ///       異なるIDが生成される可能性がある.
    const uint64_t id_;
    /// @brief エンティティのタイプ (1st/11th fields of DE)
    const EntityType type_;
    /// @brief エンティティのフォーム番号 (15th field of DE)
    /// @note IGESのPDレコードにおけるフォーム番号を表す.
    int form_number_;

    /// @brief コンストラクタ
    /// @param de_record DEレコードのパラメータ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがkUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    /// @note de2idを空のままにした場合、parameters側で指定されている
    ///       ポインター (int型) はそのままIDとして使用される.
    /// @note 継承コンストラクタでは、必ず`InitializePD`を呼び出すこと.
    ///       これにより、PDレコードのパラメータが設定され、必要な追加のポインタが設定される.
    EntityBase(const RawEntityDE&, const IGESParameterVector&,
               const pointer2ID& = {}, const uint64_t = kUnsetID);

    /// @brief デストラクタ
    virtual ~EntityBase() = default;



    /**
     * IEntityIdentifier implementation
     */

    /// @brief エンティティのIDを取得する
    /// @return エンティティのID
    /// @note プログラムの起動から終了までの間での一意性のみを保証するため、
    ///       プログラムの再起動後はIDがリセットされることに注意.
    ///       したがって、同じIGESの同じエンティティであっても、プログラムの再起動後は
    ///       異なるIDが生成される可能性がある.
    uint64_t GetID() const override { return id_; }

    /// @brief エンティティのタイプを取得する
    /// @return エンティティのタイプ (EntityType列挙型の値)
    EntityType GetType() const override { return type_; }

    /// @brief エンティティのフォーム番号を取得する
    /// @return エンティティのフォーム番号
    int GetFormNumber() const override { return form_number_; }

    /// @brief DEセクションのパラメータを取得する
    /// @param id2de IDとDEポインターのマッピング
    /// @return DEセクションのパラメータを表すRawEntityDE
    /// @throw std::out_of_range id2deが空でなく、かつパラメータ側で指定されている
    ///        ポインターの値がid2deに存在しない場合
    /// @note id2deを指定した場合、出力されるRawEntityDEにおける
    ///       ポインターの値は、id2deに基づいて変換される.
    RawEntityDE GetRawEntityDE(const id2pointer& = {}) const;



    /**
     * パラメータの検証
     */

    /// @brief 設定されたパラメータが規格に適合しているかを確認する
    /// @return 検証結果
    ValidationResult Validate() const;
    /// @brief 設定されたパラメータが規格に適合しているかを確認する
    /// @return DE/PDセクションの全パラメータが適合しているか否か
    bool IsValid() const { return Validate().is_valid; }

    /// @brief PDレコードのパラメータの有効性を確認する
    /// @return 全パラメータが適合しているか否か
    virtual ValidationResult ValidatePD() const = 0;



    /**
     * Directory Entry (DE) フィールドの操作 (EntityType, FormNumberを除く)
     */

    /// @brief Structure (3rd field of DE) を取得する
    /// @return 構造定義フィールド
    const DEStructure& GetStructure() const { return de_structure_; }
    /// @brief Structure (3rd field of DE) の値をリセットする
    /// @note 参照の存在しない、デフォルト値の状態に設定する
    void ResetStructure() { de_structure_.Reset(); }
    /// @brief Structure (3rd field of DE) の参照先を変更する
    /// @param structure 新しく参照するStructureのポインタ
    /// @return 上書きに失敗した場合 (structureがnullptrの場合) は`false`を返す
    bool OverwriteStructure(const std::shared_ptr<const IStructure>&);

    /// @brief Line Font Pattern (4th field of DE) を取得する
    /// @return 線種パターンフィールド
    const DELineFontPattern&
    GetLineFontPattern() const { return de_line_font_pattern_; }
    /// @brief Line Font Pattern (4th field of DE) の値をリセットする
    /// @note 参照の存在しない、デフォルト値の状態に設定する
    void ResetLineFontPattern() { de_line_font_pattern_.Reset(); }
    /// @brief Line Font Pattern (4th field of DE) の参照先を変更する
    /// @param line_font_definition 新しく参照するLine Font Definition Entity
    ///        (Type 304) のポインタ
    /// @return 上書きに失敗した場合 (line_font_patternがnullptrの場合) は`false`を返す
    bool OverwriteLineFontPattern(const std::shared_ptr<const ILineFontDefinition>&);
    /// @brief Line Font Pattern (4th field of DE) の値を設定する
    /// @param line_font_pattern 新しい線種パターンフィールドの値
    /// @return 上書きに成功した場合は`true` (常に`true`を返す)
    /// @note 参照が設定されていた場合、無効化される
    bool OverwriteLineFontPattern(const LineFontPattern&);

    /// @brief Level (5th field of DE) を取得する
    /// @return レベルフィールド
    const DELevel& GetLevel() const { return de_level_; }
    /// @brief Level (5th field of DE) の値をリセットする
    /// @note 参照の存在しない、デフォルト値の状態に設定する
    void ResetLevel() { de_level_.Reset(); }
    /// @brief Level (5th field of DE) の参照先を変更する
    /// @param level 新しく参照するDefinition Levels Property Entity
    ///        (Type 406, Form 1) のポインタ
    /// @return 上書きに失敗した場合 (levelがnullptrの場合) は`false`を返す
    bool OverwriteLevel(const std::shared_ptr<const IDefinitionLevelsProperty>&);
    /// @brief Level (5th field of DE) の値を設定する
    /// @param level 新しいレベルフィールドの値
    /// @return 上書きに成功した場合は`true` (常に`true`を返す)
    /// @note 参照が設定されていた場合、無効化される
    bool OverwriteLevel(const int);

    /// @brief View (6th field of DE) を取得する
    /// @return ビューフィールド
    const DEView& GetView() const { return de_view_; }
    /// @brief View (6th field of DE) の値をリセットする
    /// @note 参照の存在しない、デフォルト値の状態に設定する
    void ResetView() { de_view_.Reset(); }
    /// @brief View (6th field of DE) の参照先を変更する
    /// @param view 新しく参照するView Entity (Type 410) のポインタ
    /// @return 上書きに失敗した場合 (viewがnullptrの場合) は`false`を返す
    bool OverwriteView(const std::shared_ptr<const IView>&);
    /// @brief View (6th field of DE) の参照先を変更する
    /// @param view 新しく参照するViews Visible Associativity Entity
    ///        (Type 402, Forms 3, 4, 19) のポインタ
    /// @return 上書きに失敗した場合 (viewがnullptrの場合) は`false`を返す
    bool OverwriteView(const std::shared_ptr<const IViewsVisibleAssociativity>&);

    /// @brief Transformation Matrix (7th field of DE) を取得する
    /// @return 変換行列フィールド
    const DETransformationMatrix&
    GetTransformationMatrix() const { return de_transformation_matrix_; }
    /// @brief Transformation Matrix (7th field of DE) の値をリセットする
    /// @note 参照の存在しない、デフォルト値の状態に設定する
    ///       (単位回転行列 & ゼロ平行移動ベクトルに相当)
    void ResetTransformationMatrix() {  de_transformation_matrix_.Reset(); }
    /// @brief Transformation Matrix (7th field of DE) の参照先を変更する
    /// @param transformation_matrix 新しく参照するTransformation Matrix Entity
    ///        (Type 124) のポインタ
    /// @return 上書きに失敗した場合 (transformation_matrixがnullptrの場合)
    ///         は`false`を返す
    bool OverwriteTransformationMatrix(const std::shared_ptr<const ITransformation>&);

    /// @brief Label Display Associativity (8th field of DE) を取得する
    /// @return ラベル表示結合性フィールド
    const DELabelDisplayAssociativity& GetLabelDisplayAssociativity() const {
        return de_label_display_associativity_;
    }
    /// @brief Label Display Associativity (8th field of DE) の値をリセットする
    /// @note 参照の存在しない、デフォルト値の状態に設定する
    void ResetLabelDisplayAssociativity() {
        de_label_display_associativity_.Reset();
    }
    /// @brief Label Display Associativity (8th field of DE) の参照先を変更する
    /// @param label_display_associativity 新しく参照するLabel Display Associativity Entity
    ///        (Type 408) のポインタ
    /// @return 上書きに失敗した場合 (label_display_associativityがnullptrの場合)
    ///         は`false`を返す
    bool OverwriteLabelDisplayAssociativity(
            const std::shared_ptr<const ILabelDisplayAssociativity>&);

    /// @brief Status Number (9th field of DE) の表示状態を取得する
    /// @return true: 表示 (status 00), false: 非表示 (status 01)
    bool GetBlankStatus() const { return de_status_.blank_status; }
    /// @brief Status Number (9th field of DE) の表示状態を変更する
    /// @param blank_status 新しい表示状態
    void SetBlankStatus(const bool);
    /// @brief Status Number (9th field of DE) の従属エンティティスイッチを取得する
    /// @return 従属エンティティスイッチ
    SubordinateEntitySwitch
    GetSubordinateEntitySwitch() const { return de_status_.subordinate_entity_switch; }
    /// @brief Status Number (9th field of DE) の従属エンティティスイッチを変更する
    /// @param subordinate_entity_switch 新しい従属エンティティスイッチ
    void SetSubordinateEntitySwitch(const SubordinateEntitySwitch);
    /// @brief Status Number (9th field of DE) のエンティティ用途フラグを取得する
    /// @return エンティティ用途フラグ
    EntityUseFlag
    GetEntityUseFlag() const { return de_status_.entity_use_flag; }
    /// @brief Status Number (9th field of DE) のエンティティ用途フラグを変更する
    /// @param entity_use_flag 新しいエンティティ用途フラグ
    void SetEntityUseFlag(const EntityUseFlag);
    /// @brief Status Number (9th field of DE) の階層の種類を取得する
    /// @return 階層の種類
    HierarchyType GetHierarchyType() const { return de_status_.hierarchy; }
    /// @brief Status Number (9th field of DE) の階層の種類を変更する
    /// @param hierarchy 新しい階層の種類
    void SetHierarchyType(const HierarchyType);
    /// @brief Status Number (9th field of DE) を取得する
    /// @return ステータス番号の構造体
    const EntityStatus& GetEntityStatus() const { return de_status_; }

    /// @brief Line Weight (12th field of DE) を取得する
    /// @return 線の太さ番号
    int GetLineWeightNumber() const { return de_line_weight_; }
    /// @brief Line Weight (12th field of DE) を設定する
    /// @param line_weight 新しい線の太さ番号
    /// @return 上書きに成功した場合は`true` (正の値であれば常に`true`を返す)
    bool SetLineWeightNumber(const int);

    /// @brief Color Number (13th field of DE) を取得する
    /// @return 色フィールド
    const DEColor& GetColor() const { return de_color_; }
    /// @brief Color Number (13th field of DE) の値をリセットする
    /// @note 参照の存在しない、デフォルト値の状態に設定する
    void ResetColor() { de_color_.Reset(); }
    /// @brief Color Number (13th field of DE) の参照先を変更する
    /// @param color 新しく参照するColor Entity (Type 108) のポインタ
    /// @return 上書きに失敗した場合 (colorがnullptrの場合) は`false`を返す
    bool OverwriteColor(const std::shared_ptr<const IColorDefinition>&);
    /// @brief Color Number (13th field of DE) の値を設定する
    /// @param color 新しい色フィールドの値
    /// @return 上書きに成功した場合は`true` (常に`true`を返す)
    /// @note 参照が設定されていた場合、無効化される
    bool OverwriteColor(const ColorNumber&);

    /// @brief Entity Label (18th field of DE) を取得する
    /// @return エンティティラベル
    const std::string& GetEntityLabel() const { return de_entity_label_; }
    /// @brief Entity Label (18th field of DE) を設定する
    /// @param entity_label 新しいエンティティラベル
    /// @return 9文字以上の文字列が指定された場合は`false`を返す
    bool SetEntityLabel(const std::string&);

    /// @brief Entity Subscript Number (19th field of DE) を取得する
    /// @return エンティティサブスクリプト番号
    int GetEntitySubscript() const { return de_entity_subscript_number_; }
    /// @brief Entity Subscript Number (19th field of DE) を設定する
    /// @param entity_subscript_number 新しいエンティティサブスクリプト番号
    /// @return 8桁で表現できない数値が指定された場合は`false`を返す
    bool SetEntitySubscript(const int);



    /**
     * Parameter Data (PD) フィールドの操作
     */

    /// @brief Parameter Dataセクションのパラメータを取得する
    /// @return パラメータデータのベクトル
    /// @note 追加ポインタを含む全Parameter Dataセクションのデータを取得する
    IGESParameterVector GetParameters() const;

    /// @brief 物理的に従属するエンティティを取得する
    /// @return 物理的に従属するエンティティのID
    virtual std::vector<uint64_t> GetChildIDs() const { return {}; }

    /// @brief 物理的に従属するエンティティのポインタを取得する
    /// @param id 物理的に従属するエンティティのID
    /// @return 物理的に従属するエンティティのポインタ
    /// @note 指定されたIDの、物理的に従属するエンティティが存在しない場合は
    ///       nullptrを返す
    virtual std::shared_ptr<const EntityBase>
    GetChildEntity([[maybe_unused]] const uint64_t id) const {
        return nullptr;
    }

    /// @brief エンティティが参照する全てのエンティティのIDを取得する
    /// @return 参照する全てのエンティティのID
    /// @note Directory Entry フィールド関連のメンバも含む
    virtual std::vector<uint64_t> GetReferencedEntityIDs() const;

    /// @brief エンティティが参照する全てのエンティティのポインタが設定済みか
    /// @return 一つでも未設定のポインタがある場合は`false`
    /// @note Directory Entry フィールド関連のメンバも含む
    virtual bool AreAllReferencesSet() const;

    /// @brief エンティティが参照する全てのエンティティのうち、
    ///        ポインタが未設定のもののIDを取得する
    /// @return ポインタが未設定のエンティティのIDのリスト
    /// @note Directory Entry フィールド関連のメンバも含む
    std::unordered_set<uint64_t> GetUnresolvedReferences() const;

    /// @brief 参照先エンティティのポインタを設定する
    /// @param entity ポインタを設定するエンティティ
    /// @return 指定されたエンティティと同一のIDを持つ参照がない場合は`false`を返す
    /// @note ポインタが設定済みの場合、上書きは行わない
    /// @note Directory Entry フィールド関連のメンバも含む
    bool SetUnresolvedReference(const std::shared_ptr<const EntityBase>&);



 protected:
    /// @brief PDレコードの未設定の参照のIDを取得する
    /// @return ポインタが未設定のエンティティのIDのリスト
    /// @note 追加ポインタは除く (EntityBase側で取得するため)
    virtual std::unordered_set<uint64_t> GetUnresolvedPDReferences() const {
        // 多くのエンティティは他のエンティティを参照しないため、空のリストを返す
        return {};
    }

    /// @brief PDレコード内の参照先エンティティのポインタを設定する
    /// @param entity ポインタを設定するエンティティ
    /// @return 指定されたエンティティと同一のIDを持つ参照がない場合は`false`を返す
    /// @note ポインタが設定済みの場合、上書きは行わない
    /// @note 追加ポインタは除く (EntityBase側で設定するため)
    virtual bool SetUnresolvedPDReferences(
            [[maybe_unused]] const std::shared_ptr<const EntityBase>& entity) {
        // 多くのエンティティは他のエンティティを参照しないため、常に失敗
        return false;
    }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_ENTITY_BASE_H_
