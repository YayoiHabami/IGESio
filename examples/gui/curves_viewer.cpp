/**
 * @file examples/gui/curves_viewer.cpp
 * @brief IgesViewerGUIを使用して曲線関連のエンティティを表示するサンプル
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 */
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>

#include <igesio/entities/curves/circular_arc.h>
#include <igesio/entities/curves/composite_curve.h>
#include <igesio/entities/curves/conic_arc.h>
#include <igesio/entities/curves/copious_data.h>
#include <igesio/entities/curves/linear_path.h>
#include <igesio/entities/curves/line.h>
#include <igesio/entities/curves/rational_b_spline_curve.h>
#include <igesio/entities/structures/color_definition.h>
#include <igesio/entities/transformations/transformation_matrix.h>

// Include this file before other OpenGL/GLFW headers
// (This file includes 'glad/glad.h' and 'GLFW/glfw3.h')
#include "./iges_viewer_gui.h"



namespace {

using Matrix3d = igesio::Matrix3d;
using Vector3d = igesio::Vector3d;
using Vector2d = igesio::Vector2d;
namespace i_ent = igesio::entities;
using igesio::graphics::IgesViewerGUI;

}  // namespace


// IgesViewerGUIを継承したクラス
class CurvesViewerGUI : public IgesViewerGUI {
 private:
    // エンティティタイプごとのエンティティリスト
    std::map<i_ent::EntityType, std::vector<std::shared_ptr<i_ent::EntityBase>>> entities_;
    // エンティティタイプごとの表示フラグ
    std::map<i_ent::EntityType, bool> show_entity_;
    // 全てのエンティティを表示するフラグ
    bool show_all_ = true;

    // エンティティの表示を更新する関数
    void UpdateEntities() {
        for (auto& p : show_entity_) {
            i_ent::EntityType type = p.first;
            bool show = p.second;
            if (show) {
                for (auto& entity : entities_[type]) {
                    Renderer().AddEntity(entity);
                }
            } else {
                for (auto& entity : entities_[type]) {
                    Renderer().RemoveEntity(entity->GetID());
                }
            }
        }
        needs_redraw_ = true;
    }

 public:
    // コンストラクタ
    explicit CurvesViewerGUI(int width = 1280, int height = 720)
        : IgesViewerGUI(width, height) {
        // 初期化時に全てのエンティティを表示
        for (auto& p : show_entity_) {
            p.second = true;
        }
    }

    // エンティティを追加する関数 (SetupViewerで使用)
    void AddEntity(std::shared_ptr<i_ent::EntityBase> entity) {
        i_ent::EntityType type = entity->GetType();
        entities_[type].push_back(entity);
        show_entity_[type] = true;  // 初期で表示
        Renderer().AddEntity(entity);
    }

    // RenderControlsをオーバーライド
    void RenderControls() override {
        ImGui::Begin("Controls");
        ImGui::Text("Camera");
        ImGui::Text("  - Drag Left Mouse: Rotate");
        ImGui::Text("  - Drag Right Mouse: Pan");
        ImGui::Text("  - Mouse Wheel: Zoom");
        ImGui::Separator();

        // カメラの位置とターゲットを表示
        auto cam_pos = renderer_.Camera().GetPosition();
        auto cam_target = renderer_.Camera().GetTarget();
        ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                    cam_pos[0], cam_pos[1], cam_pos[2]);
        ImGui::Text("Camera Target: (%.2f, %.2f, %.2f)",
                    cam_target[0], cam_target[1], cam_target[2]);

        // カメラの操作
        if (ImGui::Button("Reset Camera")) {
            renderer_.Camera().Reset();
            needs_redraw_ = true;
        }
        ImGui::Separator();

        if (ImGui::ColorEdit3("Background",
                              renderer_.GetBackgroundColorRef().data())) {
            needs_redraw_ = true;
        }
        ImGui::Separator();

        // エンティティ表示のコントロール
        ImGui::Text("Entity Visibility");
        bool show_all_changed = ImGui::Checkbox("Show All Entities", &show_all_);
        if (show_all_changed) {
            for (auto& p : show_entity_) {
                p.second = show_all_;
            }
            UpdateEntities();
        }
        ImGui::Separator();

        // 各エンティティタイプのトグル
        for (auto& p : show_entity_) {
            i_ent::EntityType type = p.first;
            bool& show = p.second;
            if (ImGui::Checkbox(ToString(type).c_str(), &show)) {
                if (!show_all_) {  // show_all_がオフの場合のみ個別制御
                    UpdateEntities();
                } else {
                    // show_all_がオンの場合、個別トグルを変更したらshow_all_をオフに
                    show_all_ = false;
                    UpdateEntities();
                }
            }
        }

        ImGui::End();
    }
};



void SetupViewer(CurvesViewerGUI& viewer) {
    // 描画したいエンティティを設定
    // 例: 半径2.0の円を作成
    auto my_curve = std::make_shared<i_ent::CircularArc>(
            Vector2d{0.0, 0.0}, 2.0, 1.0, 2.15);
    my_curve->OverwriteColor(i_ent::ColorNumber::kBlack);
    viewer.AddEntity(my_curve);

    auto my_circle = std::make_shared<i_ent::CircularArc>(
            Vector2d{0.0, 0.0}, 1.2);
    my_circle->OverwriteColor(i_ent::ColorNumber::kBlue);
    viewer.AddEntity(my_circle);

    auto trans_circle = std::make_shared<i_ent::CircularArc>(
            Vector2d{0.0, 0.0}, 1.1);
    trans_circle->OverwriteColor(i_ent::ColorNumber::kMagenta);
    auto trans_matrix = std::make_shared<i_ent::TransformationMatrix>(
            igesio::AngleAxisd(igesio::kPi / 4, Vector3d::UnitX()),
            Vector3d{0, 0, 0.5});
    trans_circle->OverwriteTransformationMatrix(trans_matrix);
    viewer.AddEntity(trans_circle);

    // 複合曲線を作成
    auto curve1 = std::make_shared<i_ent::CircularArc>(
            Vector2d{0.0, 0.0}, Vector2d{1.0, 0.0}, Vector2d{0.0, 1.0});
    auto curve2 = std::make_shared<i_ent::CircularArc>(
            Vector2d{0.0, 1.5}, Vector2d{0.0, 1.0}, Vector2d{-0.5, 1.5});
    auto curve3 = std::make_shared<i_ent::CircularArc>(
            Vector2d{1.5, 1.5}, Vector2d{-0.5, 1.5}, Vector2d{3.5, 1.5});
    auto composite_curve = std::make_shared<i_ent::CompositeCurve>();
    composite_curve->AddCurve(curve1);
    composite_curve->AddCurve(curve2);
    composite_curve->AddCurve(curve3);
    viewer.AddEntity(composite_curve);

    // ColorDefinitionエンティティを作成
    auto color_def = std::make_shared<i_ent::ColorDefinition>(
            std::array<double, 3>{50.0, 100.0, 30.0}, "hello");
    composite_curve->OverwriteColor(color_def);

    // ConicArcを作成
    auto conic_arc = std::make_shared<i_ent::ConicArc>(
            std::pair<double, double>{1.0, 0.5},
            0.0, igesio::kPi * 1.25, 2.0);
    viewer.AddEntity(conic_arc);

    // CopiousDataを作成
    igesio::Matrix3Xd coords(3, 4);
    coords << 0.0, 1.0,  1.0,  0.0,
              0.0, 0.0, -1.0,  2.0,
              0.0, 3.0,  0.0, -1.0;
    auto copious_data = std::make_shared<i_ent::CopiousData>(
            i_ent::CopiousDataType::kPoints3D, coords);
    viewer.AddEntity(copious_data);

    auto linear_path = std::make_shared<i_ent::LinearPath>(
            i_ent::CopiousDataType::kPolyline3D, coords);
    viewer.AddEntity(linear_path);

    // 線分/半直線を作成
    auto segment = std::make_shared<i_ent::Line>(
            Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 0.5, 0.5},
            i_ent::LineType::kSegment);
    viewer.AddEntity(segment);

    auto ray = std::make_shared<i_ent::Line>(
            Vector3d{-1.0, 0.0, 0.0}, Vector3d{2.0, 1.0, 0.5},
            i_ent::LineType::kRay);
    segment->OverwriteColor(i_ent::ColorNumber::kRed);
    viewer.AddEntity(ray);

    // NURBS曲線を作成
    auto param = igesio::IGESParameterVector{
        3,  // 次数
        3,  // 制御点の数-1
        false, false, false, false,  // 非周期の開NURBS曲線
        0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0,  // ノットベクトル
        1.0, 1.0, 1.0, 1.0,  // 重み
        -4.0, -4.0,  0.0,    // 制御点 P(0)
        -1.5,  7.0,  3.5,    // 制御点 P(1)
         4.0, -3.0,  1.0,    // 制御点 P(2)
         4.0,  4.0,  0.0,    // 制御点 P(3)
        0.0, 1.0,            // パラメータ範囲 V(0), V(1)
        0.0, 0.0, 1.0        // 定義平面の法線ベクトル
    };
    auto nurbs_c = std::make_shared<i_ent::RationalBSplineCurve>(
        i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kRationalBSplineCurve),
        param);
    nurbs_c->OverwriteColor(i_ent::ColorNumber::kCyan);
    nurbs_c->SetLineWeightNumber(2);
    viewer.AddEntity(nurbs_c);
}

int main() {
    try {
        auto viewer = CurvesViewerGUI(1280, 720);
        SetupViewer(viewer);

        // イベントループを開始
        viewer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
