/**
 * @file extensions/inspection/instanced_entity.h
 * @brief 基準エンティティを変換行列で複製表示する非IGESエンティティ (INSPECTION用)
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 * @note IGESへはシリアライズされない描画専用のエンティティ. 1つの基準エンティティ
 *       (サーフェス等) と、複製先への4x4変換行列を複数本まとめて保持する.
 *       描画クラス (`InstancedEntityGraphics`) は基準エンティティを一度だけ
 *       テッセレーション・GPU転送し、その同一メッシュを各変換行列の位置へ
 *       描き分ける. これにより、同一形状を多数複製しても構築コストは1回で済む.
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

namespace igesio::extensions::inspection {

/// @brief 基準エンティティを変換行列で複製表示する非IGESエンティティ
/// @note `EntityType::kNonIges`としてAssemblyへ保存でき、IGES出力 (WriteIges) からは
///       スキップされる. 描画は拡張同梱の`InstancedEntityGraphics`が
///       GraphicsRegistry経由で担う (IGESIO_ENABLE_GRAPHICS時. 登録は
///       `RegisterInstancedEntityGraphics()`で行う).
/// @note 効率の要点: 同一の基準エンティティの複製は、複製先行列を本エンティティ1つに
///       まとめて持たせること. そうすれば基準形状のテッセレーション・GPUバッファは
///       1回だけ構築され、各複製は描画時にmodel行列を差し替えて同一メッシュを
///       使い回す. 複製ごとに本エンティティを分けて作ると基準形状の構築が
///       複製数ぶん走るため、効率上の利点が失われる.
/// @note 各複製の最終配置は `(Assemblyの大域変換) · (複製先行列) · (基準のDE変換)`
///       となる. 複製先行列は基準エンティティのDE変換に後掛けする配置行列であり、
///       基準のDE変換 (定義空間→モデル空間) は描画側が内部で1回だけ適用する.
/// @note IGeometryのみを実装するためレイピッキング対象にはならない
///       (CanIntersectはISurface/ICurveまたはPickRegistry登録時のみtrue).
/// @note 固有IDの二重解放を避けるため、コピー不可・ムーブ可とする (NonIgesEntityBaseの規約).
class InstancedEntity
        : public entities::NonIgesEntityBase, public entities::IGeometry {
 public:
    /// @brief コンストラクタ
    /// @param source 複製の基準となるエンティティ (描画可能な型であること)
    /// @param transforms 複製先への4x4変換行列の列 (省略時は空; 1複製ならば要素1つ)
    /// @throw std::invalid_argument sourceがnullptrの場合
    explicit InstancedEntity(
            std::shared_ptr<const entities::IEntityIdentifier> source,
            std::vector<igesio::Matrix4d> transforms = {});

    /**
     * アクセサ
     */

    /// @brief 複製の基準となるエンティティを取得する
    const std::shared_ptr<const entities::IEntityIdentifier>& Source() const {
        return source_;
    }

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
    /// @return 全複製 (各変換行列を適用した基準形状) を包含する軸平行バウンディング
    ///         ボックス. 基準がIGeometryでない場合や複製が無い場合は空のBoundingBox
    /// @note 本エンティティは変換行列を持たない (恒等) ため、定義空間のBBが
    ///       そのままモデル空間のBBとなる. Assemblyの大域変換は描画側が適用する.
    numerics::BoundingBox GetDefinedBoundingBox() const override;

 protected:
    /// @brief 座標orベクトルを変換する (恒等)
    /// @param input 変換前の座標orベクトル
    /// @param is_point 座標を変換する場合はtrue
    /// @return 入力をそのまま返す (本エンティティ自身は変換行列を持たないため.
    ///         複製先行列は描画側がmodel行列として適用する)
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input, bool is_point) const override;

 private:
    /// @brief 複製の基準となるエンティティ
    std::shared_ptr<const entities::IEntityIdentifier> source_;

    /// @brief 複製先への4x4変換行列の列 (1複製につき1行列)
    std::vector<igesio::Matrix4d> transforms_;
};



/**
 * ファクトリ関数
 */

/// @brief 複製表示エンティティを生成する (複数の複製先)
/// @param source 複製の基準となるエンティティ
/// @param transforms 複製先への4x4変換行列の列
/// @return 生成された複製表示エンティティ
/// @throw std::invalid_argument sourceがnullptrの場合
std::shared_ptr<InstancedEntity> MakeInstancedEntity(
        std::shared_ptr<const entities::IEntityIdentifier> source,
        std::vector<igesio::Matrix4d> transforms = {});

/// @brief 複製表示エンティティを生成する (単一の複製先)
/// @param source 複製の基準となるエンティティ
/// @param transform 複製先への4x4変換行列
/// @return 生成された複製表示エンティティ (複製先1つ)
/// @throw std::invalid_argument sourceがnullptrの場合
std::shared_ptr<InstancedEntity> MakeInstancedEntity(
        std::shared_ptr<const entities::IEntityIdentifier> source,
        const igesio::Matrix4d& transform);

}  // namespace igesio::extensions::inspection

#endif  // IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_H_
