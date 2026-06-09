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
#include <optional>
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
    /// @throw igesio::EntityParameterError parametersの数が不整合な場合
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
    /// @throw igesio::EntityDataError parametersのいずれかが正しくない場合
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
     * 直線部・角点サポート (ICurve override)
     */

    /// @brief 直線部のパラメータ区間リストを返す
    /// @return 各構成曲線の直線区間をComposite Curveのパラメータ空間に
    ///         マッピングしたリスト (例えばn個目の構成曲線が直線かつt∈[t1, t2]
    ///         であれば、n-1個目までの曲線のパラメータ長を累積してt1, t2に
    ///         加算した区間が返される)
    std::vector<std::array<double, 2>> GetLinearSegments() const override;

    /// @brief 角点のパラメータ値リストを返す
    /// @return 構成曲線間の接合点、および各構成曲線の角点を
    ///         Composite Curveのパラメータ空間にマッピングしたリスト
    std::vector<double> GetCornerParams() const override;

    /// @brief 角点における定義空間の左側単位接線ベクトルを返す
    /// @param t パラメータ値
    /// @return 接合点では直前の構成曲線の終端接線を返す.
    ///         それ以外は構成曲線のTryGetDefinedLeftTangentAtに委譲
    std::optional<Vector3d> TryGetDefinedLeftTangentAt(const double t) const override;

    /// @brief 角点における定義空間の右側単位接線ベクトルを返す
    /// @param t パラメータ値
    /// @return 接合点では直後の構成曲線の始端接線を返す.
    ///         それ以外は構成曲線のTryGetDefinedRightTangentAtに委譲
    std::optional<Vector3d> TryGetDefinedRightTangentAt(const double t) const override;



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
    TryGetDefinedDerivatives(const double, const unsigned int) const override;

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

    /// @brief 曲線を末尾に追加する
    /// @param curve 追加する曲線
    /// @return 追加に成功した場合は`true`、curveがnullptrの場合は`false`
    /// @throw igesio::EntityValueError 直前の曲線の終点と追加する曲線の始点が
    ///        連続でない場合 (許容差は座標スケールに対する相対値)
    /// @note curveのSubordinateEntitySwitchはkPhysicallyDependentに設定される
    /// @note 端点が取得できない曲線 (未解決参照・無限長) との接合は
    ///       チェック不能として許容される
    bool AddCurve(const std::shared_ptr<ICurve>&);

    /// @brief 範囲[first, last] (両端を含む) の構成曲線をnew_curvesで置き換える
    /// @param first 置換範囲の先頭インデックス (0始まり)
    /// @param last 置換範囲の末尾インデックス (0始まり)
    /// @param new_curves 置換後の曲線列 (空の場合は範囲削除)
    /// @return 取り外された曲線のリスト (未解決参照だった要素はnullptr)
    /// @throw std::invalid_argument first > last、またはnew_curvesに
    ///        nullptrが含まれる場合
    /// @throw std::out_of_range lastが範囲外の場合
    /// @throw igesio::EntityValueError 置換後の隣接接合 (置換列内部、および
    ///        前後の既存曲線との境界) が連続でない場合
    /// @note 検証がすべて通るまで構成曲線リストは変更されない (強い例外保証)
    /// @note new_curvesの各要素のSubordinateEntitySwitchは
    ///       kPhysicallyDependentに設定される。取り外された曲線のスイッチは
    ///       変更されない (他エンティティからの参照有無を本クラスは知り得ないため)
    std::vector<std::shared_ptr<const ICurve>> ReplaceCurves(
            const size_t first, const size_t last,
            const std::vector<std::shared_ptr<ICurve>>& new_curves);

    /// @brief 指定インデックスの曲線を置き換える
    /// @param index 置き換える曲線のインデックス (0始まり)
    /// @param curve 新しい曲線
    /// @return 取り外された曲線 (未解決参照だった場合はnullptr)
    /// @throw std::invalid_argument curveがnullptrの場合
    /// @throw std::out_of_range indexが範囲外の場合
    /// @throw igesio::EntityValueError 前後の曲線と連続でない場合
    /// @note ReplaceCurves(index, index, {curve})と等価
    std::shared_ptr<const ICurve> SetCurveAt(
            const size_t index, const std::shared_ptr<ICurve>& curve);

    /// @brief 先頭の曲線を取り外す
    /// @return 取り外された曲線 (未解決参照だった場合はnullptr)
    /// @throw std::out_of_range 曲線が1つもない場合
    /// @note 中間の曲線の単独削除は連続性を破壊するため提供しない。
    ///       削除後も前後が接続する場合に限り、ReplaceCurvesの範囲削除で
    ///       中間の曲線を削除できる
    std::shared_ptr<const ICurve> RemoveFirstCurve();

    /// @brief 末尾の曲線を取り外す
    /// @return 取り外された曲線 (未解決参照だった場合はnullptr)
    /// @throw std::out_of_range 曲線が1つもない場合
    std::shared_ptr<const ICurve> RemoveLastCurve();

    /// @brief すべての曲線を取り外す
    /// @note 取り外された曲線のSubordinateEntitySwitchは変更されない
    void ClearCurves();



    /**
     * CompositeCurve: パラメータ写像・接合診断
     */

    /// @brief 各構成曲線の開始グローバルパラメータを取得する
    /// @return サイズn+1のリスト。i番目 (i < n) は構成曲線iの開始グローバル
    ///         パラメータ、末尾は全体の終端パラメータ
    /// @note 未解決参照・無限長の曲線はパラメータ長0として扱われる
    std::vector<double> GetCurveBreakParameters() const;

    /// @brief グローバルパラメータに対応する構成曲線を特定する
    /// @param t グローバルパラメータ値
    /// @return 構成曲線のインデックスと、その曲線上におけるローカルパラメータ
    ///         の組。tが範囲外の場合はnullopt
    std::optional<std::pair<size_t, double>>
    TryGetCurveIndexAtParameter(const double) const;

    /// @brief 構成曲線のローカルパラメータをグローバルパラメータへ変換する
    /// @param index 構成曲線のインデックス (0始まり)
    /// @param t_local 当該曲線上のローカルパラメータ
    /// @return グローバルパラメータ。曲線が未解決参照・無限長の場合、
    ///         またはt_localが当該曲線のパラメータ範囲外の場合はnullopt
    /// @throw std::out_of_range indexが範囲外の場合
    std::optional<double>
    TryGetGlobalParameter(const size_t index, const double t_local) const;

    /// @brief 各接合点の隙間距離を取得する
    /// @return サイズn-1のリスト (曲線が2つ未満の場合は空)。i番目は構成曲線iの
    ///         終点と構成曲線i+1の始点の距離。端点が取得できない接合点はnullopt
    /// @note ValidatePDが警告する接合不連続を定量的に調べるための診断用
    std::vector<std::optional<double>> GetJunctionGaps() const;



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
};



/**
 * ファクトリ関数
 */

/// @brief 構成曲線のリストからCompositeCurveを作成する
/// @param curves 構成曲線のリスト (連結順)
/// @return 作成されたCompositeCurveのshared_ptr
/// @throw std::invalid_argument curvesが空、またはnullptrを含む場合
/// @throw igesio::EntityValueError 隣接する曲線の端点が連続でない場合
///        (許容差は座標スケールに対する相対値)
/// @note 各曲線のSubordinateEntitySwitchはkPhysicallyDependentに設定される
/// @note 端点が取得できない曲線 (未解決参照・無限長) との接合は
///       チェック不能として許容される
std::shared_ptr<CompositeCurve> MakeCompositeCurve(
        const std::vector<std::shared_ptr<ICurve>>& curves);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_COMPOSITE_CURVE_H_
