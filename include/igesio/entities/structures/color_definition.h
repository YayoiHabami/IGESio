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
#include <memory>
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
    /// @throw igesio::EntityParameterError parametersの数が3未満の場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    size_t SetMainPDParameters([[maybe_unused]] const pointer2ID&) override;

 public:
    /// @brief コンストラクタ
    /// @param de_record DEレコードのパラメータ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @throw igesio::EntityDataError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    ColorDefinition(const RawEntityDE&, const IGESParameterVector&,
                    const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief RGB値とオプションの色名を指定するコンストラクタ
    /// @param rgb RGB値 (0.0-100.0)
    /// @param color_name 色名 (オプション)
    /// @throw igesio::EntityValueError パラメータのいずれかが正しくない場合
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



    /**
     * 色の設定・変換
     */

    /// @brief RGB値を設定する
    /// @param rgb RGB値 (0.0〜100.0)
    /// @throw igesio::EntityValueError 成分が[0.0, 100.0]の範囲外の場合
    /// @note DEのColor Numberも最も近い標準色へ更新される
    ///       (構築時と同じ不変条件を維持する)
    void SetRGB(const std::array<double, 3>&);

    /// @brief 色名を設定する
    /// @param color_name 色名. 空文字列を指定した場合は色名なしとなり、
    ///        PD出力時に4番目のパラメータが省略される
    void SetColorName(const std::string& color_name) { color_name_ = color_name; }

    /// @brief RGB値を8bitスケール (0〜255) で取得する
    /// @return RGB値の配列 (0〜255、四捨五入)
    /// @note 範囲外のRGB値を持つ場合 (不正なファイルの読み込み時等) は
    ///       [0, 255]に飽和させる
    std::array<int, 3> GetRGB255() const;

    /// @brief 16進カラーコードを取得する
    /// @return "#RRGGBB"形式 (大文字)
    std::string GetHexCode() const;

    /// @brief 自身のRGB値に最も近い標準色番号を取得する
    /// @return 最も近いColorNumber (kBlack〜kWhite)
    ColorNumber GetClosestColorNumber() const {
        return GetClosestStandardColor(rgb_);
    }
};



/**
 * ファクトリ関数
 */

/// @brief RGB値 (0.0〜100.0、IGESネイティブスケール) からColorDefinitionを作成する
/// @param rgb RGB値 (0.0〜100.0)
/// @param color_name 色名 (省略可)
/// @return 作成されたColorDefinitionのshared_ptr
/// @throw igesio::EntityValueError RGB成分が[0.0, 100.0]の範囲外の場合
/// @note DEのColor Numberには最も近い標準色が設定される
std::shared_ptr<ColorDefinition> MakeColorDefinition(
        const std::array<double, 3>& rgb, const std::string& color_name = "");

/// @brief 8bit RGB値 (0〜255) からColorDefinitionを作成する
/// @param r 赤成分 (0〜255)
/// @param g 緑成分 (0〜255)
/// @param b 青成分 (0〜255)
/// @param color_name 色名 (省略可)
/// @return 作成されたColorDefinitionのshared_ptr
/// @throw std::invalid_argument 成分が[0, 255]の範囲外の場合
std::shared_ptr<ColorDefinition> MakeColorDefinitionFromRGB255(
        const int r, const int g, const int b, const std::string& color_name = "");

/// @brief 16進カラーコードからColorDefinitionを作成する
/// @param hex_code "#RRGGBB"または"RRGGBB"形式のカラーコード (大文字小文字不問)
/// @param color_name 色名 (省略可)
/// @return 作成されたColorDefinitionのshared_ptr
/// @throw std::invalid_argument hex_codeが上記形式でない場合
std::shared_ptr<ColorDefinition> MakeColorDefinitionFromHex(
        const std::string& hex_code, const std::string& color_name = "");

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_STRUCTURES_COLOR_DEFINITION_H_
