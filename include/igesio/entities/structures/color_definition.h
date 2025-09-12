/**
 * @file entities/structures/color_definition.h
 * @brief ColorDefinition (Type 314): 色定義エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_STRUCTURES_COLOR_DEFINITION_H_
#define IGESIO_ENTITIES_STRUCTURES_COLOR_DEFINITION_H_

#include <array>
#include <string>
#include <vector>

#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief 各色のベクトル表現
/// @note kColorVectors[static_cast<int>(color_number)]で色のベクトルを取得できる
constexpr std::array<std::array<double, 3>, 9> kColorVectors = {
        std::array{  0.0,   0.0,   0.0},   // kNoColor (Do not use)
        std::array{  0.0,   0.0,   0.0},   // kBlack
        std::array{100.0,   0.0,   0.0},   // kRed
        std::array{  0.0, 100.0,   0.0},   // kGreen
        std::array{  0.0,   0.0, 100.0},   // kBlue
        std::array{100.0, 100.0,   0.0},   // kYellow
        std::array{100.0,   0.0, 100.0},   // kMagenta
        std::array{  0.0, 100.0, 100.0},   // kCyan
        std::array{100.0, 100.0, 100.0}};  // kWhite

/// @brief Color Definitionエンティティ (Type 314) の実装クラス
class ColorDefinition : public EntityBase, public virtual IColorDefinition {
 private:
    /// @brief RGB値 (赤, 緑, 青). 各値は0.0から100.0の範囲
    std::array<double, 3> rgb_;
    /// @brief 色名 (オプション)
    std::string color_name_;

    /// @brief 指定されたRGB値に最も近い標準色を計算する
    /// @param rgb 比較するRGB値 (0.0-100.0)
    /// @return 最も近いColorNumber
    ColorNumber GetClosestStandardColor(const std::array<double, 3>&) const;

 protected:
    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    /// @throw igesio::DataFormatError parametersの数が3未満の場合
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
    ColorDefinition(const RawEntityDE&, const IGESParameterVector&,
                    const pointer2ID& = {}, const uint64_t = kUnsetID);

    /// @brief RGB値とオプションの色名を指定するコンストラクタ
    /// @param rgb RGB値 (0.0-100.0)
    /// @param color_name 色名 (オプション)
    /// @throw igesio::DataFormatError パラメータのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError パラメータの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつパラメータ側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    explicit ColorDefinition(const std::array<double, 3>&, const std::string& = "");

    /// @brief デストラクタ
    ~ColorDefinition() override = default;



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override;


    /**
     * IColorDefinition implementation
     */

    /// @brief 色の名前を取得する
    /// @return 色の名前. 色名が設定されていない場合は空文字列を返す.
    std::string GetColorName() const override { return color_name_; }
    /// @brief 色をRGB値で取得する
    /// @return RGB値の配列 (0.0-100.0)
    std::array<double, 3> GetRGB() const override { return rgb_; }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_STRUCTURES_COLOR_DEFINITION_H_
