/**
 * @file extensions/inspection/coordinate_frame_group.h
 * @brief 座標系群を保持する非IGESエンティティ (INSPECTION用)
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note IGESへはシリアライズされない描画専用のエンティティ. 原点と直交3軸からなる
 *       座標系を複数本まとめて保持し、点・各軸を色分けして描画させるために用いる.
 */
#ifndef IGESIO_EXTENSIONS_INSPECTION_COORDINATE_FRAME_GROUP_H_
#define IGESIO_EXTENSIONS_INSPECTION_COORDINATE_FRAME_GROUP_H_

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/entities/non_iges_entity_base.h"
#include "igesio/entities/interfaces/i_geometry.h"

/// @brief INSPECTION系の表示専用エンティティを束ねる拡張サブモジュール
namespace igesio::extensions::inspection {

/// @brief 1つの座標系 (原点＋直交する3軸の単位方向)
/// @note x_axis・y_axis・z_axisは単位ベクトルかつ直交していることを想定する
///       (z_axis = x_axis × y_axis). 描画時は各軸を原点からaxis_size倍した
///       線分として表示する.
struct CoordinateFrame {
    /// @brief 座標系の原点 (モデル空間)
    Vector3d origin = Vector3d::Zero();
    /// @brief X軸の単位方向
    Vector3d x_axis = Vector3d::UnitX();
    /// @brief Y軸の単位方向
    Vector3d y_axis = Vector3d::UnitY();
    /// @brief Z軸の単位方向
    Vector3d z_axis = Vector3d::UnitZ();
};

/// @brief 生成する座標系群の表示属性 (色・サイズ)
/// @note 既定値はCoordinateFrameGroupの既定と揃える (点=白, X=赤, Y=緑, Z=青)
struct CoordinateFrameStyle {
    /// @brief 点の色 (RGBA; [0,1])
    std::array<float, 4> point_color = {1.0f, 1.0f, 1.0f, 1.0f};
    /// @brief X軸 (A方向) の色 (RGBA; [0,1])
    std::array<float, 4> x_color = {1.0f, 0.2f, 0.2f, 1.0f};
    /// @brief Y軸 (B方向) の色 (RGBA; [0,1])
    std::array<float, 4> y_color = {0.2f, 1.0f, 0.2f, 1.0f};
    /// @brief Z軸 (法線方向) の色 (RGBA; [0,1])
    std::array<float, 4> z_color = {0.3f, 0.4f, 1.0f, 1.0f};
    /// @brief 点の表示径 [px]
    double point_size = 2.0;
    /// @brief 軸の表示長 [モデル長]
    double axis_size = 5.0;
    /// @brief 軸の表示線幅 [px]
    double axis_width = 2.0;
};

/// @brief 座標系群を保持する非IGESエンティティ
/// @note `EntityType::kNonIges`としてAssemblyへ保存でき、IGES出力 (WriteIges) からは
///       スキップされる. 描画は拡張同梱の`CoordinateFrameGroupGraphics`が
///       GraphicsRegistry経由で担う (IGESIO_ENABLE_GRAPHICS時. 登録は
///       `RegisterCoordinateFrameGroupGraphics()`で行う).
/// @note 変換行列は持たない (恒等). 配置はAssemblyの大域変換で行う.
/// @note IGeometryのみを実装するためピッキング対象にはならない
///       (CanIntersectはISurface/ICurveまたはPickRegistry登録時のみtrue).
/// @note 固有IDの二重解放を避けるため、コピー不可・ムーブ可とする (NonIgesEntityBaseの規約).
class CoordinateFrameGroup
        : public entities::NonIgesEntityBase, public entities::IGeometry {
 public:
    /// @brief コンストラクタ
    /// @param frames 保持する座標系の列 (省略時は空)
    explicit CoordinateFrameGroup(std::vector<CoordinateFrame> frames = {})
        : frames_(std::move(frames)) {}

    /**
     * 座標系のアクセサ
     */

    /// @brief 保持する座標系の列を取得する
    const std::vector<CoordinateFrame>& Frames() const { return frames_; }

    /// @brief 座標系の列を差し替える
    /// @param frames 新しい座標系の列
    /// @note 形状編集としてジオメトリリビジョンをバンプする
    void SetFrames(std::vector<CoordinateFrame> frames) {
        frames_ = std::move(frames);
        MarkGeometryModified();
    }

    /// @brief 座標系を1つ追加する
    /// @param frame 追加する座標系
    /// @note 形状編集としてジオメトリリビジョンをバンプする
    void AddFrame(const CoordinateFrame& frame) {
        frames_.push_back(frame);
        MarkGeometryModified();
    }

    /**
     * 表示属性のアクセサ
     */

    /// @brief 点の色 (RGBA; [0,1]) を取得する
    const std::array<float, 4>& PointColor() const { return style_.point_color; }
    /// @brief X軸の色 (RGBA; [0,1]) を取得する
    const std::array<float, 4>& XColor() const { return style_.x_color; }
    /// @brief Y軸の色 (RGBA; [0,1]) を取得する
    const std::array<float, 4>& YColor() const { return style_.y_color; }
    /// @brief Z軸の色 (RGBA; [0,1]) を取得する
    const std::array<float, 4>& ZColor() const { return style_.z_color; }
    /// @brief 点の表示径 [px] を取得する
    double PointSize() const { return style_.point_size; }
    /// @brief 軸の表示長 [モデル長] を取得する
    double AxisSize() const { return style_.axis_size; }
    /// @brief 軸の表示線幅 [px] を取得する
    double AxisWidth() const { return style_.axis_width; }

    /// @brief 点の色を設定する (RGBA; [0,1])
    void SetPointColor(const std::array<float, 4>& c) { style_.point_color = c; }
    /// @brief X軸の色を設定する (RGBA; [0,1])
    void SetXColor(const std::array<float, 4>& c) { style_.x_color = c; }
    /// @brief Y軸の色を設定する (RGBA; [0,1])
    void SetYColor(const std::array<float, 4>& c) { style_.y_color = c; }
    /// @brief Z軸の色を設定する (RGBA; [0,1])
    void SetZColor(const std::array<float, 4>& c) { style_.z_color = c; }
    /// @brief 点の表示径 [px] を設定する
    void SetPointSize(const double s) { style_.point_size = s; }
    /// @brief 軸の表示線幅 [px] を設定する
    /// @note 線幅はバウンディングボックス・頂点バッファに影響しないため
    ///       形状リビジョンはバンプしない
    void SetAxisWidth(const double w) { style_.axis_width = w; }
    /// @brief 軸の表示長 [モデル長] を設定する
    /// @note 表示長はバウンディングボックスに影響するため形状リビジョンをバンプする
    void SetAxisSize(const double s);
    /// @brief 表示属性をまとめて設定する
    /// @param style 設定する表示属性
    /// @note 軸の表示長等が変更された場合は形状リビジョンをバンプする
    void SetStyle(const CoordinateFrameStyle& style);

    /**
     * IGeometry実装
     */

    /// @brief 定義空間におけるバウンディングボックスを取得する
    /// @return 全座標系の原点と各軸の先端 (原点 + 軸·axis_size) を包含する
    ///         軸平行バウンディングボックス. 座標系が無い場合は空のBoundingBox
    numerics::BoundingBox GetDefinedBoundingBox() const override;

 protected:
    /// @brief 座標orベクトルを変換する (恒等)
    /// @param input 変換前の座標orベクトル
    /// @param is_point 座標を変換する場合はtrue
    /// @return 入力をそのまま返す (変換行列を持たないため)
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input, bool is_point) const override;

 private:
    /// @brief 保持する座標系の列
    std::vector<CoordinateFrame> frames_;

    /// @brief 表示スタイル
    CoordinateFrameStyle style_;
};



/**
 * ファクトリ関数
 */

/// @brief 新しい座標系群エンティティを生成する
/// @param frames 保持する座標系の列 (省略時は空)
/// @param style 座標系の表示属性 (省略時は規定値)
/// @return 生成された座標系群エンティティ
std::shared_ptr<CoordinateFrameGroup> MakeCoordinateFrameGroup(
        std::vector<CoordinateFrame> frames = {},
        const CoordinateFrameStyle& style = {});

}  // namespace igesio::extensions::inspection

#endif  // IGESIO_EXTENSIONS_INSPECTION_COORDINATE_FRAME_GROUP_H_
