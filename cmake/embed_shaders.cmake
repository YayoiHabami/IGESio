# ---------------------------------------------------------------
# GLSLシェーダーソースをC++文字列定数として埋め込む生成スクリプト
# ---------------------------------------------------------------
# `cmake -P` から実行される。列挙された各GLSLファイルを読み込み、
# キー (SHADER_ROOTからの相対パス) → ソーステキストのマップを返す
# 関数 GetEmbeddedShaderSources() を定義する.cppを生成する。
# ここで用いるキーがそのまま実行時の参照キーとなる。
#
# 引数 (いずれも -D で指定):
#   SHADER_ROOT  埋め込むGLSLの基準ディレクトリ (キーはここからの相対パス)
#   OUTPUT_FILE  生成する.cppの出力先パス
#   SHADER_KEYS  埋め込むGLSLのキー一覧 (SHADER_ROOTからの相対パス; CMakeリスト)

if(NOT DEFINED SHADER_ROOT OR NOT DEFINED OUTPUT_FILE OR NOT DEFINED SHADER_KEYS)
    message(FATAL_ERROR
        "embed_shaders.cmake: SHADER_ROOT, OUTPUT_FILE, SHADER_KEYS are required")
endif()

# 生文字列リテラルのデリミタ (通常のGLSLソースには出現し得ない文字列)
set(_delim "__IG_GLSL__")

# キーを安定順にソートし、不要な差分・再ビルドを避ける
list(SORT SHADER_KEYS)

set(_body "")
foreach(_key IN LISTS SHADER_KEYS)
    set(_abs "${SHADER_ROOT}/${_key}")
    # 列挙漏れ・配置ミスをビルド時に検出する
    if(NOT EXISTS "${_abs}")
        message(FATAL_ERROR
            "embed_shaders.cmake: shader source not found: ${_abs}\n"
            "  (CMakeのGRAPHICS_SHADER_SOURCESの列挙とファイル配置を確認すること)")
    endif()

    file(READ "${_abs}" _content)
    # 実行時読み込み (テキストモード) と挙動を揃え、生成物を環境非依存にするため
    # CRLFをLFへ正規化する
    string(REPLACE "\r\n" "\n" _content "${_content}")

    # 生文字列リテラルの終端がソース本文に出現しないことを保証する
    string(FIND "${_content}" ")${_delim}\"" _clash)
    if(NOT _clash EQUAL -1)
        message(FATAL_ERROR
            "embed_shaders.cmake: raw-string delimiter clash in ${_key}")
    endif()

    string(APPEND _body
        "    {\"${_key}\",\n"
        "     R\"${_delim}(${_content})${_delim}\"},\n")
endforeach()

# 生成する.cppの全体を組み立てる
set(_generated
"// このファイルはビルド時に自動生成される。直接編集しないこと。
// 生成元: cmake/embed_shaders.cmake (src/graphics/shaders/glsl/**を埋め込む)
#include \"graphics/shaders/embedded_sources.h\"

#include <string>
#include <unordered_map>

namespace igesio::graphics::shaders {

const std::unordered_map<std::string, std::string>& GetEmbeddedShaderSources() {
    static const std::unordered_map<std::string, std::string> kSources = {
${_body}    };
    return kSources;
}

}  // namespace igesio::graphics::shaders
")

file(WRITE "${OUTPUT_FILE}" "${_generated}")
