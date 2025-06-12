/**
 * @file common/iges_parameter_vector.h
 * @brief IGESのパラメータを混合して保持するためのクラス
 * @author Yayoi Habami
 * @date 2025-05-29
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_COMMON_IGES_PARAMETER_VECTOR_H_
#define IGESIO_COMMON_IGES_PARAMETER_VECTOR_H_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "igesio/common/iges_metadata.h"
#include "igesio/common/serialization.h"



namespace igesio {

/// @brief IGESParameterVectorで使用可能な型
/// @note bool, int, double, uint64_t, std::stringのいずれかを許可
/// @note Logical => `bool`, Integer => `int`, Real => `double`,
///       Pointer => `uint64_t`, String => `std::string`,
///       Language Statement => `std::string`
using VecParamType = std::variant<bool, int, double, uint64_t, std::string>;

/// @brief EntityBaseクラスで保持するパラメータの型を制限するためのヘルパー
/// @note SFINAEを用いて、許可される型のみを受け入れる:
///       `bool` (Logical), `int` (Integer), `double` (Real), `uint64_t` (Pointer),
///       `std::string` (String, Language Statement)
template<typename T> struct is_allowed_type : std::false_type {};
template<> struct is_allowed_type<bool> : std::true_type {};
template<> struct is_allowed_type<int> : std::true_type {};
template<> struct is_allowed_type<double> : std::true_type {};
template<> struct is_allowed_type<std::string> : std::true_type {};
template<> struct is_allowed_type<uint64_t> : std::true_type {};
template<typename T>
inline constexpr bool is_allowed_type_v = is_allowed_type<T>::value;

/// @brief EntityBaseクラスでParameter Dataセクションの各パラメータを
///        一つの配列として保持するためのクラス
/// @note Parameter Dataセクションでは、Logical, Integer, Real, Pointer,
///       String, Language Statementの型を持つパラメータが混在するため、
///       本クラスを使用して型を制限しつつ、異なる型の値を一つの配列として保持する
/// @note Logicalは`bool`、Integerは`int`、Realは`double`、Pointerは`uint64_t`、
///       StringはおよびLanguage Statementは`std::string`として扱う
class IGESParameterVector {
 private:
    /// @brief パラメータの値を保持するためのvector
    std::vector<VecParamType> data_;
    /// @brief 読み込み元のIGESファイルにおいて、各パラメータがどのように表現されていたか
    ///        を保持するためのvector (for conformance rules, see Section 1.4.7)
    std::vector<ValueFormat> formats_;

 public:
    /// @brief 指定したサイズまでIGESParameterVectorを拡張
    /// @param new_size 新しいサイズ
    /// @note デフォルト値はint型の0で埋める
    void resize(const size_t);

    /// @brief 指定したサイズまでIGESParameterVectorを拡張（デフォルト値で埋める）
    /// @param new_size 新しいサイズ
    /// @param default_value デフォルト値 (bool, int, double, uint64_t, std::stringのいずれか)
    void resize(const size_t, const VecParamType&);

    /// @brief 指定したサイズまでIGESParameterVectorを拡張（デフォルト値で埋める）
    /// @param new_size 新しいサイズ
    /// @param default_value デフォルト値 (bool, int, double, uint64_t, std::stringのいずれか)
    /// @param format ValueFormat
    void resize(const size_t, const VecParamType&, const ValueFormat&);

    /// @brief 指定した容量まで予約
    /// @param capacity 予約する容量
    void reserve(const size_t);



    /**
     * 要素へのアクセス
     */

    /// @brief 値の追加
    /// @tparam T 追加する要素の型
    /// @param value 追加する値
    /// @note `T`が許可される型である場合にのみ使用可能・追加できる
    template<typename T>
    std::enable_if_t<is_allowed_type_v<std::decay_t<T>>, void>
    push_back(const T& value,
              const ValueFormat& format = DefaultValueFormat<T>()) {
        data_.push_back(value);
        formats_.push_back(format);
    }

    /// @brief 指定したインデックスの値を設定
    /// @tparam T 設定する要素の型
    /// @param index インデックス
    /// @param value 設定する値
    /// @throw std::out_of_range インデックスが範囲外の場合
    template<typename T>
    std::enable_if_t<is_allowed_type_v<std::decay_t<T>>, void>
    set(size_t index, const T& value, const ValueFormat& format = DefaultValueFormat<T>()) {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of range in IGESParameterVector.");
        }
        data_[index] = value;
        formats_[index] = format;
    }

    /// @brief 指定したインデックスの値を取得
    /// @tparam T 取得する要素の型
    /// @param index インデックス
    /// @return 指定した型の値
    /// @throw std::out_of_range インデックスが範囲外の場合
    /// @throw std::bad_variant_access 指定したインデックスにの値が
    ///        指定した型ではない場合
    /// @note indexの値の型が不明な場合は、`is_type<T>(index)`を使用して、
    ///       そのインデックスの値が指定した型であるかを確認することができる
    template<typename T>
    std::enable_if_t<is_allowed_type_v<T>, T>
    get(size_t index) const {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of range in IGESParameterVector.");
        }
        if (!std::holds_alternative<T>(data_[index])) {
            throw std::bad_variant_access();
        }
        return std::get<T>(data_[index]);
    }

    /// @brief 指定した要素を文字列に変換して取得
    /// @param index インデックス
    /// @param config 変換に使用する設定 (数値用)
    /// @return 指定したインデックスの要素を文字列に変換したもの
    /// @throw std::out_of_range インデックスが範囲外の場合
    std::string get_as_string(size_t, const SerializationConfig&) const;

    /// @brief IGESParameterVectorをクリア
    void clear() noexcept;



    /**
     * 要素の検証など
     */

    /// @brief 指定したインデックスの値が指定した型であるかを確認
    /// @tparam T 確認する型
    /// @param index インデックス
    /// @return 指定した型であればtrue、そうでなければfalse
    /// @throw std::out_of_range インデックスが範囲外の場合
    template<typename T>
    std::enable_if_t<is_allowed_type_v<T>, bool>
    is_type(size_t index) const {
        if (index >= data_.size()) {
            throw std::out_of_range("Index out of range in IGESParameterVector.");
        }
        return std::holds_alternative<T>(data_[index]);
    }

    /// @brief 指定したインデックスの値の型を取得
    /// @param index インデックス
    /// @return CppParameterType 列挙型の値
    /// @throw std::out_of_range インデックスが範囲外の場合
    CppParameterType get_type(size_t) const;

    /// @brief 指定したインデックスの値のValueFormatを取得
    /// @param index インデックス
    /// @return 指定したインデックスのValueFormat
    /// @throw std::out_of_range インデックスが範囲外の場合
    ValueFormat get_format(size_t) const;

    /// @brief IGESParameterVectorのサイズを取得
    size_t size() const noexcept { return data_.size(); }

    /// @brief IGESParameterVectorが空かどうかを確認
    bool empty() const noexcept { return data_.empty(); }
};

}  // namespace igesio

#endif  // IGESIO_COMMON_IGES_PARAMETER_VECTOR_H_
