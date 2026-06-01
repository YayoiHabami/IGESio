/**
 * @file examples/gui/iges_viewer.cpp
 * @brief IgesViewerGUIの起動エントリ (コマンドライン引数の解析と起動のみ)
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 */
#include <iostream>
#include <stdexcept>
#include <string>

// Include this file before other OpenGL/GLFW headers
// (This file includes 'glad/gl.h' and 'GLFW/glfw3.h')
#include "./iges_viewer_gui.h"



namespace {

using igesio::graphics::IgesViewerGUI;

/// @brief コマンドライン引数から取得したデータ
struct CommandLineOptions {
    /// @brief ヘルプ表示フラグ
    bool show_help = false;
    /// @brief IGESファイルのパス
    std::string iges_file;
    /// @brief MSAAのサンプル数 (0で無効)
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

}  // namespace



int main(int argc, char** argv) {
    const auto options = ParseCommandLine(argc, argv);
    if (options.show_help) {
        return 0;
    }

    try {
        // ウィンドウサイズ 1280x720、MSAAサンプル数・初期ファイルは引数から
        IgesViewerGUI viewer(1280, 720, options.msaa_samples, options.iges_file);
        viewer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
