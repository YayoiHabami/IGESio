/**
 * @file models/assembly.h
 * @brief エンティティの集合を表すアセンブリクラス
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 * @note 「エンティティの集合」という責務をIgesDataから分離するためのクラス.
 *       再帰的なツリーノードであり、平坦なエンティティマップ・子Assembly・親への
 *       弱参照・大域変換・メタ情報を保持する. メモリ上の構造であり、IGESファイルへは
 *       直接シリアライズしない. 一つのエンティティは厳密に一つのAssemblyに所属する.
 */
#ifndef IGESIO_MODELS_ASSEMBLY_H_
#define IGESIO_MODELS_ASSEMBLY_H_

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/matrix.h"
#include "igesio/numerics/bounding_box.h"
#include "igesio/entities/entity_base.h"



namespace igesio::entities {
// 変換ビュー(views/curve_view.h, views/surface_view.h で定義)
class CurveView;
class SurfaceView;
}  // namespace igesio::entities



namespace igesio::models {

/// @brief 座標系(フレーム)指定子
/// @note 取得したい座標がどのフレームに属するか(§4.1の梯子)を表す. 静的生成関数
///       (Definition/EntityLocal/World/RelativeTo)で構築し、ビュー生成・配置解決の
///       引数として用いる.
class CoordFrame {
 public:
    /// @brief フレームの種類
    enum class Kind {
        /// @brief 定義空間 (P_def そのもの. 変換を一切適用しない)
        kDefinition,
        /// @brief エンティティ自身のフレーム (M_entityのみ適用. Assembly非依存)
        kEntityLocal,
        /// @brief ワールド/モデル空間 (ルートまでの全Assembly大域変換 + M_entity)
        kWorld,
        /// @brief 指定Assemblyの定義空間 (そのAssemblyの大域変換G_kは含めない. §4.1)
        kRelative,
    };

    /// @brief 定義空間フレームを生成する
    static CoordFrame Definition() { return CoordFrame(Kind::kDefinition); }
    /// @brief エンティティ自身のフレームを生成する
    static CoordFrame EntityLocal() { return CoordFrame(Kind::kEntityLocal); }
    /// @brief ワールド/モデル空間フレームを生成する
    static CoordFrame World() { return CoordFrame(Kind::kWorld); }
    /// @brief 指定Assemblyの定義空間フレームを生成する
    /// @param base 基準AssemblyのID (そのAssemblyの大域変換G_k自身は含めない. §4.1)
    static CoordFrame RelativeTo(const ObjectID& base) {
        return CoordFrame(Kind::kRelative, base);
    }

    /// @brief フレームの種類を取得する
    Kind GetKind() const { return kind_; }
    /// @brief 相対指定の基準AssemblyのIDを取得する
    /// @return 基準AssemblyのID. GetKind()==kRelativeのときのみ有効.
    const ObjectID& GetRelativeBase() const { return relative_base_; }

 private:
    /// @brief コンストラクタ
    /// @param kind フレームの種類
    /// @param base 相対指定の基準AssemblyのID (kRelative以外では未設定)
    explicit CoordFrame(const Kind kind, const ObjectID& base = ObjectID())
            : kind_(kind), relative_base_(base) {}

    /// @brief フレームの種類
    Kind kind_;
    /// @brief 相対指定の基準AssemblyのID (kRelativeのときのみ有効)
    ObjectID relative_base_;
};

/// @brief Assemblyのメタ情報
/// @note 描画・編集の際に参照される付随情報をまとめた構造体.
struct AssemblyMetadata {
    /// @brief アセンブリの名前
    std::string name;
    /// @brief 可視性 (描画対象とするか)
    bool visible = true;
    /// @brief 役割を示すタグ (任意の分類用文字列)
    std::string role_tag;
    /// @brief 色のオーバーライド (RGB; [0, 1]). 未設定の場合はメンバの色を使用する
    std::optional<std::array<float, 3>> color_override;
    /// @brief 不透明度のオーバーライド ([0, 1]). 未設定の場合はメンバの値を使用する
    std::optional<float> opacity_override;
};



/// @brief エンティティの集合を表すアセンブリクラス
/// @note 再帰的なツリーノード. 自身が直接所有するエンティティを平坦マップで保持し、
///       子Assemblyを通じて入れ子構造を表現する. 一つのエンティティは厳密に一つの
///       Assemblyに所属する (ツリー所有). ルートAssemblyのみが逆引きインデックスを
///       保持し、構造編集時に更新する.
class Assembly : public std::enable_shared_from_this<Assembly> {
 private:
    /// @brief AssemblyのID
    ObjectID id_ = IDGenerator::Generate(ObjectType::kAssembly);

    /// @brief このノードが直接所有するエンティティのIDとポインタのマッピング
    std::unordered_map<ObjectID, std::shared_ptr<entities::EntityBase>> entities_;

    /// @brief 子Assemblyのリスト (物理的な所有)
    std::vector<std::shared_ptr<Assembly>> children_;

    /// @brief 親Assemblyへの弱参照 (ルートの場合は空)
    std::weak_ptr<Assembly> parent_;

    /// @brief このノードの大域変換 (Solid Assembly Type 184相当の配置)
    igesio::Matrix4d global_transform_ = igesio::Matrix4d::Identity();

    /// @brief メタ情報
    AssemblyMetadata metadata_;

    /// @brief 全子孫エンティティから所有Assemblyへの逆引き
    /// @note ルートAssemblyのみが有効な内容を保持する. 非ルートのノードでは参照されない.
    std::unordered_map<ObjectID, Assembly*> entity_index_;

    /// @brief ルートノードの生ポインタを取得する (非const版)
    /// @note 親をたどって最上位のノードを返す. shared_ptr管理でなくても動作する.
    Assembly* RootRaw();
    /// @brief ルートノードの生ポインタを取得する (const版)
    const Assembly* RootRaw() const;

    /// @brief 逆引きインデックスにエンティティを登録する
    /// @param id 登録するエンティティのID
    /// @param owner そのエンティティを所有するAssembly
    void RegisterInIndex(const ObjectID&, Assembly*);

    /// @brief 自身と全子孫のエンティティを、指定ルートの逆引きインデックスへ再登録する
    /// @param root 登録先となるルートAssembly
    void ReindexInto(Assembly*);

    /// @brief ポインタを設定する
    /// @param[out] entity ポインタを設定するエンティティ
    /// @note entityが未登録のポインタを持ち、かつentities_がそれを持つ場合、
    ///       entityにそのポインタを設定する. 対象はこのノードのentities_のみ.
    void SetPointerIfUnset(std::shared_ptr<entities::EntityBase>);

 public:
    /// @brief AssemblyのIDを取得する
    /// @return AssemblyのID
    const ObjectID& GetID() const { return id_; }



    /**
     * エンティティの管理 (IgesDataから移設. 対象はこのノードのentities_のみ)
     */

    /// @brief エンティティを追加する
    /// @param entity 追加するエンティティ
    /// @return 追加に成功した場合は、そのエンティティのIDを返す.
    /// @throw std::invalid_argument entityがnullptrの場合
    template<typename T>
    std::enable_if_t<std::is_base_of_v<entities::EntityBase, T>, ObjectID>
    AddEntity(const std::shared_ptr<T> entity) {
        // nullptrチェック
        if (!entity) {
            throw std::invalid_argument("Entity pointer is null");
        }

        // エンティティをマップに追加
        auto id = entity->GetID();
        SetPointerIfUnset(entity);
        entities_[id] = entity;
        // ルートの逆引きインデックスへ登録
        RegisterInIndex(id, this);

        return id;
    }

    /// @brief エンティティが参照する全てのエンティティのポインタが設定済みか
    /// @return 一つでも未設定のポインタがある場合は`false`
    /// @note Directory Entry フィールド関連のメンバも含む. このノードのみを対象とする.
    bool AreAllReferencesSet() const;

    /// @brief エンティティが参照する全てのエンティティのうち、
    ///        ポインタが未設定のもののIDを取得する
    /// @return ポインタが未設定のエンティティのIDのリスト
    /// @note Directory Entry フィールド関連のメンバも含む. このノードのみを対象とする.
    std::unordered_set<ObjectID> GetUnresolvedReferences() const;

    /// @brief エンティティのポインタを取得する
    /// @param id エンティティのID
    /// @return 指定されたIDのエンティティのポインタ. 存在しない場合は`nullptr`.
    /// @note このノードが直接所有するエンティティのみを対象とする.
    std::shared_ptr<entities::EntityBase> GetEntity(const ObjectID&) const;

    /// @brief エンティティへの参照の取得
    /// @return このノードが直接所有するエンティティのポインタのマップ
    const std::unordered_map<ObjectID, std::shared_ptr<entities::EntityBase>>&
    GetEntities() const { return entities_; }

    /// @brief エンティティの数を取得する
    /// @return このノードが直接所有するエンティティの数
    size_t GetEntityCount() const { return entities_.size(); }

    /// @brief すべてのエンティティの準備ができているかを確認する
    /// @note (1) すべてのエンティティのポインタが設定済みであること
    ///       (2) すべてのエンティティが有効であること
    /// @return すべてのエンティティが準備できている場合は`true`, そうでない場合は`false`.
    /// @note このノードのみを対象とする (非再帰).
    bool IsReady() const;

    /// @brief 本Assemblyが有効かを確認する
    /// @return 検証結果
    /// @note 以下を検証する (このノードのみを対象とする. 非再帰):
    ///       - 未設定の参照がないこと
    ///       - すべてのエンティティが有効であること
    ValidationResult Validate() const;



    /**
     * ツリー・メタ情報・変換
     */

    /// @brief 子Assemblyを追加する
    /// @param child 追加する子Assembly
    /// @throw std::invalid_argument childがnullptrの場合
    /// @note childの親を自身に設定し、childとその子孫のエンティティをルートの逆引き
    ///       インデックスへ登録する.
    void AddChildAssembly(const std::shared_ptr<Assembly>&);

    /// @brief 子Assemblyのリストを取得する
    /// @return 子Assemblyのリスト
    const std::vector<std::shared_ptr<Assembly>>& GetChildAssemblies() const {
        return children_;
    }

    /// @brief 親Assemblyへの弱参照を取得する
    /// @return 親への弱参照. ルートの場合は空.
    std::weak_ptr<Assembly> GetParent() const { return parent_; }

    /// @brief ルートAssemblyを取得する
    /// @return 親をたどった最上位のAssembly
    /// @note shared_ptrで管理されている必要がある.
    std::shared_ptr<Assembly> Root();

    /// @brief 大域変換を取得する
    /// @return 大域変換行列
    const igesio::Matrix4d& GetGlobalTransform() const { return global_transform_; }

    /// @brief 大域変換を設定する
    /// @param transform 設定する大域変換行列
    void SetGlobalTransform(const igesio::Matrix4d& transform) {
        global_transform_ = transform;
    }

    /// @brief メタ情報を取得する (変更可)
    /// @return メタ情報への参照
    AssemblyMetadata& Metadata() { return metadata_; }
    /// @brief メタ情報を取得する (変更不可)
    /// @return メタ情報への参照
    const AssemblyMetadata& Metadata() const { return metadata_; }



    /**
     * 子要素のクエリ
     */

    /// @brief 指定IDのエンティティを所有するAssemblyを取得する
    /// @param id エンティティのID
    /// @return 所有するAssembly. 見つからない場合は`nullptr`.
    /// @note ルートの逆引きインデックスを参照する (O(1)).
    Assembly* FindOwner(const ObjectID&) const;

    /// @brief 所有するエンティティのIDを取得する
    /// @param recursive trueの場合は全子孫を含める (デフォルト: false)
    /// @return エンティティIDのリスト
    std::vector<ObjectID> GetEntityIDs(bool recursive = false) const;

    /// @brief 指定タイプのエンティティを取得する
    /// @param type エンティティのタイプ
    /// @param recursive trueの場合は全子孫を含める (デフォルト: false)
    /// @return 該当するエンティティのリスト
    std::vector<std::shared_ptr<entities::EntityBase>>
    FindEntitiesByType(entities::EntityType type, bool recursive = false) const;

    /// @brief 指定のEntityUseFlagを持つエンティティを取得する
    /// @param flag エンティティの用途フラグ
    /// @param recursive trueの場合は全子孫を含める (デフォルト: false)
    /// @return 該当するエンティティのリスト
    std::vector<std::shared_ptr<entities::EntityBase>>
    FindEntitiesByUseFlag(entities::EntityUseFlag flag,
                          bool recursive = false) const;

    /// @brief 述語に合致するエンティティを取得する
    /// @param predicate エンティティを受け取り、合致する場合にtrueを返す述語
    /// @param recursive trueの場合は全子孫を含める (デフォルト: false)
    /// @return 該当するエンティティのリスト
    std::vector<std::shared_ptr<entities::EntityBase>>
    FindEntities(const std::function<bool(const entities::EntityBase&)>& predicate,
                 bool recursive = false) const;



    /**
     * 座標系・ビュー・空間検索
     */

    /// @brief エンティティの配置行列を解決する
    /// @param id エンティティのID
    /// @param frame 取得したいフレーム (CoordFrame::World()やRelativeTo()等で生成)
    /// @return M_entity適用後の値に後掛けする配置行列. idがツリーに存在しない場合、
    ///         またはframeがkDefinition(配置概念なし)の場合は`std::nullopt`
    /// @throw std::invalid_argument frameがRelativeToで、基準が所有Assemblyの祖先で
    ///        ない場合
    /// @note 戻り値の配列を`ICurve::TryGetPointAt(t, placement)`等に渡すと、当該フレーム
    ///       での座標が得られる
    std::optional<igesio::Matrix4d>
    ResolvePlacement(const ObjectID& id, const CoordFrame& frame) const;

    /// @brief 指定IDの曲線について、指定フレームの変換ビューを生成する
    /// @param id エンティティのID
    /// @param frame 取得したいフレーム (CoordFrame::World()やRelativeTo()等で生成)
    /// @return 生成したCurveView. idがツリーに存在しない、または対象が曲線でない場合は
    ///         `nullptr`
    /// @throw std::invalid_argument frameがkDefinitionの場合 (ビューはM_entity適用が前提)、
    ///        またはRelativeToの基準が所有Assemblyの祖先でない場合
    std::shared_ptr<entities::CurveView>
    GetCurveView(const ObjectID& id, const CoordFrame& frame) const;

    /// @brief 指定IDの曲面について、指定フレームの変換ビューを生成する
    /// @param id エンティティのID
    /// @param frame 取得したいフレーム (CoordFrame::World()やRelativeTo()等で生成)
    /// @return 生成したSurfaceView. idがツリーに存在しない、または対象が曲面でない場合は
    ///         `nullptr`
    /// @throw std::invalid_argument frameがkDefinitionの場合 (ビューはM_entity適用が前提)、
    ///        またはRelativeToの基準が所有Assemblyの祖先でない場合
    std::shared_ptr<entities::SurfaceView>
    GetSurfaceView(const ObjectID& id, const CoordFrame& frame) const;

    /// @brief 子孫の幾何メンバを包含するワールド空間のバウンディングボックスを取得する
    /// @return 軸平行なバウンディングボックス. 幾何メンバがない、または全体が退化(点・
    ///         直線状)してAABBを構成できない場合は`std::nullopt`
    /// @note ワールド空間(このノードを含むルートまでの全大域変換を適用)で計算する.
    ///       幾何かつ物理従属でないメンバのみを対象とし、退化BBは既存ガードに倣って除外する.
    std::optional<numerics::BoundingBox> GetWorldBoundingBox() const;
};

}  // namespace igesio::models

#endif  // IGESIO_MODELS_ASSEMBLY_H_
