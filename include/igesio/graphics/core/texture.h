/**
 * @file graphics/core/texture.h
 * @brief 描画用のテクスチャクラス
 * @author Yayoi Habami
 * @date 2025-10-10
 * @copyright 2025 Yayoi Habami
 * @note エンティティの面への画像の貼り付け、描画内容のキャプチャ
 *       (`EntityRenderer::CaptureScreenshot`)などに使用する
 */
#ifndef IGESIO_GRAPHICS_CORE_TEXTURE_H_
#define IGESIO_GRAPHICS_CORE_TEXTURE_H_

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>



namespace igesio::graphics {

/// @brief 描画用テクスチャクラス
/// @note メンバ関数などでは、左下を原点とする座標系を使用する.
///       例えば、(0,0)は左下、(width-1, height-1)は右上のピクセルを表す.
///       一方で、データは内部的に、左上→右上→...→右下の順に格納される.
///       これは、OpenGLのglTexImage2D関数がこの順序でデータを受け取るため.
/// @note クラス構造としてはどのようなサイズのテクスチャでも扱えるが、
///       実際の描画に使用する場合は、幅と高さが2のべき乗であることが望ましい.
///       例えば、256x256, 512x512, 1024x1024など.
/// @note 画像ファイルとの入出力を行う場合は、`IGESIO_ENABLE_TEXTURE_IO`を
///       定義してコンパイルすること. `igesio::graphics::LoadTextureFromFile`
///       および`igesio::graphics::SaveTextureToFile`関数が使用可能になる.
/// @note サーフェスのある点 S(u,v) におけるテクスチャの色は、元の色 (RGB, A)
///       とテクスチャの色 (rgb, a) を用いて、((1-a)*RGB + a*rgb, A) として
///       計算される. つまり、テクスチャのアルファ値が0の場合は元の色がそのまま
///       使用され、1の場合はテクスチャの色がそのまま使用される.
class Texture {
    /// @brief アルファ値を持つか
    /// @note trueの場合はRGBA, falseの場合はRGBとして扱う
    bool has_alpha_ = false;
    /// @brief データの格納順序
    /// @note trueの場合、データは左下→右下→...→右上の順 (bottom_up) に格納されているとみなす.
    ///       falseの場合、データは左上→右上→...→右下の順 (top_down) に格納されているとみなす.
    bool bottom_up_ = false;

    /// @brief テクスチャのサイズ (幅, 高さ)
    std::pair<unsigned int, unsigned int> size_ = {0, 0};

    /// @brief テクスチャデータ (各ピクセルはR, G, B, (A)の順に格納される)
    /// @note has_alpha_がtrueの場合は width * height * 4個の要素を持ち、
    ///       falseの場合は width * height * 3個の要素を持つ
    std::vector<unsigned char> data_;

    /// @brief テクスチャデータのインデックスを取得する
    /// @param x ピクセルのx座標 (0からwidth-1; 左下原点)
    /// @param y ピクセルのy座標 (0からheight-1; 左下原点)
    /// @return data_のインデックス
    /// @throw std::out_of_range 指定された座標が範囲外の場合
    size_t GetDataIndex(const unsigned int, const unsigned int) const;



 public:
    /// @brief デフォルトコンストラクタ (空のテクスチャを作成)
    Texture() = default;

    /// @brief コンストラクタ (指定されたサイズとデータでテクスチャを作成)
    /// @param width テクスチャの幅 (ピクセル数)
    /// @param height テクスチャの高さ (ピクセル数)
    /// @param has_alpha アルファ値を持つか (trueの場合はRGBA, falseの場合はRGBとして扱う)
    /// @param data テクスチャデータ (R, G, B, (A)の順に格納されたバイト列)
    /// @param bottom_up データが下から上の順に格納されているか.
    ///        trueの場合、データは左下→右下→...→右上の順 (bottom_up) に格納されているとみなす.
    ///        falseの場合、データは左上→右上→...→右下の順 (top_down) に格納されているとみなす.
    Texture(const unsigned int, const unsigned int, const bool,
            const unsigned char*, const bool = true);

    /// @brief テクスチャデータを設定する
    /// @param width テクスチャの幅 (ピクセル数)
    /// @param height テクスチャの高さ (ピクセル数)
    /// @param has_alpha アルファ値を持つか (trueの場合はRGBA, falseの場合はRGBとして扱う)
    /// @param data テクスチャデータ (R, G, B, (A)の順に格納されたバイト列)
    /// @param bottom_up データが下から上の順に格納されているか.
    ///        trueの場合、データは左下→右下→...→右上の順 (bottom_up) に格納されているとみなす.
    ///        falseの場合、データは左上→右上→...→右下の順 (top_down) に格納されているとみなす.
    /// @param make_top_down データをtop_down形式に変換するか.
    ///        trueの場合、元のデータの順序に関わらず、top_down形式に変換して格納する.
    /// @throw std::invalid_argument dataがnullptrの場合や、width/heightが0の場合
    void SetData(const unsigned int, const unsigned int, const bool,
                 const unsigned char*, const bool = true, const bool = true);

    /// @brief テクスチャデータを初期化する
    /// @note サイズを(0,0)にする
    void Clear() {
        has_alpha_ = false;
        bottom_up_ = false;
        size_ = {0, 0};
        data_.clear();
    }



    /**
     * 画像のプロパティ取得
     */

    /// @brief 画像がアルファ値を持つか
    bool HasAlpha() const { return has_alpha_; }

    /// @brief 画像データがbottom_up形式で格納されているか
    bool IsBottomUp() const { return bottom_up_; }

    /// @brief 画像のサイズ
    std::pair<unsigned int, unsigned int> GetSize() const { return size_; }
    /// @brief 画像の幅
    unsigned int GetWidth() const { return size_.first; }
    /// @brief 画像の高さ
    unsigned int GetHeight() const { return size_.second; }

    /// @brief 有効なデータが設定されているか
    bool IsValid() const { return !data_.empty(); }



    /**
     * 画像データの取得・操作
     */

    /// @brief 画像データの取得 (ポインタ; 読み取り専用)
    const unsigned char* GetData() const { return data_.data(); }

    /// @brief 指定されたピクセルの色を取得 (RGB)
    /// @param x ピクセルのx座標 (0からwidth-1; 左下原点)
    /// @param y ピクセルのy座標 (0からheight-1; 左下原点)
    /// @return (R, G, B)の順に格納された配列
    /// @throw std::out_of_range 指定された座標が範囲外の場合
    std::array<unsigned char, 3> GetPixelRGB(
            const unsigned int, const unsigned int) const;
    /// @brief 指定されたピクセルの色を設定 (RGB)
    /// @param x ピクセルのx座標 (0からwidth-1; 左下原点)
    /// @param y ピクセルのy座標 (0からheight-1; 左下原点)
    /// @param rgb (R, G, B)の順に格納された配列
    /// @throw std::out_of_range 指定された座標が範囲外の場合
    void SetPixelRGB(const unsigned int, const unsigned int,
                     const std::array<unsigned char, 3>&);

    /// @brief 指定されたピクセルの色を取得 (RGBA)
    /// @param x ピクセルのx座標 (0からwidth-1; 左下原点)
    /// @param y ピクセルのy座標 (0からheight-1; 左下原点)
    /// @return (R, G, B, A)の順に格納された配列
    /// @throw std::out_of_range 指定された座標が範囲外の場合
    /// @note has_alpha_がfalseの場合、A成分は255 (不透明) として返す
    std::array<unsigned char, 4> GetPixelRGBA(
            const unsigned int, const unsigned int) const;
    /// @brief 指定されたピクセルの色を設定 (RGBA)
    /// @param x ピクセルのx座標 (0からwidth-1; 左下原点)
    /// @param y ピクセルのy座標 (0からheight-1; 左下原点)
    /// @param rgba (R, G, B, A)の順に格納された配列
    /// @throw std::out_of_range 指定された座標が範囲外の場合
    /// @note has_alpha_がfalseの場合、A成分は無視される
    void SetPixelRGBA(const unsigned int, const unsigned int,
                      const std::array<unsigned char, 4>&);



    /**
     * 新しいテクスチャの作成
     */

    /// @brief RGB形式のテクスチャを作成
    /// @return 現在のデータをコピーしたRGB形式のテクスチャ
    ///         現在のデータがRGBA形式の場合、A成分は無視される
    Texture ToRGB() const;
};

#ifdef IGESIO_ENABLE_TEXTURE_IO

/// @brief 画像ファイルからテクスチャを読み込む
/// @param filename 画像ファイルのパス
/// @return 読み込んだテクスチャ
/// @throw igesio::FileError ファイルが存在しない場合や、読み込みに失敗した場合
Texture LoadTextureFromFile(const std::string&);

/// @brief テクスチャを保存する
/// @param filename 保存先のファイルパス
/// @param texture 保存するテクスチャ
/// @throw igesio::FileError 対応していない拡張子の場合や、保存に失敗した場合
/// @note 対応する拡張子は以下の通り. 大文字・小文字は区別しない.
///       ① .png: PNG形式
///       ② .jpg, .jpeg: JPEG形式
///       ③ .bmp: BMP形式
void SaveTextureToFile(const std::string&, const Texture&);

#endif  // IGESIO_ENABLE_TEXTURE_IO

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_TEXTURE_H_
