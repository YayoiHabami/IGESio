/**
 * @file examples/gui/machining_viewer.cpp
 * @brief 簡易CAM GUI (machines拡張の実用例) のエントリポイント
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 * @note 使い方:
 *       ```text
 *       machining_viewer [-h|--help] [PROJECT=<path>] [LIB=<dir>]... [MSAA=<n>]
 *       ```
 *       `LIB=`はライブラリ検索ディレクトリ (機械定義や工具ライブラリの`library`
 *       キーの相対パスの基準) で、複数指定できる. ビルド時に
 *       `MACHINING_VIEWER_DATA_DIR`が定義されていれば、その`examples/data`を
 *       検索ディレクトリの末尾に加える.
 */
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "./machining_viewer_gui.h"

namespace {

/// @brief コマンドライン引数から取得したデータ
struct CommandLineOptions {
    /// @brief ヘルプを表示して終了するか
    bool show_help = false;
    /// @brief 引数に誤りがあるか
    bool invalid = false;
    /// @brief プロジェクトファイルのパス
    std::filesystem::path project;
    /// @brief ライブラリ検索ディレクトリ (指定順)
    std::vector<std::filesystem::path> library_dirs;
    /// @brief MSAAのサンプル数 (0で無効)
    int msaa_samples = 4;
};

/// @brief 使い方を標準出力に出力する
/// @param program 実行ファイル名 (`argv[0]`)
void PrintUsage(const char* program) {
    std::cout << "Usage: " << program
              << " [-h|--help] [PROJECT=<path>] [LIB=<dir>]... [MSAA=<samples>]\n"
              << "  -h, --help : Show this help message\n"
              << "  PROJECT    : Path to the machining project (TOML) to load\n"
              << "  LIB        : Library search directory (repeatable)\n"
              << "  MSAA       : Number of samples for MSAA "
                 "(0 to disable, default: 4)\n";
}

/// @brief コマンドライン引数を解析する
/// @param argc 引数の数
/// @param argv 引数の配列
/// @return 解析結果 (誤りがあれば`invalid`を`true`にして標準エラーに理由を出力する)
CommandLineOptions ParseCommandLine(const int argc, char** argv) {
    CommandLineOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            options.show_help = true;
            return options;
        } else if (arg.rfind("PROJECT=", 0) == 0) {
            options.project = arg.substr(8);
        } else if (arg.rfind("LIB=", 0) == 0) {
            options.library_dirs.emplace_back(arg.substr(4));
        } else if (arg.rfind("MSAA=", 0) == 0) {
            try {
                options.msaa_samples = std::stoi(arg.substr(5));
                if (options.msaa_samples < 0) throw std::invalid_argument("negative");
            } catch (const std::exception&) {
                std::cerr << "Invalid MSAA value. It should be a non-negative integer."
                          << std::endl;
                options.invalid = true;
                return options;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            options.invalid = true;
            return options;
        }
    }
#ifdef MACHINING_VIEWER_DATA_DIR
    options.library_dirs.emplace_back(MACHINING_VIEWER_DATA_DIR);
#endif
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    const CommandLineOptions options = ParseCommandLine(argc, argv);
    if (options.show_help) return 0;
    if (options.invalid) return 1;
    try {
        igesio::graphics::MachiningViewerGUI viewer(
                1600, 900, options.msaa_samples, options.project,
                options.library_dirs);
        viewer.Run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
