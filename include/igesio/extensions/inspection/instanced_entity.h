/**
 * @file extensions/inspection/instanced_entity.h
 * @brief 基準メンバ列を変換行列で複製表示する非IGESエンティティ (INSPECTION用)
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 * @note IGESへはシリアライズされない描画専用のエンティティ. アセンブリ単位の複製を
 *       想定し、複数の基準メンバ (各メンバ = 基準エンティティ + アセンブリ局所配置)
 *       と、複製先への4x4変換行列を複数本まとめて保持する.
 *       描画クラス (`InstancedEntityGraphics`) は各メンバを一度だけテッセレーション・
 *       GPU転送し、その同一メッシュを各変換行列の位置へ描き分ける. これにより、
 *       同一形状を多数複製しても構築コストはメンバ数ぶん (複製数には非依存) で済む.
 */
#ifndef IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_H_
#define IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/entities/non_iges_entity_base.h"
#include "igesio/entities/interfaces/i_geometry.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"

namespace igesio::models {
// Assembly (models/assembly.h で定義). MakeInstancedAssembly の引数型に用いる.
class Assembly;
}  // namespace igesio::models

namespace igesio::extensions::inspection {

/// @brief アセンブリ単位複製における1メンバ (基準エンティティ + 局所配置)
/// @note `local_placement` は入れ子Assemblyの大域変換を畳んだ「基準フレーム→メンバ
///       親フレーム」の配置行列. メンバ自身のDE変換 (M_entity, 定義空間→モデル空間)
///       は描画側が別途適用するため含めない. 各メンバの最終配置は
///       `(Assemblyの大域変換) · (複製先行列) · local_placement · M_entity`.
struct InstancedMember {
    /// @brief 複製の基準となるエンティティ (描画可能な型であること)
    std::shared_ptr<const entities::IEntityIdentifier> source;
    /// @brief アセンブリ局所フレーム内での配置行列 (既定: 恒等)
    igesio::Matrix4d local_placement = igesio::Matrix4d::Identity();
};

/// @brief 基準メンバ列を変換行列で複製表示する非IGESエンティティ
/// @note `EntityType::kNonIges`としてAssemblyへ保存でき、IGES出力 (WriteIges) からは
///       スキップされる. 描画は拡張同梱の`InstancedEntityGraphics`が
///       GraphicsRegistry経由で担う (IGESIO_ENABLE_GRAPHICS時. 登録は
///       `RegisterInstancedEntityGraphics()`で行う).
/// @note 効率の要点: 同一の基準形状の複製は、複製先行列を本エンティティ1つに
///       まとめて持たせること. そうすれば各メンバ形状のテッセレーション・GPUバッファは
///       メンバごとに1回だけ構築され、各複製は描画時にmodel行列を差し替えて同一メッシュを
///       使い回す. 複製ごとに本エンティティを分けて作ると基準形状の構築が
///       複製数ぶん走るため、効率上の利点が失われる.
/// @note 各複製の最終配置は `(Assemblyの大域変換) · (複製先行列) · local_placement ·
///       (メンバのDE変換)` となる. 複製先行列はメンバ配置に後掛けする配置行列であり、
///       メンバのDE変換 (定義空間→モデル空間) は描画側が内部で1回だけ適用する.
/// @note IGeometryのみを実装するためレイピッキング対象にはならない
///       (CanIntersectはISurface/ICurveまたはPickRegistry登録時のみtrue).
/// @note 固有IDの二重解放を避けるため、コピー不可・ムーブ可とする (NonIgesEntityBaseの規約).
class InstancedEntity
        : public entities::NonIgesEntityBase, public entities::IGeometry {
 public:
    /// @brief コンストラクタ (アセンブリ単位; 複数メンバ)
    /// @param members 複製の基準となるメンバ列 (各メンバ = 基準エンティティ + 局所配置)
    /// @param transforms 複製先への4x4変換行列の列 (省略時は空; 1複製ならば要素1つ)
    /// @throw std::invalid_argument いずれかのメンバのsourceがnullptrの場合
    explicit InstancedEntity(
            std::vector<InstancedMember> members,
            std::vector<igesio::Matrix4d> transforms = {});

    /// @brief コンストラクタ (単一エンティティ; 1メンバ・恒等配置へ委譲)
    /// @param source 複製の基準となるエンティティ (描画可能な型であること)
    /// @param transforms 複製先への4x4変換行列の列 (省略時は空; 1複製ならば要素1つ)
    /// @throw std::invalid_argument sourceがnullptrの場合
    explicit InstancedEntity(
            std::shared_ptr<const entities::IEntityIdentifier> source,
            std::vector<igesio::Matrix4d> transforms = {});

    /**
     * アクセサ
     */

    /// @brief 複製の基準となるメンバ列を取得する
    const std::vector<InstancedMember>& Members() const { return members_; }

    /// @brief メンバ数を取得する
    std::size_t MemberCount() const { return members_.size(); }

    /// @brief 複製先への変換行列の列を取得する
    const std::vector<igesio::Matrix4d>& Transforms() const {
        return transforms_;
    }

    /// @brief 複製数 (変換行列の本数) を取得する
    std::size_t InstanceCount() const { return transforms_.size(); }

    /// @brief 複製先への変換行列の列を差し替える
    /// @param transforms 新しい変換行列の列
    /// @note 配置の変更はバウンディングボックスに影響するため形状リビジョンをバンプする
    ///       (基準形状の再テッセレーションは発生しない)
    void SetTransforms(std::vector<igesio::Matrix4d> transforms) {
        transforms_ = std::move(transforms);
        MarkGeometryModified();
    }

    /// @brief 複製先への変換行列を1つ追加する
    /// @param transform 追加する4x4変換行列
    /// @note 形状リビジョンをバンプする
    void AddInstance(const igesio::Matrix4d& transform) {
        transforms_.push_back(transform);
        MarkGeometryModified();
    }

    /**
     * IGeometry実装
     */

    /// @brief 定義空間におけるバウンディングボックスを取得する
    /// @return 全複製 (各メンバの基準形状に局所配置・複製先行列を適用したもの) を
    ///         包含する軸平行バウンディングボックス. 幾何メンバが無い・複製が無い
    ///         場合は空のBoundingBox
    /// @note 本エンティティは変換行列を持たない (恒等) ため、定義空間のBBが
    ///       そのままモデル空間のBBとなる. Assemblyの大域変換は描画側が適用する.
    numerics::BoundingBox GetDefinedBoundingBox() const override;

 protected:
    /// @brief 座標orベクトルを変換する (恒等)
    /// @param input 変換前の座標orベクトル
    /// @param is_point 座標を変換する場合はtrue
    /// @return 入力をそのまま返す (本エンティティ自身は変換行列を持たないため.
    ///         複製先行列・局所配置は描画側がmodel行列として適用する)
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input, bool is_point) const override;

 private:
    /// @brief 複製の基準となるメンバ列 (各メンバ = 基準エンティティ + 局所配置)
    std::vector<InstancedMember> members_;

    /// @brief 複製先への4x4変換行列の列 (1複製につき1行列)
    std::vector<igesio::Matrix4d> transforms_;
};



/**
 * ファクトリ関数
 */

/// @brief 複製表示エンティティを生成する (アセンブリ単位; 複数メンバ)
/// @param members 複製の基準となるメンバ列
/// @param transforms 複製先への4x4変換行列の列
/// @return 生成された複製表示エンティティ
/// @throw std::invalid_argument いずれかのメンバのsourceがnullptrの場合
std::shared_ptr<InstancedEntity> MakeInstancedEntity(
        std::vector<InstancedMember> members,
        std::vector<igesio::Matrix4d> transforms = {});

/// @brief 複製表示エンティティを生成する (単一エンティティ・複数の複製先)
/// @param source 複製の基準となるエンティティ
/// @param transforms 複製先への4x4変換行列の列
/// @return 生成された複製表示エンティティ
/// @throw std::invalid_argument sourceがnullptrの場合
std::shared_ptr<InstancedEntity> MakeInstancedEntity(
        std::shared_ptr<const entities::IEntityIdentifier> source,
        std::vector<igesio::Matrix4d> transforms = {});

/// @brief 複製表示エンティティを生成する (単一エンティティ・単一の複製先)
/// @param source 複製の基準となるエンティティ
/// @param transform 複製先への4x4変換行列
/// @return 生成された複製表示エンティティ (複製先1つ)
/// @throw std::invalid_argument sourceがnullptrの場合
std::shared_ptr<InstancedEntity> MakeInstancedEntity(
        std::shared_ptr<const entities::IEntityIdentifier> source,
        const igesio::Matrix4d& transform);

/// @brief Assemblyサブツリーを平坦化して複製表示エンティティを生成する
/// @param template_root 複製の雛形となるAssembly (このノードを基準フレームとみなす)
/// @param transforms 複製先への4x4変換行列の列
/// @return 生成された複製表示エンティティ
/// @note `template_root`とその全子孫が直接所有する幾何エンティティ (IGeometry) を
///       メンバ化する. 各メンバのlocal_placementには、基準フレーム (template_root)
///       から所有ノードまでに介在する子Assemblyの大域変換の累積を畳み込む
///       (template_root自身の大域変換は基準フレームの原点として扱い、含めない).
///       幾何でない (純粋な定義・変換) エンティティは複製対象に含めない.
std::shared_ptr<InstancedEntity> MakeInstancedAssembly(
        const models::Assembly& template_root,
        std::vector<igesio::Matrix4d> transforms = {});

}  // namespace igesio::extensions::inspection

#endif  // IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_H_
