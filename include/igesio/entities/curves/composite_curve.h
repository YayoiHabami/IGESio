/**
 * @file entities/curves/compsite_curve.h
 * @brief CompositeCurve (Type 102): 複合曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_CURVES_COMPOSITE_CURVE_H_
#define IGESIO_ENTITIES_CURVES_COMPOSITE_CURVE_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief 複合曲線を表すエンティティクラス
/// @brief 複数の曲線を連結して構成される曲線
/// @todo 無限の長さを持つ曲線が指定された場合の処理
class CompositeCurve : public EntityBase, public virtual ICurve3D {
 protected:
    /// @brief 複合曲線を構成する曲線のリスト
    std::vector<PointerContainer<false, ICurve>> curves_;

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
    /// @brief 引数を取らないコンストラクタ
    /// @note デフォルトのDEレコードと空のPDレコードを使用する
    CompositeCurve();

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
    CompositeCurve(const RawEntityDE&, const IGESParameterVector&,
                   const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());



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
     * CompositeCurve: 構成要素 (曲線) の操作
     */

    /// @brief 曲線の数を取得する
    /// @return 構成する曲線の数
    size_t GetCurveCount() const { return curves_.size(); }

    /// @brief 曲線を取得する
    /// @param index 取得する曲線のインデックス (0始まり)
    /// @return 指定されたインデックスの曲線のポインタ、参照が設定されていない場合はnullptr
    /// @throw std::out_of_range indexが範囲外の場合
    std::shared_ptr<const ICurve> GetCurveAt(const size_t) const;

    /// @brief 曲線を追加する
    /// @param curve 追加する曲線
    /// @return 追加に成功した場合は`true`、失敗した場合は`false`
    /// @note curveのSubordinateEntitySwitchはkPhysicallyDependent
    ///       に設定される
    bool AddCurve(const std::shared_ptr<ICurve>&);

    // TODO: 曲線を削除するメソッドを追加する



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



 private:
    /// @brief 指定されたパラメータ値に対する曲線を取得する
    /// @param t パラメータ値
    /// @return 指定されたパラメータ値に対応する曲線のポインタ、
    ///         およびその曲線上におけるパラメータ値
    /// @note パラメータ値が範囲外の場合はnullptrを返す
    std::pair<std::shared_ptr<const ICurve>, double>
    GetCurveAtParameter(const double) const;

    /// @brief 指定されたパラメータ値に対する曲線のインデックスを取得する
    /// @param t パラメータ値
    /// @return 指定されたパラメータ値に対応する曲線のインデックス、
    ///         およびその曲線上におけるパラメータ値
    /// @note パラメータ値が範囲外の場合はnulloptを返す
    std::optional<std::pair<size_t, double>>
    GetCurveIndexAtParameter(const double) const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_COMPOSITE_CURVE_H_
