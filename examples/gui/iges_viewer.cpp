/**
 * @file examples/gui/curves_viewer.cpp
 * @brief IgesViewerGUIを使用して曲線関連のエンティティを表示するサンプル
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 */
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

/// @brief 現在時刻を文字列で取得する関数
/// @param フォーマット (デフォルト: yyyy-mm-dd hhmmss)
/// @return フォーマットに従った現在時刻の文字列
std::string CurrentTimeString(const std::string& format = "%Y-%m-%d %H%M%S") {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    // std::localtimeは非スレッドセーフかつMSVCで非推奨警告となるため、
    // プラットフォーム毎の安全な版を使う (引数順がWindows/POSIXで異なる点に注意)
    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &now_c);  // MSVCは(tm*, time_t*)の順
#else
    localtime_r(&now_c, &local_time);  // POSIXは(time_t*, tm*)の順
#endif

    // フォーマットに従って文字列を生成
    std::stringstream ss;
    ss << std::put_time(&local_time, format.c_str());
    return ss.str();
}

}  // namespace


// IgesViewerGUIを継承したクラス
class ExampleIGESViewer : public IgesViewerGUI {
 private:
    // エンティティタイプごとのエンティティリスト
    std::map<i_ent::EntityType, std::vector<std::shared_ptr<i_ent::EntityBase>>> entities_;
    // エンティティタイプごとの表示フラグ
    std::map<i_ent::EntityType, bool> show_entity_;
    // 全てのエンティティを表示するフラグ
    bool show_all_ = true;
    // IGESデータ
    i_mod::IgesData iges_data_;
    // 最初から読み込むIGESファイル
    std::string initial_iges_file_;

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
            const auto& iges_entities = iges_data_.Root().GetEntities();
            for (const auto& pair : iges_entities) {
                AddEntity(pair.second);
            }

            // シーンをモデルのルートへ束ね、描画時にツリーを走査させる
            // (可視/抑制サブツリーのスキップ・変換/色のリフレッシュ).
            // 再読込のたびにrootが変わるため毎回束ね直す (選択はリセットされる)
            BindSceneRoot(iges_data_.RootPtr());

            // カメラをリセット
            Renderer().Camera().Reset();
            needs_redraw_ = true;
        } catch (const std::exception& e) {
            std::cerr << "Error loading IGES file: " << e.what() << std::endl;
        }
    }

 public:
    // コンストラクタ
    explicit ExampleIGESViewer(
        int width = 1280, int height = 720,
        int msaa_samples = 0,
        const std::string& initial_iges_file = "")
        : IgesViewerGUI(width, height, msaa_samples),
          initial_iges_file_(initial_iges_file) {
        // 初期化時に全てのエンティティを表示状態に設定
        for (auto& p : show_entity_) {
            p.second = true;
        }

        // 透明なオブジェクトの描画を有効化する
        renderer_.EnableTransparency(true);
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

        i_ent::EntityType type = entity->GetType();
        if (entity->GetSubordinateEntitySwitch() ==
            i_ent::SubordinateEntitySwitch::kPhysicallyDependent) {
            // すでに親エンティティに追加されている可能性があるのでスキップ
            return;
        } else if (type == i_ent::EntityType::kTransformationMatrix ||
                   type == i_ent::EntityType::kColorDefinition) {
            return;
        }

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
    // (親の共通コントロールを描画した上で、本サンプル固有の項目を追記する)
    void RenderControls() override {
        // 親 (共通) のControlsパネル (カメラ情報・操作説明・選択一覧等) を描画
        IgesViewerGUI::RenderControls();

        // 本サンプル固有のコントロールを同じControlsパネルへ追記する
        // (Beginを同名で再度呼ぶと既存のウィンドウへ追記される)
        ImGui::Begin("Controls");
        ImGui::Separator();

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

        // スクリーンショットボタン
        if (ImGui::Button("Capture Screenshot")) {
            CaptureScreenshot("screenshot " + CurrentTimeString() + ".png");
        }
        ImGui::Separator();

        // IGESファイル読み込みボタン
        static char filename_buf[256] = "";
        if (initial_iges_file_ != "") {
            // ファイルの初期値が設定されている場合は、一度だけそれをセットする
            strcpy_s(filename_buf, initial_iges_file_.c_str());
            initial_iges_file_ = "";
            LoadIgesFile(filename_buf);
        }
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

/// @brief コマンドライン引数から取得したデータ
struct CommandLineOptions {
    /// @brief ヘルプ表示フラグ
    bool show_help = false;
    /// @brief IGESファイルのパス
    std::string iges_file;
    /// @brief MSAAのサンプル数
    /// @note 0で無効
    int msaa_samples = 0;
};

/// @brief コマンドライン引数を解析する関数
/// @param argc 引数の数
/// @param argv 引数の配列
/// @return 解析結果の構造体
CommandLineOptions ParseCommandLine(int argc, char** argv) {
    CommandLineOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0]
                      << " [-h|--help] [FILE=<IGES file path>] [MSAA=<samples>]"
                      << std::endl
                      << "  - h, --help: Show this help message" << std::endl
                      << "  - FILE: Path to the IGES file to load" << std::endl
                      << "  - MSAA: Number of samples for MSAA " << std::endl
                      << "    (Antialiasing; 0 to disable, default: 0)" << std::endl;
            options.show_help = true;
            return options;
        } else if (arg.rfind("FILE=", 0) == 0) {
            options.iges_file = arg.substr(5);
        } else if (arg.rfind("MSAA=", 0) == 0) {
            try {
                options.msaa_samples = std::stoi(arg.substr(5));
                if (options.msaa_samples < 0) {
                    throw std::invalid_argument("Negative value");
                }
            } catch (const std::exception&) {
                std::cerr << "Invalid MSAA value. It should be a non-negative integer."
                          << std::endl;
                return options;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return options;
        }
    }

    return options;
}



int main(int argc, char** argv) {
    // コマンドライン引数の解析
    auto options = ParseCommandLine(argc, argv);
    if (options.show_help) {
        return 0;
    }

    try {
        // ウィンドウサイズ 1280x720
        // エイリアシングのサンプル数 0 (無効)
        auto viewer = ExampleIGESViewer(
                1280, 720, options.msaa_samples,
                options.iges_file);

        // イベントループを開始
        viewer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
