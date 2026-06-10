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
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
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

/// @brief ロック状態 (選択・編集の可否)
/// @note 参照ジオメトリ(一時的な参照平面など)やユーザーロックの表現に用いる. 強制は
///       選択を行う層(対話ピック/セッション)が`SelectionSet`の外で行う(本構造体は意図のみ保持).
struct LockState {
    /// @brief 選択可能か (falseの場合はピック/選択の対象外)
    bool selectable = true;
    /// @brief 編集可能か (falseの場合は編集操作の対象外. 将来用)
    bool editable = true;
};

/// @brief Assemblyのメタ情報 (注釈・対話ロック)
/// @note 参照時にlive読みされる情報のみを持つ. 描画の派生キャッシュへ影響しないため、
///       mutable参照経由で自由に編集してよい (変更通知は不要).
struct AssemblyMetadata {
    /// @brief アセンブリの名前
    std::string name;
    /// @brief 役割を示すタグ (任意の分類用文字列)
    std::string role_tag;
    /// @brief ロック状態 (選択・編集の可否)
    LockState lock;
};

/// @brief 表示状態 (描画の派生キャッシュに影響する状態)
/// @note 変更はAssemblyのsetter経由でのみ行う (モデルリビジョンのバンプを内蔵).
struct DisplayState {
    /// @brief 可視性 (描画対象とするか)
    /// @note 表示トグル. 非表示でも論理的には存在する(BBox/検証/出力/クエリには含まれる).
    bool visible = true;
    /// @brief 抑制 (論理的にモデルから除外するか)
    /// @note visibleとは別概念. 抑制時は描画されず、BBox/検証/出力/クエリからも除外される
    ///       (子孫も連鎖). 描画条件は visible かつ !suppressed.
    bool suppressed = false;
    /// @brief 色のオーバーライド (RGB; [0, 1]). 未設定の場合はメンバの色を使用する
    std::optional<std::array<float, 3>> color_override;
    /// @brief 不透明度のオーバーライド ([0, 1]). 未設定の場合はメンバの値を使用する
    std::optional<float> opacity_override;
};

/// @brief 削除時に、他から参照されているエンティティの扱い
/// @note 自己完結の不変条件(各Assemblyが参照を解決できる)を保つための削除戦略.
enum class RemovalPolicy {
    /// @brief 参照が残るなら削除を拒否する (自己完結を保つ既定)
    kReject,
    /// @brief 参照元・物理従属子も連鎖削除する
    kCascade,
    /// @brief 参照を未解決のまま削除する (被参照のweak_ptrは自然失効)
    kOrphan,
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
    /// @note 値型は識別の基底`IEntityIdentifier`. IGESエンティティ(EntityBase)に
    ///       加えて、非IGESエンティティ(計算・描画専用のユーザー定義型)も保存できる.
    ///       IGES固有の機構(ポインタ解決・DEステータス・検証等)はEntityBaseへの
    ///       キャストで選別し、非EntityBaseには安全な既定挙動を与える
    std::unordered_map<ObjectID, std::shared_ptr<entities::IEntityIdentifier>>
            entities_;

    /// @brief 子Assemblyのリスト (物理的な所有)
    std::vector<std::shared_ptr<Assembly>> children_;

    /// @brief 親Assemblyへの弱参照 (ルートの場合は空)
    std::weak_ptr<Assembly> parent_;

    /// @brief このノードの大域変換 (Solid Assembly Type 184相当の配置)
    igesio::Matrix4d global_transform_ = igesio::Matrix4d::Identity();

    /// @brief メタ情報 (注釈・対話ロック)
    AssemblyMetadata metadata_;

    /// @brief 表示状態 (描画の派生キャッシュに影響する状態)
    DisplayState display_;

    /// @brief モデルリビジョン (ルートAssemblyのみが有効な値を保持する)
    /// @note 構造・変換・表示状態の変更毎にインクリメントされる.
    ///       レンダラはこの値の変化で再同期 (Reconcile) の要否を判定する.
    uint64_t revision_ = 0;

    /// @brief 全子孫エンティティから所有Assemblyへの逆引き
    /// @note ルートAssemblyのみが有効な内容を保持する. 非ルートのノードでは参照されない.
    std::unordered_map<ObjectID, Assembly*> entity_index_;

    /// @brief ルートノードの生ポインタを取得する (非const版)
    /// @note 親をたどって最上位のノードを返す. shared_ptr管理でなくても動作する.
    Assembly* RootRaw();
    /// @brief ルートノードの生ポインタを取得する (const版)
    const Assembly* RootRaw() const;

    /// @brief ルートのモデルリビジョンをインクリメントする
    /// @note 逆引きインデックスと同様にRootRaw()経由でルートへ集約する.
    void BumpRevision() { ++RootRaw()->revision_; }

    /// @brief 逆引きインデックスにエンティティを登録する
    /// @param id 登録するエンティティのID
    /// @param owner そのエンティティを所有するAssembly
    void RegisterInIndex(const ObjectID&, Assembly*);

    /// @brief 自身と全子孫のエンティティを、指定ルートの逆引きインデックスへ再登録する
    /// @param root 登録先となるルートAssembly
    /// @note 呼出元は編入先rootのモデルリビジョンをバンプすること. サブツリーを
    ///       別rootへ移す場合は離脱元rootをデタッチ前にバンプすること (デタッチ後は
    ///       BumpRevision()が新rootへ集約され、離脱元を観察するレンダラが同期されない).
    void ReindexInto(Assembly*);

    /// @brief ポインタを設定する
    /// @param[out] entity ポインタを設定するエンティティ
    /// @note entityが未登録のポインタを持ち、かつentities_がそれを持つ場合、
    ///       entityにそのポインタを設定する. 対象はこのノードのentities_のみ.
    /// @note ポインタ解決はIGESエンティティ固有の機構のため、entity・entities_の
    ///       要素ともEntityBaseへのキャストで選別する (非EntityBaseは対象外)
    void SetPointerIfUnset(const std::shared_ptr<entities::IEntityIdentifier>&);

    /// @brief エンティティを一括追加する実装 (AddEntitiesの共通本体)
    /// @param entities 追加するエンティティの配列
    /// @note EntityBase版とテンプレート版のAddEntitiesで共有する.
    ///       ポインタの一括解決はEntityBaseの要素のみを対象とする
    /// @note テンプレート版AddEntitiesから任意の要素型で実体化されるため、
    ///       ヘッダ内で定義する
    template <typename T>
    void AddEntitiesImpl(const std::vector<std::shared_ptr<T>>& entities) {
        // 事前にreserveしてリハッシュを抑える
        entities_.reserve(entities_.size() + entities.size());

        // まず全エンティティをマップとルート逆引きインデックスへ登録する
        for (const auto& entity : entities) {
            if (!entity) {
                throw std::invalid_argument("Entity pointer is null");
            }
            const auto id = entity->GetID();
            entities_[id] = entity;
            RegisterInIndex(id, this);
        }
        // 構造変更としてモデルリビジョンをバンプする (一括追加で1回)
        BumpRevision();

        // 全件登録後に参照解決を1回だけ行う (O(エンティティ数+参照数)).
        // 各エンティティの未解決参照のうち、このノードが持つものを解決することで、
        // 前方参照・後方参照の双方が一括で解決される. 1件ずつAddEntityを呼ぶ場合に
        // 生じる挿入ごとの全件走査 (O(N^2)) を回避するための経路.
        // ポインタ解決はIGESエンティティ(EntityBase)固有の機構のため、
        // 非EntityBaseの要素はスキップする
        for (const auto& [id, ent] : entities_) {
            const auto eb =
                    std::dynamic_pointer_cast<entities::EntityBase>(ent);
            if (!eb) continue;
            for (const auto& rid : eb->GetUnresolvedReferences()) {
                auto it = entities_.find(rid);
                if (it == entities_.end()) continue;
                if (auto ref = std::dynamic_pointer_cast<entities::EntityBase>(
                        it->second)) {
                    eb->SetUnresolvedReference(ref);
                }
            }
        }
    }

    /// @brief nodeが自身(this)のサブツリーに属すか (自身を含む)
    /// @param node 判定対象のノード
    /// @return nodeから親をたどってthisに到達できる場合はtrue
    bool IsInSubtree(const Assembly* node) const;

    /// @brief nodeからrootまで全ての lock.editable がtrueか
    /// @param node 起点ノード
    /// @return 経路上にeditable==falseが無ければtrue. nodeがnullptrならtrue.
    bool IsChainEditable(const Assembly* node) const;

    /// @brief 指定ノードのentities_とルート逆引きインデックスから1エンティティを除去する
    /// @param owner 対象エンティティを所有するノード
    /// @param id 除去するエンティティのID
    void EraseEntity(Assembly* owner, const ObjectID& id);

    /// @brief idを起点に、物理従属子と参照元の閉包を連鎖削除する (kCascade用)
    /// @param start 起点エンティティのID
    /// @note GetChildIDs(物理従属)とFindReferrers(参照元)をvisited管理で再帰展開する.
    ///       共有定義を指す参照元が多い場合は広範に削除されうる(設計§12.2 P9-7のリスク).
    void RemoveCascade(const ObjectID& start);

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
    /// @note IGESエンティティ(EntityBase)のほか、`IEntityIdentifier`を実装した
    ///       非IGESエンティティ(計算・描画専用のユーザー定義型)も追加できる.
    ///       非IGESエンティティはIGES出力(WriteIges)からスキップされる.
    /// @note ビュー(CurveView/SurfaceView)は型上は追加可能だが、スナップショット
    ///       意味論(生成時点の変換を固定)のため追加しないこと(非推奨)
    template<typename T>
    std::enable_if_t<std::is_base_of_v<entities::IEntityIdentifier, T>, ObjectID>
    AddEntity(const std::shared_ptr<T>& entity) {
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
        // 構造変更としてモデルリビジョンをバンプする
        BumpRevision();

        return id;
    }

    /// @brief 複数のエンティティを一括で追加する
    /// @param entities 追加するエンティティの配列
    /// @throw std::invalid_argument いずれかのエンティティがnullptrの場合
    /// @note 全エンティティをマップとルート逆引きインデックスへ登録した後、
    ///       参照解決を1回だけ行う. 1件ずつ`AddEntity`を呼ぶ場合に生じる挿入
    ///       ごとの全件走査 (O(N^2)) を回避し、O(エンティティ数+参照数)で完了する.
    ///       多数のエンティティをまとめて読み込む場合に使用する.
    void AddEntities(
            const std::vector<std::shared_ptr<entities::EntityBase>>&);

    /// @brief 複数のエンティティを一括で追加する (テンプレート版)
    /// @param entities 追加するエンティティの配列
    ///        (要素型はIEntityIdentifierを実装する任意の型)
    /// @throw std::invalid_argument いずれかのエンティティがnullptrの場合
    /// @note 非IGESエンティティを含む配列を一括追加する場合に使用する.
    ///       ポインタの一括解決はEntityBaseの要素のみを対象とする
    /// @note 波括弧初期化リスト (`AddEntities({a, b})` 等) はテンプレート引数を
    ///       推論できないため、EntityBase版のオーバーロードへ一意に解決される
    ///       (オーバーロード曖昧の構造的な回避)
    template <typename T, std::enable_if_t<
            std::is_base_of_v<entities::IEntityIdentifier, T> &&
            !std::is_same_v<T, entities::EntityBase>, int> = 0>
    void AddEntities(const std::vector<std::shared_ptr<T>>& entities) {
        AddEntitiesImpl(entities);
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
    std::shared_ptr<entities::IEntityIdentifier> GetEntity(const ObjectID&) const;

    /// @brief エンティティのポインタを指定型として取得する
    /// @tparam T 取得する型 (EntityBase・具象エンティティ・能力インターフェース等)
    /// @param id エンティティのID
    /// @return `dynamic_pointer_cast<T>`の結果.
    ///         存在しない場合・型が一致しない場合は`nullptr`.
    /// @note このノードが直接所有するエンティティのみを対象とする.
    template <typename T>
    std::shared_ptr<T> GetEntityAs(const ObjectID& id) const {
        return std::dynamic_pointer_cast<T>(GetEntity(id));
    }

    /// @brief エンティティへの参照の取得
    /// @return このノードが直接所有するエンティティのポインタのマップ
    const std::unordered_map<ObjectID,
                             std::shared_ptr<entities::IEntityIdentifier>>&
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
        BumpRevision();
    }

    /// @brief メタ情報 (注釈・対話ロック) を取得する (変更可)
    /// @return メタ情報への参照
    /// @note live読みされる情報のみのため自由に編集してよい (変更通知は不要).
    AssemblyMetadata& Metadata() { return metadata_; }
    /// @brief メタ情報 (注釈・対話ロック) を取得する (変更不可)
    /// @return メタ情報への参照
    const AssemblyMetadata& Metadata() const { return metadata_; }

    /// @brief 表示状態を取得する (変更不可)
    /// @return 表示状態への参照
    /// @note 変更はSetVisible等のsetter経由でのみ行う (モデルリビジョンのバンプを内蔵).
    const DisplayState& Display() const { return display_; }

    /// @brief 可視性を設定する
    /// @param visible 設定する可視性
    /// @note 値が変化したときのみモデルリビジョンをバンプする.
    void SetVisible(const bool visible) {
        if (display_.visible == visible) return;
        display_.visible = visible;
        BumpRevision();
    }

    /// @brief 抑制状態を設定する
    /// @param suppressed 設定する抑制状態
    /// @note 値が変化したときのみモデルリビジョンをバンプする.
    void SetSuppressed(const bool suppressed) {
        if (display_.suppressed == suppressed) return;
        display_.suppressed = suppressed;
        BumpRevision();
    }

    /// @brief 色のオーバーライドを設定する
    /// @param color 設定する色 (RGB; [0,1]). std::nulloptでオーバーライドを解除する
    /// @note 値が変化したときのみモデルリビジョンをバンプする.
    void SetColorOverride(const std::optional<std::array<float, 3>>& color) {
        if (display_.color_override == color) return;
        display_.color_override = color;
        BumpRevision();
    }

    /// @brief 不透明度のオーバーライドを設定する
    /// @param opacity 設定する不透明度 ([0,1]). std::nulloptで解除する
    /// @note 値が変化したときのみモデルリビジョンをバンプする.
    void SetOpacityOverride(const std::optional<float>& opacity) {
        if (display_.opacity_override == opacity) return;
        display_.opacity_override = opacity;
        BumpRevision();
    }

    /// @brief モデルリビジョンを取得する (ルートの値を返す)
    /// @return モデルリビジョン
    /// @note 構造・変換・表示状態の変更毎に単調増加する. レンダラはこの値の変化で
    ///       再同期 (Reconcile) の要否を判定する. リビジョン値は同一rootに対して
    ///       のみ比較可能.
    uint64_t Revision() const { return RootRaw()->revision_; }



    /**
     * 子要素のクエリ
     */

    /// @brief 指定IDのエンティティを所有するAssemblyを取得する
    /// @param id エンティティのID
    /// @return 所有するAssembly. 見つからない場合は`nullptr`.
    /// @note ルートの逆引きインデックスを参照する (O(1)).
    Assembly* FindOwner(const ObjectID&) const;

    /// @brief 指定IDのエンティティが実効的に選択可能か
    /// @param id エンティティのID
    /// @return 所有ノードからrootまで全ての lock.selectable がtrueならtrue.
    ///         ツリーに属さないIDは制限なし(true)とみなす.
    /// @note lock.selectableを祖先方向へANDで畳む. 強制はSelectionSetでなく本層
    ///       (または対話ピック/Scene)で行う方針に従う. v1はエンティティID対象.
    bool IsEffectivelySelectable(const ObjectID& id) const;

    /// @brief 指定IDのエンティティが実効的に編集可能か
    /// @param id エンティティのID
    /// @return 所有ノードからrootまで全ての lock.editable がtrueならtrue.
    ///         ツリーに属さないIDは制限なし(true)とみなす.
    /// @note IsEffectivelySelectableの編集版. 編集操作(削除等)のガードに用いる.
    bool IsEffectivelyEditable(const ObjectID& id) const;

    /// @brief 所有するエンティティのIDを取得する
    /// @param recursive trueの場合は全子孫を含める (デフォルト: false)
    /// @return エンティティIDのリスト
    std::vector<ObjectID> GetEntityIDs(bool recursive = false) const;

    /// @brief 指定タイプのエンティティを取得する
    /// @param type エンティティのタイプ
    /// @param recursive trueの場合は全子孫を含める (デフォルト: false)
    /// @return 該当するエンティティのリスト
    std::vector<std::shared_ptr<entities::IEntityIdentifier>>
    FindEntitiesByType(entities::EntityType type, bool recursive = false) const;

    /// @brief 指定のEntityUseFlagを持つエンティティを取得する
    /// @param flag エンティティの用途フラグ
    /// @param recursive trueの場合は全子孫を含める (デフォルト: false)
    /// @return 該当するエンティティのリスト
    /// @note 用途フラグはDEステータス由来のため、対象はIGESエンティティ
    ///       (EntityBase) のみ. 非IGESエンティティは含まれない
    std::vector<std::shared_ptr<entities::EntityBase>>
    FindEntitiesByUseFlag(entities::EntityUseFlag flag,
                          bool recursive = false) const;

    /// @brief 述語に合致するエンティティを取得する
    /// @param predicate エンティティを受け取り、合致する場合にtrueを返す述語
    /// @param recursive trueの場合は全子孫を含める (デフォルト: false)
    /// @return 該当するエンティティのリスト
    std::vector<std::shared_ptr<entities::IEntityIdentifier>>
    FindEntities(
            const std::function<bool(const entities::IEntityIdentifier&)>& predicate,
            bool recursive = false) const;



    /**
     * 編集・ライフサイクル (TODO 3-3)
     */

    /// @brief 指定IDを参照しているエンティティのIDを収集する (逆方向検索)
    /// @param id 参照されている側のエンティティのID
    /// @return idを参照する全エンティティのID (id自身は除く)
    /// @note ルート全体の子孫エンティティを走査する線形検索. 大規模モデルでは逆参照
    ///       インデックス化を将来検討する(設計§12.2 P9-7).
    std::vector<ObjectID> FindReferrers(const ObjectID& id) const;

    /// @brief 指定IDのエンティティを削除する
    /// @param id 削除するエンティティのID
    /// @param policy 参照が残る場合の扱い (既定: kReject)
    /// @return 削除した場合はtrue. 未存在/このサブツリー外/編集不可、または
    ///         kRejectで参照が残る場合はfalse(無変更)
    /// @note 所有ノードを逆引きで特定し、このノードのサブツリー内のみを対象とする.
    ///       削除後はルートの逆引きインデックスからも除去する. 描画への反映は
    ///       モデルリビジョン経由で自動検知されるため、呼び出し側の通知は不要.
    bool RemoveEntity(const ObjectID& id,
                      RemovalPolicy policy = RemovalPolicy::kReject);

    /// @brief 直接の子Assemblyを削除する
    /// @param child_id 削除する子AssemblyのID (直接の子のみ対象)
    /// @param policy 外部(サブツリー外)からの参照が残る場合の扱い (既定: kReject)
    /// @return 削除した場合はtrue. 直接の子に該当が無い/編集不可、または
    ///         kRejectで外部からの参照(inbound)が残る場合はfalse(無変更)
    /// @note サブツリー内の全エンティティをルート逆引きインデックスからも除去する.
    ///       kCascadeは外部の参照元もRemoveEntity(kCascade)で連鎖削除する.
    bool RemoveChildAssembly(const ObjectID& child_id,
                             RemovalPolicy policy = RemovalPolicy::kReject);

    /// @brief このノードの全エンティティと全子Assemblyを除去する (一斉リセット)
    /// @note 自ノード+全子孫のエンティティをルート逆引きインデックスからも除去する.
    ///       明示的なリセットのためロック(editable)は考慮しない.
    void Clear();

    /// @brief エンティティを別のAssemblyノードへ移動する
    /// @param id 移動するエンティティのID
    /// @param dest 移動先のAssembly (同一ルートであること)
    /// @throw std::invalid_argument destが同一ルートでない、またはidがツリーに無い場合
    /// @note entities_間の付け替えと逆引きownerの更新を行い、dest内で参照を張り直す.
    ///       移動は非破壊のためロックは考慮しない. 自己完結は
    ///       ValidateSelfContainedRecursive(後続)で検証する.
    void MoveEntityTo(const ObjectID& id, Assembly& dest);

    /// @brief 直接の子Assemblyを別のAssemblyノードへ移動する
    /// @param child_id 移動する子AssemblyのID (直接の子のみ対象)
    /// @param dest 移動先のAssembly (同一ルートであること)
    /// @throw std::invalid_argument destが同一ルートでない、child_idが直接の子でない、
    ///        またはdestが移動対象サブツリー内(循環)の場合
    /// @note children_の付け替えと親weak_ptr更新を行う. 同一ルートのため逆引きインデックス
    ///       (エンティティ→所有ノード)は不変.
    void MoveChildAssemblyTo(const ObjectID& child_id, Assembly& dest);

    /// @brief 自ノードと全子孫の可視性を一括設定する
    /// @param visible 設定する可視性
    void SetVisibleRecursive(bool visible);

    /// @brief 自ノードと全子孫の抑制状態を一括設定する
    /// @param suppressed 設定する抑制状態
    void SetSuppressedRecursive(bool suppressed);

    /// @brief 自ノードと全子孫の色オーバーライドを一括設定する
    /// @param color 設定する色 (RGB; [0,1]). std::nulloptでオーバーライドを解除する
    void SetColorOverrideRecursive(
            const std::optional<std::array<float, 3>>& color);

    /// @brief 自ノードと全子孫の不透明度オーバーライドを一括設定する
    /// @param opacity 設定する不透明度 ([0,1]). std::nulloptで解除する
    void SetOpacityOverrideRecursive(const std::optional<float>& opacity);

    /// @brief このノードの大域変換へ追加変換を合成する (親フレームでの後付け; 非再帰)
    /// @param transform 合成する変換 (124相当・スケールなし前提)
    /// @note 結果は global_transform_ = transform * global_transform_ (左から合成=親
    ///       フレームでの適用). 子孫へは描画ドライバの累積で自然に波及するため再帰しない(§9).
    void ComposeGlobalTransform(const igesio::Matrix4d& transform);

    /// @brief 各ノードが参照を自己完結的に解決できるかを再帰的に検証する
    /// @return 検証結果. いずれかのノードに未解決参照があればis_valid==false
    /// @note GetUnresolvedReferencesの再帰版. 各編集の事後条件(自己完結の不変条件)に用いる.
    ///       エンティティ個別の幾何検証は含めず、参照解決のみを対象とする.
    ValidationResult ValidateSelfContainedRecursive() const;



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

    /// @brief 子孫エンティティの遅延幾何キャッシュを並列に事前構築する
    /// @param recursive trueの場合は全子孫を含める (デフォルト: true)
    /// @note 各エンティティのEntityBase::PrepareGeometryCache()を1回ずつ呼ぶ
    ///       (TrimmedSurfaceの領域判定キャッシュ等). 重い遅延計算を描画/クエリ前に
    ///       まとめて済ませるための一括処理. 読み込み・構造編集 (キャッシュを無効化する
    ///       Set/Add/Remove系) が完了し、並列読み取りを始める前に1回呼ぶこと.
    ///       本メソッドは内部で全ワーカーを待ち合わせてから返る.
    void PrepareGeometryCaches(bool recursive = true) const;
};



/**
 * ファクトリ関数
 */

/// @brief Assemblyを作成する
/// @param name アセンブリ名 (Metadata().nameに設定する。省略時は空のまま)
/// @return 作成されたAssemblyのshared_ptr
/// @note Assemblyはenable_shared_from_thisを継承するため、必ずshared_ptrとして
///       生成する必要がある。本ファクトリはその規約を構造的に保証する
std::shared_ptr<Assembly> MakeAssembly(const std::string& name = "");

}  // namespace igesio::models

#endif  // IGESIO_MODELS_ASSEMBLY_H_
