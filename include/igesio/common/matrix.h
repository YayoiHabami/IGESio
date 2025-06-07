/**
 * @file common/matrix.h
 * @brief 本ライブラリで使用する行列・ベクトルクラスを定義する
 * @author Yayoi Habami
 * @date 2025-05-30
 * @copyright 2025 Yayoi Habami
 * @note 基本的にはEigenの行列・ベクトルクラスを使用するが、
 *       サードパーティーライブラリであるEigenを使用しない場合に備えて、
 *       代替の行列・ベクトルクラスを定義する。
 */
#ifndef IGESIO_COMMON_MATRIX_H_
#define IGESIO_COMMON_MATRIX_H_

#ifdef IGESIO_ENABLE_EIGEN

/**
 * Eigenの行列・ベクトルクラスを使用する場合
 *
 * 可能であればEigenを合わせて本ライブラリをビルドすることを推奨する。
 */

#include <Eigen/Core>

namespace igesio {

using Eigen::Dynamic;
using Eigen::NoChange;

using Eigen::Matrix2d;
using Eigen::Matrix23d;
using Eigen::Matrix32d;
using Eigen::Matrix3d;
using Eigen::Matrix2Xd;
using Eigen::Matrix3Xd;
using Eigen::Vector2d;
using Eigen::Vector3d;

}  // namespace igesio

#else  // IGESIO_ENABLE_EIGEN

/**
 * Eigenの行列・ベクトルクラスを使用する場合
 *
 * 基本的にはEigenの行列・ベクトルクラスをそのまま使用されたいが、
 * サードパーティーライブラリであるEigenを使用しない場合に備えて、
 * 代替の行列・ベクトルクラスを以下に定義する。
 * 可能であればEigenの行列・ベクトルクラスを使用することを推奨する。
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace igesio {

/// @brief 動的列数を表す定数
/// @note Matrixテンプレートの列数MにDynamicを指定することで、
///       動的な列数を持つ行列を作成できる
constexpr int Dynamic = -1;
/// @brief 行数・列数の変更なしを表す定数
/// @brief Matrix::conservativeResize()で行数を変更しない場合に使用
constexpr size_t NoChange = 0;

namespace detail {

/// @brief 2次元または3次元の固定行数・動的列数対応行列クラス
/// @tparam N 行数（2または3のみ）
/// @tparam M 列数（Dynamicまたは1～3）
/// @note このクラスは以下の制限がある：
///       - 行数Nは2または3のみサポート
///       - 列数Mは動的（Dynamic = -1）または1～3の固定値のみサポート
///       - 内部データはcolumn-majorで格納される
///       - サポートされる演算：要素アクセス、行列加算/減算、スカラー乗算/除算、
///         行列・ベクトル積（特定条件下）
///       - サイズ変更は列数のみ可能（conservativeResize）で、行数は変更不可
///       - ベクトル（M=1）の場合のみ単一インデックスアクセスが可能
///       - 行列乗算は行列・ベクトル積のみサポート（一般的な行列乗算は未サポート）
/// @note IGESIO_ENABLE_EIGENが定義されていない場合に使用される、
///       Eigenの代替実装として機能する
template<int N, int M>
class Matrix {
 private:
    /// @brief 内部データ（column-majorで格納）
    std::vector<std::array<double, N>> data_;

    /// @brief Nは2か3のみを許可
    static_assert(N == 2 || N == 3, "N must be 2 or 3");
    /// @brief Mは-1(Dynamic)または1～3のみを許可
    static_assert(M == Dynamic || (M >= 1 && M <= 3), "M must be Dynamic or between 1 and 3");

 public:
    /// @brief デフォルトコンストラクタ
    /// @note 固定列数の場合は指定された列数で初期化、動的列数の場合は空で初期化
    /// @note 各要素の値は未設定となることに注意。値が設定された状態で初期化したい場合は、
    ///       Matrix::Zero()などの静的メソッドを使用すること。
    Matrix() {
        if constexpr (M != Dynamic) {
            data_.resize(M);
        }
    }

    /// @brief 動的列数の場合のみ使用可能なコンストラクタ
    /// @param rows 行数（Nに等しい値を指定すること)
    /// @param cols 列数
    /// @note M == Dynamicの場合のみ使用可能
    /// @note 本実装ではrowsを無視する
    /// @note 各要素の値は未設定となることに注意。値が設定された状態で初期化したい場合は、
    ///       Matrix::Zero()などの静的メソッドを使用すること。
    template<int M_ = M, typename std::enable_if_t<M_ == Dynamic, int> = 0>
    explicit Matrix([[maybe_unused]]int rows, int cols) {
        data_.resize(cols);
    }

    /// @brief 初期化子リストを使用したコンストラクタ (ベクトル用)
    /// @param init_list 初期化リスト
    ///        `{value1, value2, ...}`
    /// @note M == 1の場合のみ使用可能
    template<int M_ = M, typename std::enable_if_t<M_ == 1, int> = 0>
    Matrix(std::initializer_list<double> init_list) {
        // 行数チェック
        if (init_list.size() != N) {
            throw std::invalid_argument("Row count must match template parameter N");
        }

        // データ領域を確保
        data_.resize(1);
        data_[0].fill(0.0);  // 初期化

        // データを格納
        size_t idx = 0;
        for (const auto& val : init_list) {
            data_[0][idx] = val;
            ++idx;
        }
    }

    /// @brief 初期化リストを使用したコンストラクタ
    /// @param init_list 初期化リスト
    ///        `{{row1_col1, row1_col2, ...}, {row2_col1, row2_col2, ...}, ...}`
    /// @note 行数はNに、列数は固定列数の場合はMに一致する必要がある. 例えば
    ///       2x3行列の場合は、`{{1, 2, 3}, {4, 5, 6}}`のように指定する。
    Matrix(std::initializer_list<std::initializer_list<double>> init_list) {
        // 行数チェック
        if (init_list.size() != N) {
            throw std::invalid_argument("Row count must match template parameter N");
        }

        // 列数を取得（最初の行から）
        size_t cols = init_list.begin()->size();

        // 固定列数の場合は列数チェック
        if constexpr (M != Dynamic) {
            if (cols != M) {
                throw std::invalid_argument("Column count must match template parameter M");
            }
        }

        // 全ての行の列数が一致するかチェック
        for (const auto& row : init_list) {
            if (row.size() != cols) {
                throw std::invalid_argument("All rows must have the same number of columns");
            }
        }

        // データ領域を確保
        data_.resize(cols);

        // データをcolumn-majorで格納
        size_t row_idx = 0;
        for (const auto& row : init_list) {
            size_t col_idx = 0;
            for (const auto& val : row) {
                data_[col_idx][row_idx] = val;
                ++col_idx;
            }
            ++row_idx;
        }
    }

    /// @brief 行数を取得
    /// @return 行数
    size_t rows() const { return N; }

    /// @brief 列数を取得
    /// @return 列数
    size_t cols() const { return data_.size(); }

    /// @brief 総要素数を取得
    /// @return 総要素数
    size_t size() const { return rows() * cols(); }

    /// @brief 要素アクセス（非const版）
    /// @param i 行インデックス
    /// @param j 列インデックス
    /// @return 要素への参照
    double& operator()(size_t i, size_t j) {
        assert(i < N && j < data_.size());
        return data_[j][i];
    }

    /// @brief 要素アクセス（const版）
    /// @param i 行インデックス
    /// @param j 列インデックス
    /// @return 要素への参照
    const double& operator()(size_t i, size_t j) const {
        assert(i < N && j < data_.size());
        return data_[j][i];
    }

    /// @brief ベクトル（M=1）の場合のみ有効な単一インデックスアクセス（非const版）
    /// @param i 行インデックス
    /// @return 要素への参照
    /// @note M == 1の場合のみ使用可能
    template<int M_ = M>
    typename std::enable_if_t<M_ == 1, double&>
    operator()(size_t i) {
        assert(i < N);
        return data_[0][i];
    }

    /// @brief ベクトル（M=1）の場合のみ有効な単一インデックスアクセス（const版）
    /// @param i 行インデックス
    /// @return 要素への参照
    /// @note M == 1の場合のみ使用可能
    template<int M_ = M>
    typename std::enable_if_t<M_ == 1, const double&>
    operator()(size_t i) const {
        assert(i < N);
        return data_[0][i];
    }

    /// @brief サイズ変更（データ保持）
    /// @param rows 新しい行数（NoChangeのみ許可）
    /// @param cols 新しい列数
    /// @note 行数の変更は許可されない。動的列数の場合のみ列数変更可能
    /// @throw std::invalid_argument 行数がNoChangeでない場合、
    ///        または固定列数で列数を変更しようとした場合
    void conservativeResize(size_t rows, size_t cols) {
        if (rows != NoChange) {
            throw std::invalid_argument("Rows must be NoChange");
        }

        if constexpr (M != Dynamic) {
            if (cols != M) {
                throw std::invalid_argument("Cannot resize fixed-size columns");
            }
            return;
        }

        size_t old_cols = data_.size();
        if (cols != old_cols) {
            data_.resize(cols);
        }
    }

    /// @brief 行列加算
    /// @param other 加算する行列
    /// @return 加算結果の行列
    /// @throw std::invalid_argument 動的列数で列数が一致しない場合
    Matrix<N, M> operator+(const Matrix<N, M>& other) const {
        if constexpr (M == Dynamic) {
            if (cols() != other.cols()) {
                throw std::invalid_argument("Matrix dimensions don't match for addition");
            }
        }

        Matrix<N, M> result;
        if constexpr (M == Dynamic) {
            result.data_.resize(cols());
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                result(i, j) = (*this)(i, j) + other(i, j);
            }
        }
        return result;
    }

    /// @brief 行列加算代入
    /// @param other 加算する行列
    /// @return 自身への参照
    /// @throw std::invalid_argument 動的列数で列数が一致しない場合
    Matrix<N, M>& operator+=(const Matrix<N, M>& other) {
        if constexpr (M == Dynamic) {
            if (cols() != other.cols()) {
                throw std::invalid_argument("Matrix dimensions don't match for addition");
            }
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                (*this)(i, j) += other(i, j);
            }
        }
        return *this;
    }

    /// @brief 行列減算
    /// @param other 減算する行列
    /// @return 減算結果の行列
    /// @throw std::invalid_argument 動的列数で列数が一致しない場合
    Matrix<N, M> operator-(const Matrix<N, M>& other) const {
        if constexpr (M == Dynamic) {
            if (cols() != other.cols()) {
                throw std::invalid_argument("Matrix dimensions don't match for subtraction");
            }
        }

        Matrix<N, M> result;
        if constexpr (M == Dynamic) {
            result.data_.resize(cols());
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                result(i, j) = (*this)(i, j) - other(i, j);
            }
        }
        return result;
    }

    /// @brief 行列減算代入
    /// @param other 減算する行列
    /// @return 自身への参照
    /// @throw std::invalid_argument 動的列数で列数が一致しない場合
    Matrix<N, M>& operator-=(const Matrix<N, M>& other) {
        if constexpr (M == Dynamic) {
            if (cols() != other.cols()) {
                throw std::invalid_argument("Matrix dimensions don't match for subtraction");
            }
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                (*this)(i, j) -= other(i, j);
            }
        }
        return *this;
    }

    /// @brief スカラー乗算
    /// @param scalar 乗算するスカラー値
    /// @return 乗算結果の行列
    Matrix<N, M> operator*(double scalar) const {
        Matrix<N, M> result;
        if constexpr (M == Dynamic) {
            result.data_.resize(cols());
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                result(i, j) = (*this)(i, j) * scalar;
            }
        }
        return result;
    }

    /// @brief スカラー乗算代入
    /// @param scalar 乗算するスカラー値
    /// @return 自身への参照
    Matrix<N, M>& operator*=(double scalar) {
        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                (*this)(i, j) *= scalar;
            }
        }
        return *this;
    }

    /// @brief スカラー除算
    /// @param scalar 除算するスカラー値
    /// @return 除算結果の行列
    /// @throw std::invalid_argument ゼロ除算の場合
    Matrix<N, M> operator/(double scalar) const {
        if (scalar == 0) {
            throw std::invalid_argument("Division by zero");
        }

        Matrix<N, M> result;
        if constexpr (M == Dynamic) {
            result.data_.resize(cols());
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                result(i, j) = (*this)(i, j) / scalar;
            }
        }
        return result;
    }

    /// @brief スカラー除算代入
    /// @param scalar 除算するスカラー値
    /// @return 自身への参照
    /// @throw std::invalid_argument ゼロ除算の場合
    Matrix<N, M>& operator/=(double scalar) {
        if (scalar == 0) {
            throw std::invalid_argument("Division by zero");
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                (*this)(i, j) /= scalar;
            }
        }
        return *this;
    }

    /// @brief 行列・行列積
    /// @param other 乗算する行列
    /// @return 乗算結果の行列
    /// @throw std::invalid_argument 動的列数で次元が一致しない場合
    template<int N2, int M2>
    Matrix<N, M2> operator*(const Matrix<N2, M2>& other) const {
        if ((M == Dynamic && cols() != N2) || (M != Dynamic && M != N2)) {
            throw std::invalid_argument("Matrix dimensions don't match for multiplication");
        }
        Matrix<N, M2> result;
        if constexpr (M2 == Dynamic) {
            result = Matrix<N, M2>(N, other.cols());
        }

        for (size_t i = 0; i < N; ++i) {
            for (size_t j = 0; j < other.cols(); ++j) {
                double sum = 0.0;
                for (size_t k = 0; k < (M == Dynamic ? cols() : M); ++k) {
                    sum += (*this)(i, k) * other(k, j);
                }
                result(i, j) = sum;
            }
        }
        return result;
    }



    /**
     * 静的メソッド
     */
    /// @brief 全ての要素が指定された値で初期化された行列を生成 (動的サイズ版)
    /// @param rows 行数 (Nに等しい値を指定すること)
    /// @param cols 列数
    /// @param value 初期化する値
    /// @return 初期化された行列
    /// @throw std::invalid_argument rowsがN以外の値の場合
    template<int M_ = M>
    static typename std::enable_if_t<M_ == Dynamic, Matrix<N, M>>
    Constant(int rows, int cols, double value) {
        if (rows != N) {
            throw std::invalid_argument("Rows must be equal to N");
        }

        Matrix<N, M> result(rows, cols);
        for (size_t j = 0; j < cols; ++j) {
            for (size_t i = 0; i < N; ++i) {
                result(i, j) = value;
            }
        }
        return result;
    }

    /// @brief 全ての要素が指定された値で初期化された行列を生成 (固定サイズ版)
    /// @param value 初期化する値
    /// @return 初期化された行列
    template<int M_ = M>
    static typename std::enable_if_t<M_ != Dynamic, Matrix<N, M>>
    Constant(double value) {
        Matrix<N, M> result;
        for (size_t j = 0; j < result.cols(); ++j) {
            for (size_t i = 0; i < N; ++i) {
                result(i, j) = value;
            }
        }
        return result;
    }

    /// @brief ゼロ行列を作成
    /// @param rows 行数 (Nに等しい値を指定すること)
    /// @param cols 列数
    /// @return ゼロ行列
    template<int M_ = M>
    static typename std::enable_if_t<M_ == Dynamic, Matrix<N, M>>
    Zero(int rows, int cols) { return Constant(rows, cols, 0.0); }

    /// @brief ゼロ行列を作成 (固定サイズ版)
    /// @return ゼロ行列
    template<int M_ = M>
    static typename std::enable_if_t<M_ != Dynamic, Matrix<N, M>>
    Zero() { return Constant(0.0); }

    /// @brief 単位行列を作成 (動的サイズ版)
    /// @param rows 行数 (Nに等しい値を指定すること)
    /// @param cols 列数
    /// @return 単位行列
    template<int M_ = M>
    static typename std::enable_if_t<M_ == Dynamic, Matrix<N, M>>
    Identity(int rows, int cols) {
        if (rows != N) {
            throw std::invalid_argument("Rows must be equal to N");
        }

        Matrix<N, M> result = Zero(rows, cols);
        for (size_t i = 0; i < std::min(N, cols); ++i) {
            result(i, i) = 1.0;
        }
        return result;
    }

    /// @brief 単位行列を作成 (固定サイズ版)
    /// @return 単位行列
    template<int M_ = M>
    static typename std::enable_if_t<M_ != Dynamic, Matrix<N, M>>
    Identity() {
        Matrix<N, M> result = Zero();
        for (size_t i = 0; i < std::min(N, M); ++i) {
            result(i, i) = 1.0;
        }
        return result;
    }
};

/// @brief スカラー・行列乗算（非メンバ関数）
/// @tparam N 行数
/// @tparam M 列数
/// @param scalar 乗算するスカラー値
/// @param mat 乗算する行列
/// @return 乗算結果の行列
template<int N, int M>
Matrix<N, M> operator*(double scalar, const detail::Matrix<N, M>& mat) {
    return mat * scalar;
}

/// @brief 出力演算子
/// @param os 出力ストリーム
/// @param matrix 行列
/// @return 出力ストリーム
/// @note Eigenのデフォルトのフォーマットと異なることに注意.
///       2行3列の行列は`((1, 2, 3), (4, 5, 6))`のように出力される.
template<int N, int M>
std::ostream& operator<<(std::ostream& os, const detail::Matrix<N, M>& matrix) {
    os << "(";
    for (size_t i = 0; i < matrix.rows(); ++i) {
        os << "(";
        for (size_t j = 0; j < matrix.cols(); ++j) {
            os << matrix(i, j);
            if (j < matrix.cols() - 1) os << ", ";
        }
        os << ")";
        if (i < matrix.rows() - 1) os << ", ";
    }
    os << ")";
    return os;
}

}  // namespace detail



/// @brief 2x2行列の型エイリアス
using Matrix2d = detail::Matrix<2, 2>;

/// @brief 2x3行列の型エイリアス
using Matrix23d = detail::Matrix<2, 3>;

/// @brief 3x2行列の型エイリアス
using Matrix32d = detail::Matrix<3, 2>;

/// @brief 3x3行列の型エイリアス
using Matrix3d = detail::Matrix<3, 3>;

/// @brief 2行・動的列数行列の型エイリアス
using Matrix2Xd = detail::Matrix<2, Dynamic>;

/// @brief 3行・動的列数行列の型エイリアス
using Matrix3Xd = detail::Matrix<3, Dynamic>;

/// @brief 2次元ベクトルの型エイリアス
using Vector2d = detail::Matrix<2, 1>;

/// @brief 3次元ベクトルの型エイリアス
using Vector3d = detail::Matrix<3, 1>;

}  // namespace igesio

#endif  // IGESIO_ENABLE_EIGEN

#endif  // IGESIO_COMMON_MATRIX_H_
