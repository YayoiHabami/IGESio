/**
 * @file tests/entities/views/views_for_testing.h
 * @brief entities/views配下のテストで使用するモック曲線・曲面と補助関数
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 * @note CurveView/SurfaceViewはICurve/ISurfaceのみに依存するため、既知の定義空間形状と
 *       既知のM_entity(protected Transform)を仕込んだモックを検証の土台とする.
 *       これにより`placement·(M_entity·P_def)`の合成順序と、点/ベクトルの区別を厳密に
 *       検算できる.
 */
#ifndef IGESIO_TESTS_ENTITIES_VIEWS_VIEWS_FOR_TESTING_H_
#define IGESIO_TESTS_ENTITIES_VIEWS_VIEWS_FOR_TESTING_H_

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/matrix.h"
#include "igesio/numerics/bounding_box.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"



namespace igesio::tests {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;

/// @brief 円周率 π/2 (モック形状の導関数位相に使用)
constexpr double kPiHalf = 1.57079632679489661923;

/// @brief 既知の4x4同次変換行列を生成する
/// @param angle 回転角度 [rad]
/// @param axis 回転軸 (正規化前で可。ゼロベクトル不可)
/// @param translation 並進ベクトル
/// @return 回転R(angle,axis)と並進Tからなる同次変換行列
/// @note angleが0の場合は回転を単位行列とする
inline igesio::Matrix4d MakeTransform(const double angle,
                                      const igesio::Vector3d& axis,
                                      const igesio::Vector3d& translation) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    if (angle != 0.0) {
        m.topLeftCorner<3, 3>() =
            igesio::AngleAxisd(angle, axis.normalized()).toRotationMatrix();
    }
    m.topRightCorner<3, 1>() = translation;
    return m;
}



/// @brief 検証用のモック曲線
/// @note 定義空間では z=0平面上の単位円 C_def(t) = (cos t, sin t, 0) を表す.
///       これにより点・接線・法線・従法線がすべて閉形式で定まる. protected `Transform`
///       には既知のM_entity(回転+並進)を仕込み、定義空間と実空間の差からM_entityの
///       適用順序を検算できるようにする.
class MockCurve : public i_ent::ICurve {
    /// @brief 自身のID (kEntityNewとして採番)
    igesio::ObjectID id_;
    /// @brief 仕込むM_entity (protected Transformが参照する)
    igesio::Matrix4d m_entity_;
    /// @brief パラメータ範囲 (既定: 第一象限の四分円。閉じていない)
    std::array<double, 2> range_;

 public:
    /// @brief コンストラクタ
    /// @param m_entity 仕込むM_entity (既定: 単位行列)
    /// @param range パラメータ範囲 (既定: [0, π/2])
    explicit MockCurve(
            const igesio::Matrix4d& m_entity = igesio::Matrix4d::Identity(),
            const std::array<double, 2>& range = {0.0, kPiHalf})
            : m_entity_(m_entity), range_(range) {
        id_ = igesio::IDGenerator::Generate(
            igesio::ObjectType::kEntityNew,
            static_cast<uint16_t>(i_ent::EntityType::kCircularArc));
    }
    /// @brief デストラクタ
    ~MockCurve() override { igesio::IDGenerator::Release(id_.ToInt()); }

    /// @brief k階導関数 C^(k)(t) = (cos(t+kπ/2), sin(t+kπ/2), 0) を返す
    static igesio::Vector3d DefinedDerivative(const double t,
                                              const unsigned int k) {
        const double phase = t + static_cast<double>(k) * kPiHalf;
        return igesio::Vector3d(std::cos(phase), std::sin(phase), 0.0);
    }

    const igesio::ObjectID& GetID() const override { return id_; }
    i_ent::EntityType GetType() const override {
        return i_ent::EntityType::kCircularArc;
    }
    int GetFormNumber() const override { return 0; }

    i_num::BoundingBox GetDefinedBoundingBox() const override {
        // 内容は任意(デコレータの変換適用のみを検証するため)。有効な3Dボックスを返す
        return i_num::BoundingBox(igesio::Vector3d(-1.0, -1.0, -1.0),
                                  std::array<double, 3>{2.0, 2.0, 2.0});
    }

    bool IsClosed() const override { return false; }
    std::array<double, 2> GetParameterRange() const override { return range_; }

    /// @brief 基底の配置適用版を可視化する(名前隠蔽の回避)
    using i_ent::ICurve::TryGetDerivatives;
    std::optional<i_ent::CurveDerivatives>
    TryGetDerivatives(const double t, const unsigned int n) const override {
        if (t < range_[0] || t > range_[1]) return std::nullopt;
        i_ent::CurveDerivatives result(n);
        for (unsigned int k = 0; k <= n; ++k) {
            result[k] = DefinedDerivative(t, k);
        }
        return result;
    }

 protected:
    std::optional<igesio::Vector3d> Transform(
            const std::optional<igesio::Vector3d>& input,
            const bool is_point) const override {
        return i_num::ApplyTransform(m_entity_, input, is_point);
    }
};



/// @brief 検証用のモック曲面
/// @note 定義空間では z=0平面 S_def(u,v) = (u, v, 0) を表す.
///       Su=(1,0,0), Sv=(0,1,0), 法線=(0,0,1) がすべて閉形式で定まる.
///       protected `Transform`には既知のM_entityを仕込む.
class MockSurface : public i_ent::ISurface {
    /// @brief 自身のID
    igesio::ObjectID id_;
    /// @brief 仕込むM_entity
    igesio::Matrix4d m_entity_;
    /// @brief パラメータ範囲 {u_min, u_max, v_min, v_max}
    std::array<double, 4> range_;

 public:
    /// @brief コンストラクタ
    /// @param m_entity 仕込むM_entity (既定: 単位行列)
    /// @param range パラメータ範囲 (既定: [0,1]x[0,1])
    explicit MockSurface(
            const igesio::Matrix4d& m_entity = igesio::Matrix4d::Identity(),
            const std::array<double, 4>& range = {0.0, 1.0, 0.0, 1.0})
            : m_entity_(m_entity), range_(range) {
        id_ = igesio::IDGenerator::Generate(
            igesio::ObjectType::kEntityNew,
            static_cast<uint16_t>(i_ent::EntityType::kRationalBSplineSurface));
    }
    /// @brief デストラクタ
    ~MockSurface() override { igesio::IDGenerator::Release(id_.ToInt()); }

    const igesio::ObjectID& GetID() const override { return id_; }
    i_ent::EntityType GetType() const override {
        return i_ent::EntityType::kRationalBSplineSurface;
    }
    int GetFormNumber() const override { return 0; }

    i_num::BoundingBox GetDefinedBoundingBox() const override {
        return i_num::BoundingBox(igesio::Vector3d(-1.0, -1.0, -1.0),
                                  std::array<double, 3>{2.0, 2.0, 2.0});
    }

    bool IsUClosed() const override { return false; }
    bool IsVClosed() const override { return false; }
    std::array<double, 4> GetParameterRange() const override { return range_; }

    /// @brief 基底の配置適用版を可視化する(名前隠蔽の回避)
    using i_ent::ISurface::TryGetDerivatives;
    std::optional<i_ent::SurfaceDerivatives>
    TryGetDerivatives(const double u, const double v,
                      const unsigned int order) const override {
        if (u < range_[0] || u > range_[1] ||
            v < range_[2] || v > range_[3]) {
            return std::nullopt;
        }
        i_ent::SurfaceDerivatives result(order);
        result(0, 0) = igesio::Vector3d(u, v, 0.0);
        if (order >= 1) {
            result(1, 0) = igesio::Vector3d(1.0, 0.0, 0.0);
            result(0, 1) = igesio::Vector3d(0.0, 1.0, 0.0);
        }
        // 2階以上はゼロベクトル(デフォルト)
        return result;
    }

 protected:
    std::optional<igesio::Vector3d> Transform(
            const std::optional<igesio::Vector3d>& input,
            const bool is_point) const override {
        return i_num::ApplyTransform(m_entity_, input, is_point);
    }
};

}  // namespace igesio::tests

#endif  // IGESIO_TESTS_ENTITIES_VIEWS_VIEWS_FOR_TESTING_H_
