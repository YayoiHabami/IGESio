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
#include <string>
#include <utility>
#include <vector>

#include <igesio/entities/curves/circular_arc.h>
#include <igesio/entities/curves/composite_curve.h>
#include <igesio/entities/curves/conic_arc.h>
#include <igesio/entities/curves/copious_data.h>
#include <igesio/entities/curves/linear_path.h>
#include <igesio/entities/curves/line.h>
#include <igesio/entities/curves/rational_b_spline_curve.h>
#include <igesio/entities/structures/color_definition.h>
#include <igesio/entities/transformations/transformation_matrix.h>
#include <igesio/reader.h>

// Include this file before other OpenGL/GLFW headers
// (This file includes 'glad/glad.h' and 'GLFW/glfw3.h')
#include "./iges_viewer_gui.h"



namespace {

using Matrix3d = igesio::Matrix3d;
using Vector3d = igesio::Vector3d;
using Vector2d = igesio::Vector2d;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
namespace i_graph = igesio::graphics;
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
    // IGESデータ
    i_mod::IgesData iges_data_;

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

    // IGESファイルを読み込む関数
    void LoadIgesFile(const std::string& filename) {
        try {
            iges_data_ = igesio::ReadIges(filename);

            // 既存のエンティティをクリア
            for (auto& p : entities_) {
                for (auto& entity : p.second) {
                    Renderer().RemoveEntity(entity->GetID());
                }
            }
            entities_.clear();
            show_entity_.clear();

            // IGESファイルからエンティティを取得して追加
            const auto& iges_entities = iges_data_.GetEntities();
            for (const auto& pair : iges_entities) {
                AddEntity(pair.second);
            }

            // カメラをリセット
            Renderer().Camera().Reset();
            needs_redraw_ = true;
        } catch (const std::exception& e) {
            std::cerr << "Error loading IGES file: " << e.what() << std::endl;
        }
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
        if (!entity->IsSupported()) {
            std::cerr << "Entity type " << ToString(entity->GetType())
                      << " is not supported." << std::endl;
            return;
        } else if (auto result = entity->Validate(); !result.is_valid) {
            std::cerr << "Entity " << entity->GetID() << " is invalid: "
                      << result.Message() << std::endl;
            return;
        }

        if (entity->GetSubordinateEntitySwitch() ==
            i_ent::SubordinateEntitySwitch::kPhysicallyDependent) {
            // すでに親エンティティに追加されている可能性があるのでスキップ
            return;
        } else if (entity->GetType() == i_ent::EntityType::kTransformationMatrix ||
                   entity->GetType() == i_ent::EntityType::kColorDefinition) {
            return;
        }

        i_ent::EntityType type = entity->GetType();
        if (!Renderer().AddEntity(entity)) {
            std::cerr << "Failed to add entity " << entity->GetID()
                      << " (type " << ToString(type)
                      << ") to renderer." << std::endl;
            return;
        }

        entities_[type].push_back(entity);
        show_entity_[type] = true;  // 初期で表示
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

        // カメラの投影モード切り替え
        ImGui::Text("Projection Mode");
        int current_mode = static_cast<int>(renderer_.Camera().GetProjectionMode());
        if (ImGui::RadioButton("Perspective", &current_mode,
                static_cast<int>(i_graph::ProjectionMode::kPerspective))) {
            renderer_.Camera().SetProjectionMode(i_graph::ProjectionMode::kPerspective);
            needs_redraw_ = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Orthographic", &current_mode,
                static_cast<int>(i_graph::ProjectionMode::kOrthographic))) {
            renderer_.Camera().SetProjectionMode(i_graph::ProjectionMode::kOrthographic);
            needs_redraw_ = true;
        }

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

        // IGESファイル読み込みボタン
        static char filename_buf[256] = "";  // ファイル名入力バッファ
        ImGui::InputText("IGES File", filename_buf, IM_ARRAYSIZE(filename_buf));
        if (ImGui::Button("Load IGES File")) {
            LoadIgesFile(filename_buf);
        }
        ImGui::Separator();
        // エンティティ表示のコントロール
        ImGui::Text("Entity Visibility");
        if (ImGui::Checkbox("Show All Entities", &show_all_)) {
            for (auto& p : show_entity_) p.second = show_all_;
            UpdateEntities();
        }
        ImGui::Separator();

        // 各エンティティタイプのトグル
        bool individual_toggle_changed = false;
        for (auto& p : show_entity_) {
            i_ent::EntityType type = p.first;
            bool& show = p.second;
            if (ImGui::Checkbox(ToString(type).c_str(), &show)) {
                individual_toggle_changed = true;
                // "Show All"がオンの状態で個別のチェックを外した場合、"Show All"をオフにする
                if (show_all_ && !show) show_all_ = false;
            }
        }

        if (individual_toggle_changed) {
            // 全ての個別のトグルがオンになっているかチェック
            bool all_individual_on = true;
            if (show_entity_.empty()) {
                all_individual_on = false;
            } else {
                for (const auto& p : show_entity_) {
                    if (!p.second) {
                        all_individual_on = false;
                        break;
                    }
                }
            }

            // 全てオンなら "Show All" もオンにする
            if (all_individual_on) show_all_ = true;

            UpdateEntities();
        }

        ImGui::End();
    }
};

int main() {
    try {
        auto viewer = CurvesViewerGUI(1280, 720);

        // イベントループを開始
        viewer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
