/**
 * @file graphics/shaders.h
 * @brief シェーダープログラムをまとめる
 * @author Yayoi Habami
 * @date 2025-08-07
 * @copyright 2025 Yayoi Habami
 * @note このヘッダーファイルは、シェーダープログラムのソースコードを
 *       まとめるために使用する. 各シェーダーコードは、①C++の文字列リテラルとして
 *       直接定義する方法か、②外部のGLSLファイルをインクルードする方法で
 *       定義できる. どちらの方法でも、呼び出しは`graphics/shaders/xxx.h`の
 *       中で行う. 各`xxx.h`においては、xxx（例えばcurves, surfacesなど）に
 *       関連するシェーダーコードを定義し、`GetXXXShaderCode`関数を通じて
 *       取得できるようにする. ①の指定方法では、`glsl/`で始まるシェーダーコードの
 *       相対パスを指定する. ②の指定方法では、実際にGLSLコードを文字列リテラルとして
 *       定義する.
 * @note C++ソース上にないシェーダーコードは、`src/graphics/shaders/glsl/`
 *       以下に保存されている. `#include "glsl/..."` の形で参照できるコードは、
 *       `src/graphics/shaders/glsl`フォルダ以下の各ファイル.
 *       例えば、commonフォルダ下の`nurbs_surface_prop.glsl`を参照する場合は、
 *       `#include "glsl/common/nurbs_surface_prop.glsl"`とする.
 *       この場合、この部分がそのままファイル内の文字列で置換される.
 * @note 使用可能なシェーダーと、その組み合わせは以下の通り (computeは非対応):
 *       (1) 2要素: vertex → fragment
 *       (2) 3要素: vertex → geometry → fragment
 *       (3) 4要素: vertex → tcs → tes → fragment
 *       (4) 5要素: vertex → tcs → tes → geometry → fragment
 */
#ifndef SRC_GRAPHICS_SHADERS_H_
#define SRC_GRAPHICS_SHADERS_H_

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/graphics/core/i_entity_graphics.h"
#include "./shaders/curves.h"
#include "./shaders/surfaces.h"

namespace igesio::graphics::shaders {

/// @brief インクルード部分の正規表現
/// @note match[0]が`#include "glsl/..."`全体、
///       match[1]が`glsl/...`の部分になる
constexpr char kIncludePattern[] = "#include\\s+\"(glsl/[^\"]+)\"";

/// @brief 相対パスからインクルード部分文字列を作成する
/// @param relative_path インクルードするファイルの相対パス
/// @return `#include "glsl/..."`の形の文字列
inline std::string MakeIncludeString(const std::string& relative_path) {
    return "#include \"" + relative_path + "\"";
}

/// @brief 置換用のシェーダーコード
struct IncludeShaderCode {
    /// @brief インクルードするファイルのパス
    std::filesystem::path path;
    /// @brief インクルード用のGLSLコード
    std::string glsl_code;

    /// @brief インクルード文を展開する
    /// @param relative_path インクルードするファイルの相対パス
    /// @param shader_code 展開するシェーダーコード (relative_pathのファイル内容)
    /// @note `#include "relative_path"` の部分をすべてshader_codeで置換する
    void ExpandInclude(const std::string& relative_path,
                       const std::string& shader_code) {
        std::string include_string = MakeIncludeString(relative_path);

        // すべてのinclude_stringをshader_codeで置換する
        size_t pos = glsl_code.find(include_string);
        while (pos != std::string::npos) {
            glsl_code.replace(pos, include_string.length(), shader_code);
            pos = glsl_code.find(include_string, pos + shader_code.length());
        }
    }

    /// @brief 展開が必要なソースの相対パスを取得する
    /// @return 展開が必要な相対パスのベクター
    std::vector<std::string> GetIncludePaths() const {
        std::vector<std::string> paths;
        std::regex include_regex(kIncludePattern);
        std::smatch match;
        std::string code_copy = glsl_code;  // コピーを使って検索

        while (std::regex_search(code_copy, match, include_regex)) {
            if (match.size() == 2) {
                paths.push_back(match[1].str());
                code_copy = match.suffix().str();  // 検索範囲を更新
            } else {
                break;  // 不正なマッチの場合は終了
            }
        }

        return paths;
    }
};

/// @brief すべてのシェーダーコードのインクルードを（再帰的に）展開する
/// @param shader_map インクルード用のGLSLコードのunordered_map
/// @return インクルードが展開されたGLSLコードのunordered_map
/// @throw igesio::FileError インクルードするファイルが見つからない場合
/// @throw igesio::ImplementationError インクルードの循環参照が発生した場合
/// @todo 同じソースに対して、複数回同じファイルが展開されないようにする
inline std::unordered_map<std::string, IncludeShaderCode>
ExpandAllIncludes(const std::unordered_map<std::string, IncludeShaderCode>& shader_map) {
    std::unordered_map<std::string, std::vector<std::string>> dependencies;
    // トポロジカルソートされたファイルリスト
    std::vector<std::string> sorted_files;
    // 訪問済みノードと再帰スタック
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursion_stack;

    // 依存関係グラフを構築
    for (const auto& [path, code] : shader_map) {
        dependencies[path] = code.GetIncludePaths();
    }

    // 循環参照を検出しながらトポロジカルソートを実行するDFS関数を
    // すべてのファイルに対して実行
    std::function<void(const std::string&)> dfs = [&](const std::string& file_path) {
        // 訪問済みなら何もしない
        if (visited.count(file_path)) return;

        // 現在のパスがシェーダーマップに存在しない場合はエラー
        if (shader_map.find(file_path) == shader_map.end()) {
            throw igesio::FileError("Include file not found: " + file_path);
        }
        visited.insert(file_path);
        recursion_stack.insert(file_path);

        // 依存ファイルを再帰的に探索
        for (const auto& dep_path : dependencies[file_path]) {
            // 再帰スタックに既に存在する場合、循環参照を検出
            if (recursion_stack.count(dep_path)) {
                throw igesio::ImplementationError("Circular include detected: " +
                        file_path + " -> " + dep_path);
            }
            dfs(dep_path);
        }

        // DFSの帰りがけにリストに追加（トポロジカルソート）
        recursion_stack.erase(file_path);
        sorted_files.push_back(file_path);
    };
    for (const auto& [path, _] : shader_map) {
        if (!visited.count(path)) dfs(path);
    }

    // トポロジカルソートされた順序でインクルードを展開
    // sorted_filesには依存関係の末端から順にファイルパスが格納されている
    std::unordered_map<std::string, IncludeShaderCode> expanded_map;
    for (const auto& file_to_process : sorted_files) {
        expanded_map[file_to_process] = shader_map.at(file_to_process);
        auto& current_shader = expanded_map.at(file_to_process);

        // このファイルがインクルードしている各ファイルについて展開処理を行う
        // 依存ファイルは既に展開済みのはず
        for (const auto& include_path : dependencies[file_to_process]) {
            const auto& included_shader = shader_map.at(include_path);
            current_shader.ExpandInclude(include_path, included_shader.glsl_code);
        }
    }

    return expanded_map;
}

/// @brief インクルード用のGLSLコードを取得する
/// @return IncludeShaderCodeのunordered_map.
///         キーは相対パス、値はIncludeShaderCode構造体
/// @note 相対パスは、glslフォルダをルートとした`glsl/common/nurbs_surface_prop.glsl`
///       のような文字列. この部分を含む行がファイル内の文字列で置換される.
/// @note プログラム起動時に一度だけ初期化される. 以降の呼び出しでは
///       その際に読み込まれたコードが返される.
inline const std::unordered_map<std::string, IncludeShaderCode>&
GetIncludeShaderCodes() {
    static std::unordered_map<std::string, IncludeShaderCode> codes;
    static std::once_flag init_flag;

    std::call_once(init_flag, [&]() {
        // このファイルの場所を基準にする
        auto root_path = std::filesystem::path(__FILE__).parent_path() /
                         "shaders/glsl";

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path)) {
            if (entry.is_regular_file()) {
                IncludeShaderCode code;

                // ファイル名からインクルード用文字列を生成
                auto relative_path = std::filesystem::relative(entry.path(), root_path);
                auto include_string = std::string("glsl/") + relative_path.generic_string();

                // GLSLコードを読み込み
                std::ifstream file(entry.path());
                if (file) {
                    // 読み込みに成功した場合のみ格納
                    code.path = entry.path();
                    code.glsl_code = std::string((std::istreambuf_iterator<char>(file)),
                                                  std::istreambuf_iterator<char>());
                    codes[include_string] = std::move(code);
                }
            }
        }

        // すべてのインクルードを展開
        codes = ExpandAllIncludes(codes);
    });

    return codes;
}

/// @brief シェーダーコード内のインクルード部分を展開する
/// @param shader_code インクルードを展開するシェーダーコード
/// @return インクルードが展開されたシェーダーコード
/// @throw igesio::FileError インクルードするファイルが見つからない場合
inline std::string ExpandShaderIncludes(const std::string& shader_code) {
    const auto& include_codes = GetIncludeShaderCodes();

    // shader_codeが相対パス（glsl/で始まる文字列）の場合は、
    // それに対応するコードを探し、それについて再度展開を行う
    // -> kRationalBSplineSurfaceShaderのように、一部のシェーダーは
    //    ファイルパスのみを指定しているため
    if (shader_code.rfind("glsl/", 0) == 0) {
        auto it = include_codes.find(shader_code);
        if (it != include_codes.end()) {
            return ExpandShaderIncludes(it->second.glsl_code);
        } else {
            throw igesio::FileError("Specified shader file not found: " + shader_code);
        }
    }

    // インクルード部分をすべて展開する
    IncludeShaderCode dummy_code;
    dummy_code.glsl_code = shader_code;
    for (const auto& path : dummy_code.GetIncludePaths()) {
        auto it = include_codes.find(path);
        if (it == include_codes.end()) {
            throw igesio::FileError("Included shader file not found: " + path);
        }
        dummy_code.ExpandInclude(path, it->second.glsl_code);
    }
    return dummy_code.glsl_code;
}

/// @brief ShaderCodeの全要素に対してインクルードを展開する
/// @param code インクルードを展開するShaderCode
/// @return インクルードが展開されたShaderCode
/// @throw igesio::FileError インクルードするファイルが見つからない場合
inline ShaderCode ExpandShaderCodeIncludes(const ShaderCode& code) {
    ShaderCode expanded_code = code;
    expanded_code.vertex = ExpandShaderIncludes(code.vertex);
    expanded_code.fragment = ExpandShaderIncludes(code.fragment);
    if (!code.geometry.empty()) {
        expanded_code.geometry = ExpandShaderIncludes(code.geometry);
    }
    if (!code.tcs.empty()) {
        expanded_code.tcs = ExpandShaderIncludes(code.tcs);
    }
    if (!code.tes.empty()) {
        expanded_code.tes = ExpandShaderIncludes(code.tes);
    }
    return expanded_code;
}

/// @brief シェーダーのソースコードを取得する
/// @param shader_type シェーダーのタイプ
/// @return シェーダーのソースコード、
///         指定されたタイプのシェーダーがない場合はnullopt
/// @throw igesio::FileError インクルードするファイルが見つからない場合
std::optional<ShaderCode> GetShaderCode(const ShaderType shader_type) {
    // 特定のシェーダーを持たないタイプの場合はnullopt
    if (!HasSpecificShaderCode(shader_type)) return std::nullopt;

    try {
        // 曲線シェーダーを優先して取得
        if (auto code = GetCurveShaderCode(shader_type)) {
            return ExpandShaderCodeIncludes(*code);
        }

        // なければ曲面シェーダーを取得
        if (auto code = GetSurfaceShaderCode(shader_type)) {
            return ExpandShaderCodeIncludes(*code);
        }
    } catch (const igesio::FileError& e) {
        auto suffix = " in shader type " + ToString(shader_type);
        throw igesio::FileError(std::string(e.what()) + suffix);
    }

    // 対応するシェーダーコードが見つからない場合はnullopt
    return std::nullopt;
}

}  // namespace igesio::graphics::shaders

#endif  // SRC_GRAPHICS_SHADERS_H_
