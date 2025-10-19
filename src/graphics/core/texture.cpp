/**
 * @file graphics/core/texture.cpp
 * @brief 描画用のテクスチャクラス
 * @author Yayoi Habami
 * @date 2025-10-10
 * @copyright 2025 Yayoi Habami
 * @note 面に張り付けて描画するテクスチャを表す
 */
#include "igesio/graphics/core/texture.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"

#ifdef IGESIO_ENABLE_TEXTURE_IO
// 画像の読み込み用
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#endif  // IGESIO_ENABLE_TEXTURE_IO

namespace {

using igesio::graphics::Texture;

}  // namespace



size_t Texture::GetDataIndex(const unsigned int x, const unsigned int y) const {
    if (x >= size_.first || y >= size_.second) {
        throw std::out_of_range("Pixel coordinates out of range.");
    }
    unsigned int row = bottom_up_ ? (size_.second - 1 - y) : y;
    return (row * size_.first + x) * (has_alpha_ ? 4 : 3);
}

Texture::Texture(const unsigned int width, const unsigned int height,
                 const bool has_alpha, const unsigned char* data,
                 const bool bottom_up) {
    SetData(width, height, has_alpha, data, bottom_up);
}

void Texture::SetData(const unsigned int width, const unsigned int height,
                      const bool has_alpha, const unsigned char* data,
                      const bool bottom_up, const bool make_top_down) {
    const size_t expected_size = width * height * (has_alpha ? 4 : 3);
    if (data == nullptr) {
        throw std::invalid_argument("Data pointer is null.");
    } else if (width == 0 || height == 0) {
        throw std::invalid_argument("Width and height must be positive.");
    }

    size_ = {width, height};
    has_alpha_ = has_alpha;
    bottom_up_ = make_top_down ? false : bottom_up;

    data_.resize(expected_size);
    // データをコピー
    if (!make_top_down || !bottom_up) {
        // 順序を変える必要がない場合は、そのままコピー
        std::copy_n(data, expected_size, data_.begin());
    } else {
        // bottom_upからtop_downに変換する場合
        const unsigned int row_size = width * (has_alpha ? 4 : 3);
        for (unsigned int y = 0; y < height; ++y) {
            // 元データの上からy行目を、該当する行にコピー
            std::copy_n(data + (height - 1 - y) * row_size,
                        row_size, data_.begin() + y * row_size);
        }
    }
}



/**
 * 画像データの取得・操作
 */

std::array<unsigned char, 3> Texture::GetPixelRGB(
        const unsigned int x, const unsigned int y) const {
    size_t idx = GetDataIndex(x, y);
    return {data_[idx], data_[idx + 1], data_[idx + 2]};
}

void Texture::SetPixelRGB(
        const unsigned int x, const unsigned int y,
        const std::array<unsigned char, 3>& rgb) {
    size_t idx = GetDataIndex(x, y);
    data_[idx] = rgb[0];
    data_[idx + 1] = rgb[1];
    data_[idx + 2] = rgb[2];
}

std::array<unsigned char, 4> Texture::GetPixelRGBA(
        const unsigned int x, const unsigned int y) const {
    size_t idx = GetDataIndex(x, y);
    if (has_alpha_) {
        return {data_[idx], data_[idx + 1], data_[idx + 2], data_[idx + 3]};
    } else {
        return {data_[idx], data_[idx + 1], data_[idx + 2], 255};
    }
}

void Texture::SetPixelRGBA(
        const unsigned int x, const unsigned int y,
        const std::array<unsigned char, 4>& rgba) {
    size_t idx = GetDataIndex(x, y);
    data_[idx] = rgba[0];
    data_[idx + 1] = rgba[1];
    data_[idx + 2] = rgba[2];
    if (has_alpha_) data_[idx + 3] = rgba[3];
}

Texture Texture::ToRGB() const {
    if (!IsValid()) {
        return Texture();  // 空のテクスチャを返す
    }
    if (!has_alpha_) {
        return *this;  // 既にRGB形式ならそのまま返す
    }

    // nullptrは渡せないため、先にデータをコピーする
    std::vector<unsigned char> rgb_data(size_.first * size_.second * 3);
    for (unsigned int y = 0; y < size_.second; ++y) {
        for (unsigned int x = 0; x < size_.first; ++x) {
            auto rgba = GetPixelRGBA(x, y);
            rgb_data[(y * size_.first + x) * 3 + 0] = rgba[0];
            rgb_data[(y * size_.first + x) * 3 + 1] = rgba[1];
            rgb_data[(y * size_.first + x) * 3 + 2] = rgba[2];
        }
    }

    Texture rgb_texture;
    rgb_texture.SetData(size_.first, size_.second, false,
                        rgb_data.data(), bottom_up_, false);
    return rgb_texture;
}



#ifdef IGESIO_ENABLE_TEXTURE_IO

Texture igesio::graphics::LoadTextureFromFile(const std::string& filename) {
    int width, height, channels;
    unsigned char* img_data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    if (img_data == nullptr) {
        throw igesio::FileError("Failed to load image file: " + filename);
    }
    if (channels != 3 && channels != 4) {
        stbi_image_free(img_data);
        throw igesio::FileError("Unsupported image format (must be RGB or RGBA): " + filename);
    }

    Texture tex;
    tex.SetData(static_cast<unsigned int>(width),
                static_cast<unsigned int>(height),
                channels == 4,
                img_data,
                true,   // stb_imageはbottom_up形式で読み込む
                true);  // top_down形式に変換して格納

    stbi_image_free(img_data);

    return tex;
}

void igesio::graphics::SaveTextureToFile(const std::string& filename, const Texture& texture) {
    std::string prefix = "Failed to save texture image: ";
    if (!texture.IsValid()) {
        throw igesio::FileError(prefix + "Texture data is invalid.");
    }

    // 拡張子を取得
    std::filesystem::path filepath(filename);
    if (!filepath.has_extension()) {
        throw igesio::FileError(prefix + "Filename has no extension: " + filename);
    }
    std::string ext = filepath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // 共通のパラメータ
    auto [width, height] = texture.GetSize();
    auto channels = texture.HasAlpha() ? 4 : 3;
    if (!texture.IsBottomUp()) {
        // 画像データがtop-down形式の場合は反転
        stbi_flip_vertically_on_write(true);
    }

    // 対応する拡張子に応じて保存 (アルファ値を表現可能な形式)
    if (ext == ".png") {
        // PNG形式で保存
        if (stbi_write_png(filename.c_str(), width, height, channels,
                           texture.GetData(), width * channels) == 0) {
            throw igesio::FileError(prefix + "Failed to save PNG image: " + filename);
        }
        return;
    } else if (ext == ".bmp") {
        // BMP形式で保存
        if (stbi_write_bmp(filename.c_str(), width, height, channels,
                           texture.GetData()) == 0) {
            throw igesio::FileError(prefix + "Failed to save BMP image: " + filename);
        }
        return;
    }

    // 対応する拡張子に応じて保存 (アルファ値を表現できない形式)
    auto rgb_texture = texture.ToRGB();
    if (ext == ".jpg" || ext == ".jpeg") {
        // JPEG形式で保存 (品質は90に設定)
        if (stbi_write_jpg(filename.c_str(), width, height, 3,
                           rgb_texture.GetData(), 90) == 0) {
            throw igesio::FileError(prefix + "Failed to save JPEG image: " + filename);
        }
        return;
    }

    throw igesio::FileError(prefix + "Unsupported file extension: " + ext);
}

#endif  // IGESIO_ENABLE_TEXTURE_IO
