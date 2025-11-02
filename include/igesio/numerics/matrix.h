/**
 * @file numerics/matrix.h
 * @brief 本ライブラリで使用する行列・ベクトルクラスを定義する
 * @author Yayoi Habami
 * @date 2025-05-30
 * @copyright 2025 Yayoi Habami
 * @note 基本的にはEigenの行列・ベクトルクラスを使用するが、
 *       サードパーティーライブラリであるEigenを使用しない場合に備えて、
 *       代替の行列・ベクトルクラスを定義する。
 */
#ifndef IGESIO_NUMERICS_MATRIX_H_
#define IGESIO_NUMERICS_MATRIX_H_

#include <algorithm>
#include <string>

namespace igesio {

/// @brief 円周率の定数
constexpr double kPi = 3.14159265358979323846;

}  // namespace igesio



#ifdef IGESIO_ENABLE_EIGEN

/**
 * Eigenの行列・ベクトルクラスを使用する場合
 *
 * 可能であればEigenを合わせて本ライブラリをビルドすることを推奨する。
 */

#include <Eigen/Core>
#include <Eigen/Geometry>

namespace igesio {

using Eigen::Dynamic;
using Eigen::NoChange;

template<typename T, int N, int M>
using Matrix = Eigen::Matrix<T, N, M>;

template<typename T, int N>
using Vector = Eigen::Matrix<T, N, 1>;
template<typename T, int M>
using RowVector = Eigen::Matrix<T, 1, M>;

// float
using Eigen::Matrix2f;
using Eigen::Matrix23f;
using Eigen::Matrix24f;
using Eigen::Matrix2Xf;
using Eigen::Matrix3f;
using Eigen::Matrix32f;
using Eigen::Matrix34f;
using Eigen::Matrix3Xf;
using Eigen::Matrix4f;
using Eigen::Matrix42f;
using Eigen::Matrix43f;
using Eigen::Matrix4Xf;
using Eigen::MatrixX2f;
using Eigen::MatrixX3f;
using Eigen::MatrixX4f;
using Eigen::MatrixXf;
using Eigen::Vector2f;
using Eigen::Vector3f;
using Eigen::Vector4f;
using Eigen::VectorXf;
using Eigen::RowVector2f;
using Eigen::RowVector3f;
using Eigen::RowVector4f;
using Eigen::RowVectorXf;

// double
using Eigen::Matrix2d;
using Eigen::Matrix23d;
using Eigen::Matrix24d;
using Eigen::Matrix2Xd;
using Eigen::Matrix3d;
using Eigen::Matrix32d;
using Eigen::Matrix34d;
using Eigen::Matrix3Xd;
using Eigen::Matrix4d;
using Eigen::Matrix42d;
using Eigen::Matrix43d;
using Eigen::Matrix4Xd;
using Eigen::MatrixX2d;
using Eigen::MatrixX3d;
using Eigen::MatrixX4d;
using Eigen::MatrixXd;
using Eigen::Vector2d;
using Eigen::Vector3d;
using Eigen::Vector4d;
using Eigen::VectorXd;
using Eigen::RowVector2d;
using Eigen::RowVector3d;
using Eigen::RowVector4d;
using Eigen::RowVectorXd;

template<typename T>
using AngleAxis = Eigen::AngleAxis<T>;
using AngleAxisf = Eigen::AngleAxisf;
using AngleAxisd = Eigen::AngleAxisd;

/// @brief 2つのベクトル間の角度をラジアンで計算する
/// @param a ベクトル1
/// @param b ベクトル2
/// @param in_degrees 角度を度で取得する場合は`true`、ラジアンで取得する場合は`false`(デフォルト)
/// @return ベクトルaとbのなす角
/// @throw std::invalid_argument ベクトルのサイズが異なる場合、またはゼロベクトルが含まれる場合
template<typename DerivedA, typename DerivedB>
double AngleBetween(const Eigen::MatrixBase<DerivedA>& a,
                    const Eigen::MatrixBase<DerivedB>& b,
                    bool in_degrees = false) {
    // 単位ベクトルに正規化
    double na = a.norm();
    double nb = b.norm();
    if (na == 0.0 || nb == 0.0) {
        throw std::invalid_argument("Cannot compute angle with zero-length vector.");
    }
    // サイズが異なる場合は例外を投げる
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vectors must have the same dimension.");
    }

    // 内積を計算 ([-1, 1]の範囲に収める)
    double cos_theta = a.dot(b) / (na * nb);
    cos_theta = std::clamp(cos_theta, -1.0, 1.0);
    double angle_rad = std::acos(cos_theta);
    if (in_degrees) {
        return angle_rad * (180.0 / kPi);
    } else {
        return angle_rad;
    }
}

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
#include <cassert>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"



namespace igesio {

/// @brief 動的列数を表す定数
/// @note Matrixテンプレートの列数MにDynamicを指定することで、
///       動的な列数を持つ行列を作成できる
constexpr int Dynamic = -1;
/// @brief 行数・列数の変更なしを表す定数
/// @brief Matrix::conservativeResize()で行数を変更しない場合に使用
constexpr size_t NoChange = 0;



/// @brief 2次元または3次元の固定行数・動的列数対応行列クラス
/// @tparam T 要素の型 (doubleなど)
/// @tparam N 行数（Dynamicまたは1～4）
/// @tparam M 列数（Dynamicまたは1～4）
/// @note このクラスは以下の制限がある：
///       - 型Tはdoubleまたはfloatのみを許可
///       - 行数Nは動的（Dynamic = -1）または1～4の固定値のみサポート
///       - 列数Mは動的（Dynamic = -1）または1～4の固定値のみサポート
///       - 内部データはcolumn-majorで格納される
///       - サポートされる演算：要素アクセス、行列加算/減算、スカラー乗算/除算、
///         行列・ベクトル積（特定条件下）
///       - サイズ変更はconservativeResizeで行数・列数ともに可能
///       - ベクトル（M=1）の場合のみ単一インデックスアクセスが可能
///       - 行列乗算は行列・ベクトル積のみサポート
/// @note IGESIO_ENABLE_EIGENが定義されていない場合に使用される、
///       Eigenの代替実装として機能する
/// @note 将来的には、ビルド時間の短縮のため、明示的実体化を行うように
///       実装の分離を検討する
template<typename T, int N, int M>
class Matrix {
 private:
    /// @brief Tはdoubleまたはfloatのみを許可
    static_assert(std::is_same_v<T, double> || std::is_same_v<T, float>,
                  "T must be double or float");

    /// @brief Nは-1(Dynamic)または1～4のみを許可
    static_assert(N == Dynamic || (N >= 1 && N <= 4),
                  "N must be Dynamic or between 1 and 4");
    /// @brief Mは-1(Dynamic)または1～4のみを許可
    static_assert(M == Dynamic || (M >= 1 && M <= 4),
                  "M must be Dynamic or between 1 and 4");

    /// @brief 内部データ
    std::vector<T> data_;
    /// @brief 動的な行数（NがDynamicの場合のみ使用）
    size_t rows_ = 0;
    /// @brief column-majorか
    bool column_major_ = true;

    /// @brief 2つの次元のうち、どちらかがDynamicならDynamicを、
    ///        そうでなければ最初の次元を返すヘルパー
    template<int D1, int D2>
    static constexpr int ResultDim = (D1 == Dynamic || D2 == Dynamic) ? Dynamic : D1;

    /// @brief 別の行列との加算/減算に対する戻り値の型を定義する
    /// @tparam N2 行数
    /// @tparam M2 列数
    /// @note mat + mat2のような演算に対して、戻り値の型を決定するためのエイリアス;
    ///       以下の条件に基づいて戻り値の型を決定する:
    ///       - NまたはN2がDynamicの場合、行数はDynamic; そうでなければN
    ///       - MまたはM2がDynamicの場合、列数はDynamic; そうでなければM
    template<int N2, int M2>
    using AddReturnType = Matrix<T, ResultDim<N, N2>, ResultDim<M, M2>>;
    /// @brief 別の行列との加算/減算に対するtype_traits
    /// @tparam N2 行数
    /// @tparam M2 列数
    template<int N2, int M2>
    using enable_if_addable_t = std::enable_if_t<
            (N2 == Dynamic || N2 == N) && (M2 == Dynamic || M2 == M)>;

    /// @brief 別の行列との乗算に対する戻り値の型を定義する
    /// @tparam N2 行数
    /// @tparam M2 列数
    /// @note mat * mat2のような演算に対して、戻り値の型を決定するためのエイリアス;
    ///       以下の条件に基づいて戻り値の型を決定する:
    ///       - NまたはN2がDynamicの場合、行数はDynamic; そうでなければN
    ///       - MまたはM2がDynamicの場合、列数はDynamic; そうでなければM2
    template<int N2, int M2>
    using MulReturnType = Matrix<T, ResultDim<N, N2>, ResultDim<M2, M>>;
    /// @brief 別の行列との乗算に対するtype_traits
    /// @tparam N2 行数
    /// @tparam M2 列数
    template<int N2, int M2>
    using enable_if_multipliable_t = std::enable_if_t<
            M == N2 || M == Dynamic || N2 == Dynamic>;

    /// @brief インデックスを計算する
    /// @param i 行インデックス
    /// @param j 列インデックス
    /// @return インデックス
    size_t index(size_t i, size_t j) const {
        assert(i < rows() && j < cols());
        if (column_major_) {
            // column-majorの場合
            return j * rows() + i;
        } else {
            // row-majorの場合
            return i * cols() + j;
        }
    }

    /// @brief カンマ初期化用のヘルパークラス
    class CommaInitializer {
     public:
        /// @brief コンストラクタ
        /// @param mat 初期化対象のMatrixオブジェクト
        /// @param index 次に値を挿入するインデックス
        CommaInitializer(Matrix& mat, const size_t index)
            : matrix_(mat), index_(index) {}

        /// @brief カンマ演算子
        /// @param value 設定する値
        /// @return ヘルパーオブジェクトの参照
        /// @note 行列のサイズを超える値は無視されます。
        CommaInitializer& operator,(T value) {
            if (index_ < matrix_.size()) {
                auto cols = index_ % matrix_.cols();
                auto rows = static_cast<size_t>(index_ / matrix_.cols());
                matrix_(rows, cols) = value;
                ++index_;
            }
            return *this;
        }

     private:
        Matrix& matrix_;
        size_t index_;
    };

    /// @brief Matrixの非constな部分ブロックを表すプロキシクラス
    template<int BlockRows, int BlockCols>
    class MatrixBlock {
     private:
        Matrix<T, N, M>& matrix_;
        size_t start_row_;
        size_t start_col_;
        size_t block_rows_;
        size_t block_cols_;

     public:
        /// @brief コンストラクタ
        /// @param matrix 対象のMatrixオブジェクト
        /// @param start_row 開始行インデックス
        /// @param start_col 開始列インデックス
        template<int BlockRows_ = BlockRows,
                 typename = std::enable_if_t<BlockRows_ != Dynamic && BlockCols != Dynamic>>
        MatrixBlock(Matrix<T, N, M>& matrix,
                    size_t start_row, size_t start_col) :
            MatrixBlock(matrix, start_row, start_col,
                    static_cast<size_t>(BlockRows), static_cast<size_t>(BlockCols)) {}

        /// @brief コンストラクタ
        /// @param matrix 対象のMatrixオブジェクト
        /// @param start_row 開始行インデックス
        /// @param start_col 開始列インデックス
        /// @param block_rows ブロックの行数 (BlockRowsがDynamicの場合は省略不可)
        /// @param block_cols ブロックの列数 (BlockColsがDynamicの場合は省略不可)
        /// @throw std::invalid_argument ブロックのサイズが一致しない場合、
        ///        またはDynamicブロックサイズが指定された場合
        MatrixBlock(Matrix<T, N, M>& matrix,
                    size_t start_row, size_t start_col,
                    int block_rows, int block_cols)
            : matrix_(matrix), start_row_(start_row), start_col_(start_col) {
            // 動的行数・動的列数が指定された場合は、行数と列数を明示的に設定する必要がある
            if (block_rows == Dynamic || block_cols == Dynamic) {
                throw std::invalid_argument("Dynamic block size is not allowed");
            }
            block_rows_ = static_cast<size_t>(block_rows);
            block_cols_ = static_cast<size_t>(block_cols);

            if (BlockRows != Dynamic && block_rows_ != static_cast<size_t>(BlockRows)) {
                throw std::invalid_argument("Block row size does not match");
            }
            if (BlockCols != Dynamic && block_cols_ != static_cast<size_t>(BlockCols)) {
                throw std::invalid_argument("Block column size does not match");
            }

            // ブロックが元の行列の範囲内に収まるかチェック
            if (start_row + block_rows_ > matrix.rows() || start_col + block_cols_ > matrix.cols()) {
                throw std::out_of_range("Block is out of matrix bounds.");
            }
        }

        /// @brief 他のMatrixオブジェクトからブロックへ代入する
        /// @param other 代入元のMatrixオブジェクト
        /// @return *thisの参照
        /// @throw std::invalid_argument サイズが一致しない場合
        template<int N2, int M2>
        MatrixBlock& operator=(const Matrix<T, N2, M2>& other) {
            if (other.rows() != block_rows_ || other.cols() != block_cols_) {
                throw std::invalid_argument("Block size does not match");
            }
            for (size_t i = 0; i < block_rows_; ++i) {
                for (size_t j = 0; j < block_cols_; ++j) {
                    matrix_(start_row_ + i, start_col_ + j) = other(i, j);
                }
            }
            return *this;
        }

        /// @brief ブロックから新しいMatrixオブジェクトを生成する (読み取り用)
        /// @return 新しいMatrixオブジェクト
        operator Matrix<T, BlockRows, BlockCols>() const {
            if constexpr (BlockRows == Dynamic || BlockCols == Dynamic) {
                // 動的サイズの場合
                // matrix_をconstとして扱うためにstatic_castを使用する
                return static_cast<const Matrix<T, N, M>&>(matrix_)
                        .block(start_row_, start_col_, block_rows_, block_cols_);
            } else {
                // 固定サイズの場合
                Matrix<T, BlockRows, BlockCols> result;
                for (size_t i = 0; i < BlockRows; ++i) {
                    for (size_t j = 0; j < BlockCols; ++j) {
                        result(i, j) = matrix_(start_row_ + i, start_col_ + j);
                    }
                }
                return result;
            }
        }
    };

 public:
    /// @brief デフォルトコンストラクタ
    /// @note 固定行数・固定列数の場合は指定された列数で初期化する.
    ///       行数か列数のどちらかが動的の場合は空で初期化する.
    /// @note 各要素の値は未設定となることに注意。値が設定された状態で初期化したい場合は、
    ///       Matrix::Zero()などの静的メンバ関数を使用すること。
    Matrix() { resize(N == Dynamic ? 0 : N, M == Dynamic ? 0 : M); }

    /// @brief 動的行数または動的列数の場合のみ使用可能なコンストラクタ
    /// @param rows 行数
    /// @param cols 列数
    /// @note N == Dynamic または M == Dynamicの場合のみ使用可能
    /// @note 各要素の値は未設定となることに注意。値が設定された状態で初期化したい場合は、
    ///       Matrix::Zero()などの静的メンバ関数を使用すること。
    /// @throw std::invalid_argument 固定行数・列数と一致しない場合
    template<int N_ = N, int M_ = M, typename = std::enable_if_t<N_ == Dynamic || M_ == Dynamic>>
    explicit Matrix(int rows, int cols) {
        resize(rows, cols);
    }

    /// @brief 可変長引数を使用したコンストラクタ
    /// @param args 可変長引数
    /// @note `Vector3d vec(1.0, 2.0, 3.0);`や`RowVector3d row_vec(1.0, 2.0, 3.0);`のように、
    ///       使用する. 指定する引数の数は、行数Nまたは列数Mのどちらかに一致する必要がある.
    /// @note M == 1 または N == 1の場合のみ使用可能
    template<typename ...Args, typename = std::enable_if_t<
            std::conjunction_v<std::is_constructible<T, Args>...> && (M == 1 || N == 1)>>
    explicit Matrix(Args&&... args) {
        // 行数か列数が固定であり、かつ列数か行数とargsの数が一致することを確認
        static_assert((N == 1 && (M == sizeof...(args) || M == Dynamic)) ||
                      (M == 1 && (N == sizeof...(args) || N == Dynamic)),
                      "Row or column count at compile time must be Dynamic, and "
                      "the number of args must match the other dimension");

        // データ領域を確保
        if constexpr (N == 1) {
            resize(1, sizeof...(args));
        } else {
            resize(sizeof...(args), 1);
        }

        // 可変長引数を使用してデータを格納
        size_t idx = 0;
        (void)(((*this)(idx++) = T(std::forward<Args>(args))), ...);
    }

    /// @brief 初期化子リストを使用したコンストラクタ (ベクトル用)
    /// @param init_list 初期化リスト
    ///        `{value1, value2, ...}`
    /// @note M == 1 または N == 1の場合のみ使用可能
    template<typename U, typename = std::enable_if_t<
            std::is_constructible_v<T, U> && (M == 1 || N == 1)>>
    Matrix(std::initializer_list<U> init_list) {
        // 行数/列数チェック
        if (N == 1 && (M != Dynamic && M != init_list.size())) {
            throw std::invalid_argument("line count must be Dynamic or "
                    "equal to the size of the initializer list");
        } else if (M == 1 && (N != Dynamic && N != init_list.size())) {
            throw std::invalid_argument("column count must be Dynamic or "
                    "equal to the size of the initializer list");
        }

        // データ領域を確保
        if constexpr (N == 1) {
            resize(1, init_list.size());
        } else {
            resize(init_list.size(), 1);
        }

        // データを格納
        size_t idx = 0;
        for (const auto& val : init_list) {
            (*this)(idx) = val;
            ++idx;
        }
    }

    /// @brief 初期化リストを使用したコンストラクタ
    /// @param init_list 初期化リスト
    ///        `{{row1_col1, row1_col2, ...}, {row2_col1, row2_col2, ...}, ...}`
    /// @note 行数はNに、列数は固定列数の場合はMに一致する必要がある. 例えば
    ///       2x3行列の場合は、`{{1, 2, 3}, {4, 5, 6}}`のように指定する。
    Matrix(std::initializer_list<std::initializer_list<T>> init_list) {
        // 行数チェック
        if (N != Dynamic && init_list.size() !=
                static_cast<typename std::initializer_list<T>::size_type>(N)) {
            throw std::invalid_argument("Row count must match template parameter N; "
                                        "otherwise, N must be Dynamic");
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

        // データ領域を確保 (新しい行数 x 新しい列数)
        resize(init_list.size(), cols);

        // データをrow-majorで格納
        size_t row_idx = 0;
        for (const auto& row : init_list) {
            size_t col_idx = 0;
            for (const auto& val : row) {
                (*this)(row_idx, col_idx) = val;
                ++col_idx;
            }
            ++row_idx;
        }
    }

    /// @brief 行数を取得
    /// @return 行数
    size_t rows() const {
        if constexpr (N == Dynamic) {
            return rows_;
        } else {
            return N;
        }
    }

    /// @brief 列数を取得
    /// @return 列数
    size_t cols() const {
        if constexpr (M == Dynamic) {
            // 動的列数の場合はデータサイズから計算
            return (rows() == 0) ? 0 : static_cast<size_t>(data_.size() / rows());
        } else {
            // 固定列数の場合はMを返す
            return M;
        }
    }

    /// @brief 総要素数を取得
    /// @return 総要素数
    size_t size() const { return rows() * cols(); }

    /// @brief 要素アクセス（非const版）
    /// @param i 行インデックス
    /// @param j 列インデックス
    /// @return 要素への参照
    T& operator()(size_t i, size_t j) {
        assert(i < rows() && j < cols());
        return data_[index(i, j)];
    }

    /// @brief 要素アクセス（const版）
    /// @param i 行インデックス
    /// @param j 列インデックス
    /// @return 要素への参照
    const T& operator()(size_t i, size_t j) const {
        assert(i < rows() && j < cols());
        return data_[index(i, j)];
    }

    /// @brief ベクトル（M=1）の場合のみ有効な単一インデックスアクセス（非const版）
    /// @param i 行インデックス
    /// @return 要素への参照
    /// @note M == 1の場合のみ使用可能
    template<int M_ = M>
    typename std::enable_if_t<(M_ == 1 || N == 1), T&>
    operator()(size_t i) {
        assert(i < size());
        return data_[i];
    }

    /// @brief ベクトル（M=1）の場合のみ有効な単一インデックスアクセス（const版）
    /// @param i 行インデックス
    /// @return 要素への参照
    /// @note M == 1の場合のみ使用可能
    template<int M_ = M>
    typename std::enable_if_t<(M_ == 1 || N == 1), const T&>
    operator()(size_t i) const {
        assert(i < size());
        return data_[i];
    }

    /// @brief ベクトルの要素アクセス (非const版)
    /// @param i インデックス
    /// @return 要素への参照
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 || N == 1)>>
    T& operator[](size_t i) {
        assert(i < size());
        return (*this)(i);
    }

    /// @brief ベクトルの要素アクセス (const版)
    /// @param i インデックス
    /// @return 要素への参照
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 || N == 1)>>
    const T& operator[](size_t i) const {
        assert(i < size());
        return (*this)(i);
    }

    /// @brief x座標への要素アクセス (非const版; 固定長ベクトルのみ)
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 && N >= 1) || (N == 1 && M_ >= 1)>>
    T& x() { return (*this)(0); }
    /// @brief x座標への要素アクセス (const版; 固定長ベクトルのみ)
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 && N >= 1) || (N == 1 && M_ >= 1)>>
    const T& x() const { return (*this)(0); }
    /// @brief y座標への要素アクセス (非const版; 固定長ベクトルのみ)
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 && N >= 2) || (N == 1 && M_ >= 2)>>
    T& y() { return (*this)(1); }
    /// @brief y座標への要素アクセス (const版; 固定長ベクトルのみ)
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 && N >= 2) || (N == 1 && M_ >= 2)>>
    const T& y() const { return (*this)(1); }
    /// @brief z座標への要素アクセス (非const版; 固定長ベクトルのみ)
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 && N >= 3) || (N == 1 && M_ >= 3)>>
    T& z() { return (*this)(2); }
    /// @brief z座標への要素アクセス (const版; 固定長ベクトルのみ)
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 && N >= 3) || (N == 1 && M_ >= 3)>>
    const T& z() const { return (*this)(2); }
    /// @brief w座標への要素アクセス (非const版; 固定長ベクトルのみ)
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 && N >= 4) || (N == 1 && M_ >= 4)>>
    T& w() { return (*this)(3); }
    /// @brief w座標への要素アクセス (const版; 固定長ベクトルのみ)
    template<int M_ = M, typename = std::enable_if_t<(M_ == 1 && N >= 4) || (N == 1 && M_ >= 4)>>
    const T& w() const { return (*this)(3); }

    /// @brief i列目のベクトルを取得
    /// @param i 取得する列のインデックス
    /// @return i列目のベクトル
    /// @throw std::out_of_range 列インデックスが範囲外の場合
    /// @note `mat.col(i) = vec;`のような代入操作は不可
    Matrix<T, N, 1> col(size_t i) const {
        if (i >= cols()) {
            throw std::out_of_range("Column index out of range");
        }

        Matrix<T, N, 1> result;
        for (size_t row = 0; row < N; ++row) {
            result(row) = (*this)(row, i);
        }

        return result;
    }

    /// @brief j行目のベクトルを取得
    /// @param j 取得する行のインデックス
    /// @return j行目のベクトル
    /// @throw std::out_of_range 行インデックスが範囲外の場合
    /// @note `mat.row(i) = row_vec;`のような代入操作は不可
    Matrix<T, 1, M> row(size_t j) const {
        if (j >= rows()) {
            throw std::out_of_range("Row index out of range");
        }

        Matrix<T, 1, M> result;
        for (size_t col = 0; col < M; ++col) {
            result(col) = (*this)(j, col);
        }

        return result;
    }

    /// @brief 全要素をcolumn-majorで取得 (非const版)
    /// @return 行列の全要素をcolumn-major順で格納した配列へのポインタ
    /// @note 要素数はrows x cols
    T* data() {
        return data_.data();
    }

    /// @brief row-majorで全要素を取得 (const版)
    /// @return 行列の全要素をcolumn-major順で格納した配列へのポインタ
    /// @note 要素数はrows x cols
    const T* data() const {
        return data_.data();
    }

    /// @brief 行列の一部を取得する (const版)
    /// @tparam row_count 取得する行数
    /// @tparam col_count 取得する列数
    /// @param row_start 開始行インデックス
    /// @param col_start 開始列インデックス
    /// @return 指定された部分行列
    /// @throw std::out_of_range 行インデックスまたは列インデックスが範囲外の場合
    template<size_t row_count, size_t col_count>
    Matrix<T, row_count, col_count> block(size_t row_start, size_t col_start) const {
        if (row_start + row_count > rows() || col_start + col_count > cols()) {
            throw std::out_of_range("Block indices out of range");
        }

        Matrix<T, row_count, col_count> result;
        for (size_t i = 0; i < row_count; ++i) {
            for (size_t j = 0; j < col_count; ++j) {
                result(i, j) = (*this)(row_start + i, col_start + j);
            }
        }
        return result;
    }

    /// @brief 行列の一部を取得する (const版)
    /// @param row_start 開始行インデックス
    /// @param col_start 開始列インデックス
    /// @param row_count 取得する行数
    /// @param col_count 取得する列数
    /// @return 指定された部分行列
    /// @throw std::out_of_range 行インデックスまたは列インデックスが範囲外の場合
    Matrix<T, Dynamic, Dynamic> block(size_t row_start, size_t col_start,
                                      size_t row_count, size_t col_count) const {
        if (row_start + row_count > rows() || col_start + col_count > cols()) {
            throw std::out_of_range("Block indices out of range");
        }

        Matrix<T, Dynamic, Dynamic> result(row_count, col_count);
        for (size_t i = 0; i < row_count; ++i) {
            for (size_t j = 0; j < col_count; ++j) {
                result(i, j) = (*this)(row_start + i, col_start + j);
            }
        }
        return result;
    }

    /// @brief 行列の非constな一部を取得する
    /// @tparam row_count 取得する行数
    /// @tparam col_count 取得する列数
    /// @param row_start 開始行インデックス
    /// @param col_start 開始列インデックス
    /// @return 指定された部分行列
    /// @throw std::out_of_range 行インデックスまたは列インデックスが範囲外の場合
    template<int row_count, int col_count>
    MatrixBlock<row_count, col_count> block(size_t row_start, size_t col_start) {
        return MatrixBlock<row_count, col_count>(*this, row_start, col_start);
    }

    /// @brief 行列の非constな一部を取得する (動的サイズ版)
    /// @param row_start 開始行インデックス
    /// @param col_start 開始列インデックス
    /// @param row_count 取得する行数
    /// @param col_count 取得する列数
    /// @return 指定された部分行列
    /// @throw std::out_of_range 行インデックスまたは列インデックスが範囲外の場合
    MatrixBlock<Dynamic, Dynamic> block(size_t row_start, size_t col_start,
                                         size_t row_count, size_t col_count) {
        return MatrixBlock<Dynamic, Dynamic>(
                *this, row_start, col_start, row_count, col_count);
    }

    /// @brief カンマ初期化子
    /// @param value 最初の値
    /// @return カンマ初期化用のヘルパーオブジェクト
    /// @note 使用前に resize() などでサイズが設定されている必要があります。
    /// @note 例: matrix << 1, 2, 3, 4;
    CommaInitializer operator<<(T value) {
        assert(size() > 0); // サイズが0の場合はアサーション失敗
        if (size() > 0) {
            data_[0] = value;
        }
        return CommaInitializer(*this, 1);
    }



    /**
     * サイズの変更・変換
     */

    /// @brief 行列のサイズを変更（データ破棄）
    /// @param new_rows 新しい行数
    /// @param new_cols 新しい列数
    /// @note 動的な行または列を持つ場合のみサイズ変更可能
    /// @throw std::invalid_argument 固定サイズの行列でサイズを変更しようとした場合
    /// @note 値は0で初期化される
    void resize(size_t new_rows, size_t new_cols) {
        if constexpr (N != Dynamic) {
            if (new_rows != N && new_rows != NoChange) {
                throw std::invalid_argument("Cannot resize fixed-size rows");
            }
            new_rows = N;
        }
        if constexpr (M != Dynamic) {
            if (new_cols != M && new_cols != NoChange) {
                throw std::invalid_argument("Cannot resize fixed-size columns");
            }
            new_cols = M;
        }

        if ((data_.size() != 0) &&
            (new_rows == NoChange || new_rows == rows()) &&
            (new_cols == NoChange || new_cols == cols())) {
            // サイズが変わらない場合は何もしない
            return;
        }

        // リサイズ先の行数と列数を決定
        new_rows = (new_rows == NoChange) ? rows() : new_rows;
        new_cols = (new_cols == NoChange) ? cols() : new_cols;
        // 行数がDynamicの場合はrows_を更新
        if constexpr (N == Dynamic) rows_ = new_rows;

        // データを新しいサイズで初期化
        data_.resize(new_rows * new_cols, 0);
    }

    /// @brief サイズ変更（データ保持）
    /// @param new_rows 新しい行数
    /// @param new_cols 新しい列数
    /// @note 動的な行または列を持つ場合のみサイズ変更可能
    /// @throw std::invalid_argument 固定サイズの行列でサイズを変更しようとした場合
    void conservativeResize(size_t new_rows, size_t new_cols) {
        if constexpr (N != Dynamic) {
            if (new_rows != N && new_rows != NoChange) {
                throw std::invalid_argument("Cannot resize fixed-size rows");
            }
        }
        if constexpr (M != Dynamic) {
            if (new_cols != M && new_cols != NoChange) {
                throw std::invalid_argument("Cannot resize fixed-size columns");
            }
        }

        if ((new_rows == NoChange || new_rows == rows()) &&
            (new_cols == NoChange || new_cols == cols())) {
            // サイズが変わらない場合は何もしない
            return;
        }

        // リサイズ先の行数と列数を決定
        const size_t old_rows = rows();
        const size_t old_cols = cols();
        const size_t final_new_rows = (new_rows == NoChange) ? old_rows : new_rows;
        const size_t final_new_cols = (new_cols == NoChange) ? old_cols : new_cols;

        // 元データを一時的に保持
        std::vector<T> old_data;
        old_data.swap(data_);

        // 新しいサイズでリサイズ
        resize(final_new_rows, final_new_cols);

        // データをコピー
        const size_t rows_to_copy = std::min(old_rows, final_new_rows);
        const size_t cols_to_copy = std::min(old_cols, final_new_cols);

        for (size_t j = 0; j < cols_to_copy; ++j) {
            for (size_t i = 0; i < rows_to_copy; ++i) {
                // column-majorを想定
                (*this)(i, j) = old_data[j * old_rows + i];
            }
        }
    }

    /// @brief サイズを変更した行列を取得する
    /// @param new_rows 新しい行数
    /// @param new_cols 新しい列数
    /// @return サイズを変更した行列
    /// @note 動的な行または列を持つ場合のみサイズ変更可能
    Matrix<T, Dynamic, Dynamic> reshaped(size_t new_rows, size_t new_cols) const {
        Matrix<T, Dynamic, Dynamic> result;
        result.resize(new_rows, new_cols);

        const size_t rows_to_copy = std::min(rows(), new_rows);
        const size_t cols_to_copy = std::min(cols(), new_cols);

        for (size_t j = 0; j < cols_to_copy; ++j) {
            for (size_t i = 0; i < rows_to_copy; ++i) {
                result(i, j) = (*this)(i, j);
            }
        }
        return result;
    }

    /// @brief 転置
    /// @return 転置行列
    Matrix<T, M, N> transpose() const {
        Matrix<T, M, N> result;
        result.resize(cols(), rows());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(j, i) = (*this)(i, j);
            }
        }
        return result;
    }

    /// @brief 自身を転置 (正方行列 or 動的行数・列数の場合のみ)
    template<int N_ = N>
    typename std::enable_if_t<N_ == M, Matrix<T, N, M>&>
    transposeInPlace() {
        auto original = *this;
        if (rows() != cols()) {
            resize(cols(), rows());
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                (*this)(i, j) = original(j, i);
            }
        }
        return *this;
    }

    /// @brief 型を変換
    /// @tparam U 変換後の型
    /// @return 型を変換した行列
    template<typename U>
    Matrix<U, N, M> cast() const {
        Matrix<U, N, M> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = static_cast<U>((*this)(i, j));
            }
        }
        return result;
    }



    /**
     * 演算子オーバーロード
     */

    /// @brief 単項演算子-（要素ごとの符号反転）
    /// @return 符号反転した行列
    Matrix<T, N, M> operator-() const {
        Matrix<T, N, M> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = -(*this)(i, j);
            }
        }
        return result;
    }

    /// @brief 行列加算
    /// @param other 加算する行列
    /// @return 加算結果の行列
    /// @throw std::invalid_argument 動的行数/動的列数で行数/列数が一致しない場合
    template<int N2, int M2, typename = enable_if_addable_t<N2, M2>>
    AddReturnType<N2, M2> operator+(const Matrix<T, N2, M2>& other) const {
        if constexpr (M == Dynamic || M2 == Dynamic || N == Dynamic || N2 == Dynamic) {
            if ((cols() != other.cols()) || (rows() != other.rows())) {
                throw std::invalid_argument("Matrix dimensions don't match for addition");
            }
        }

        AddReturnType<N2, M2> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = (*this)(i, j) + other(i, j);
            }
        }
        return result;
    }

    /// @brief 行列加算代入
    /// @param other 加算する行列
    /// @return 自身への参照
    /// @throw std::invalid_argument 動的行数/動的列数で行数/列数が一致しない場合
    template<int N2, int M2, typename = enable_if_addable_t<N2, M2>>
    Matrix<T, N, M>& operator+=(const Matrix<T, N2, M2>& other) {
        if constexpr (M == Dynamic || M2 == Dynamic || N == Dynamic || N2 == Dynamic) {
            if ((cols() != other.cols()) || (rows() != other.rows())) {
                throw std::invalid_argument("Matrix dimensions don't match for addition");
            }
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                (*this)(i, j) += other(i, j);
            }
        }
        return *this;
    }

    /// @brief 行列減算
    /// @param other 減算する行列
    /// @return 減算結果の行列
    /// @throw std::invalid_argument 動的行数/動的列数で行数/列数が一致しない場合
    template<int N2, int M2, typename = enable_if_addable_t<N2, M2>>
    AddReturnType<N2, M2> operator-(const Matrix<T, N2, M2>& other) const {
        if constexpr (M == Dynamic || M2 == Dynamic || N == Dynamic || N2 == Dynamic) {
            if ((cols() != other.cols()) || (rows() != other.rows())) {
                throw std::invalid_argument("Matrix dimensions don't match for subtraction");
            }
        }

        AddReturnType<N2, M2> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = (*this)(i, j) - other(i, j);
            }
        }
        return result;
    }

    /// @brief 行列減算代入
    /// @param other 減算する行列
    /// @return 自身への参照
    /// @throw std::invalid_argument 動的行数/動的列数で行数/列数が一致しない場合
    template<int N2, int M2, typename = enable_if_addable_t<N2, M2>>
    Matrix<T, N, M>& operator-=(const Matrix<T, N2, M2>& other) {
        if constexpr (M == Dynamic || M2 == Dynamic || N == Dynamic || N2 == Dynamic) {
            if ((cols() != other.cols()) || (rows() != other.rows())) {
                throw std::invalid_argument("Matrix dimensions don't match for subtraction");
            }
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                (*this)(i, j) -= other(i, j);
            }
        }
        return *this;
    }

    /// @brief スカラー乗算
    /// @param scalar 乗算するスカラー値
    /// @return 乗算結果の行列
    Matrix<T, N, M> operator*(T scalar) const {
        Matrix<T, N, M> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = (*this)(i, j) * scalar;
            }
        }
        return result;
    }

    /// @brief スカラー乗算代入
    /// @param scalar 乗算するスカラー値
    /// @return 自身への参照
    Matrix<T, N, M>& operator*=(T scalar) {
        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                (*this)(i, j) *= scalar;
            }
        }
        return *this;
    }

    /// @brief スカラー除算
    /// @param scalar 除算するスカラー値
    /// @return 除算結果の行列
    /// @throw std::invalid_argument ゼロ除算の場合
    Matrix<T, N, M> operator/(T scalar) const {
        if (scalar == 0) {
            throw std::invalid_argument("Division by zero");
        }

        Matrix<T, N, M> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = (*this)(i, j) / scalar;
            }
        }
        return result;
    }

    /// @brief スカラー除算代入
    /// @param scalar 除算するスカラー値
    /// @return 自身への参照
    /// @throw std::invalid_argument ゼロ除算の場合
    Matrix<T, N, M>& operator/=(T scalar) {
        if (scalar == 0) {
            throw std::invalid_argument("Division by zero");
        }

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                (*this)(i, j) /= scalar;
            }
        }
        return *this;
    }

    /// @brief 行ベクトルと列ベクトルの行列積
    /// @param other 乗算する列ベクトル
    /// @return 乗算結果のスカラー値 (内積)
    template<int N2, int M2>
    auto operator*(const Matrix<T, N2, M2>& other) const
            -> std::enable_if_t<N == 1 && M2 == 1 && M == N2, T> {
        T result = 0;
        for (size_t k = 0; k < M; ++k) {
            result += (*this)(0, k) * other(k, 0);
        }
        return result;
    }

    /// @brief 行列・行列積
    /// @param other 乗算する行列
    /// @return 乗算結果の行列
    /// @throw std::invalid_argument 自身の列数と相手の行数が一致しない場合
    template<int N2, int M2>
    auto operator*(const Matrix<T, N2, M2>& other) const -> std::enable_if_t<
            (M == N2 || M == Dynamic || N2 == Dynamic) && !(N == 1 && M2 == 1),
            MulReturnType<N2, M2>> {
        if (cols() != other.rows()) {
            throw std::invalid_argument("Matrix dimensions don't match for multiplication");
        }
        MulReturnType<N2, M2> result;
        result.resize(rows(), other.cols());

        for (size_t i = 0; i < rows(); ++i) {
            for (size_t j = 0; j < other.cols(); ++j) {
                T sum = 0.0;
                for (size_t k = 0; k < cols(); ++k) {
                    sum += (*this)(i, k) * other(k, j);
                }
                result(i, j) = sum;
            }
        }
        return result;
    }



    /**
     * ベクトル専用演算
     */

    /// @brief 内積 (ベクトル専用)
    /// @param other 内積を計算する相手のベクトル
    /// @return 内積の結果
    /// @throw std::invalid_argument 列数または行数が一致しない場合
    template<int N2, int M2, typename = enable_if_addable_t<N2, M2>>
    T dot(const Matrix<T, N2, M2>& other) const {
        if (size() != other.size() || (rows() == 1 && cols() != other.cols()) ||
            (cols() == 1 && rows() != other.rows())) {
            throw std::invalid_argument("Matrix dimensions don't match for dot product");
        }

        T result = 0.0;
        for (size_t i = 0; i < N; ++i) {
            result += (*this)(i) * other(i);
        }
        return result;
    }

    /// @brief 外積 (3次元ベクトル専用)
    /// @param other 外積を計算する相手のベクトル
    /// @return 外積の結果
    /// @throw std::invalid_argument 1x3または3x1のベクトルでない場合
    template<int N2, int M2, typename = enable_if_addable_t<N2, M2>>
    Matrix<T, ResultDim<N, N2>, ResultDim<M, M2>>
    cross(const Matrix<T, N2, M2>& other) const {
        if (size() != 3 || other.size() != 3 ||
            (rows() == 1 && cols() != other.cols()) ||
            (cols() == 1 && rows() != other.rows())) {
            throw std::invalid_argument("Both matrices must be 3D vectors for cross product");
        }

        Matrix<T, ResultDim<N, N2>, ResultDim<M, M2>> result;
        result(0) = (*this)(1) * other(2) - (*this)(2) * other(1);
        result(1) = (*this)(2) * other(0) - (*this)(0) * other(2);
        result(2) = (*this)(0) * other(1) - (*this)(1) * other(0);
        return result;
    }



    /**
     * 要素ごとの演算
     */

    /// @brief 要素ごとの積 (アダマール積)
    /// @param other 要素ごとの積を計算する相手の行列
    /// @return 要素ごとの積の結果
    /// @throw std::invalid_argument 動的列数で列数が一致しない場合
    /// @note 要素ごとの演算のため、引数や戻り値のサイズは足し算と同じ
    template<int N2, int M2, typename = enable_if_addable_t<N2, M2>>
    AddReturnType<N2, M2> cwiseProduct(const Matrix<T, N2, M2>& other) const {
        if (rows() != other.rows() || cols() != other.cols()) {
            throw std::invalid_argument("Matrix dimensions must match for cwiseProduct");
        }

        AddReturnType<N2, M2> result;
        result.conservativeResize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = (*this)(i, j) * other(i, j);
            }
        }

        return result;
    }

    /// @brief 要素ごとの除算
    /// @param other 要素ごとの除算を計算する相手の行列
    /// @return 要素ごとの除算の結果
    /// @throw std::invalid_argument 動的列数で列数が一致しない場合
    template<int N2, int M2, typename = enable_if_addable_t<N2, M2>>
    AddReturnType<N2, M2> cwiseQuotient(const Matrix<T, N2, M2>& other) const {
        // 動的サイズの場合のチェック
        if (rows() != other.rows() || cols() != other.cols()) {
            throw std::invalid_argument("Matrix dimensions must match for cwiseQuotient");
        }

        AddReturnType<N2, M2> result;
        result.conservativeResize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = (*this)(i, j) / other(i, j);
            }
        }

        return result;
    }

    /// @brief 要素ごとの逆数
    /// @return 要素ごとの逆数を計算した結果
    Matrix<T, N, M> cwiseInverse() const {
        Matrix<T, N, M> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = 1.0 / (*this)(i, j);
            }
        }

        return result;
    }

    /// @brief 要素ごとの平方根
    /// @return 要素ごとの平方根を計算した結果
    Matrix<T, N, M> cwiseSqrt() const {
        Matrix<T, N, M> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = std::sqrt((*this)(i, j));
            }
        }

        return result;
    }

    /// @brief 要素ごとの絶対値
    /// @return 要素ごとの絶対値を計算した結果
    Matrix<T, N, M> cwiseAbs() const {
        Matrix<T, N, M> result;
        result.resize(rows(), cols());

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result(i, j) = std::abs((*this)(i, j));
            }
        }

        return result;
    }

    /// @brief 正規化されたベクトルを返す
    /// @return 大きさが1のベクトル
    /// @throw std::invalid_argument ゼロベクトルの正規化を試みた場合、
    ///        または行列がベクトルでない場合
    template<int M_ = M, int N_ = N>
    auto normalized() const -> std::enable_if_t<
            N_ == 1 || M_ == 1 || (N_ == Dynamic && M_ == Dynamic),
            Matrix<T, N, M>> {
        if (rows() != 1 && cols() != 1) {
            throw std::invalid_argument("Normalization is only defined for vectors");
        }

        T norm_value = norm();
        if (norm_value == 0) {
            throw std::invalid_argument("Cannot normalize a zero vector");
        }

        return (*this) / norm_value;
    }


    /**
     * Reduction operations
     */

    /// @brief 要素の二乗和
    /// @return 全要素の二乗和
    T squaredNorm() const {
        T result = 0.0;

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result += (*this)(i, j) * (*this)(i, j);
            }
        }

        return result;
    }

    /// @brief ノルム（要素の二乗和の平方根）
    /// @return ノルム
    T norm() const {
        return std::sqrt(squaredNorm());
    }

    /// @brief 全要素の和
    /// @return 全要素の和
    T sum() const {
        T result = 0.0;

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result += (*this)(i, j);
            }
        }

        return result;
    }

    /// @brief 全要素の積
    /// @return 全要素の積
    T prod() const {
        T result = 1.0;

        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                result *= (*this)(i, j);
            }
        }

        return result;
    }



    /**
     * 要素の検証 (戻り値がbool)
     */

    /// @brief NaNを含むかどうか
    /// @return 1つでもNaNが含まれていればtrue、そうでなければfalse
    bool hasNaN() const {
        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                if (std::isnan((*this)(i, j))) {
                    return true;
                }
            }
        }
        return false;
    }

    /// @brief すべての要素が有限値かどうか
    /// @return 1つでも±∞やNaNが含まれていればfalse、すべて有限値ならtrue
    bool allFinite() const {
        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                if (!std::isfinite((*this)(i, j))) {
                    return false;
                }
            }
        }
        return true;
    }

    /// @brief すべての要素がvに近しいか
    /// @param v 比較する値
    /// @param tol 許容誤差
    /// @return すべての要素がvにtol以内であればtrue、そうでなければfalse
    bool isConstant(T v, T tol = static_cast<T>(1e-6)) const {
        for (size_t j = 0; j < cols(); ++j) {
            for (size_t i = 0; i < rows(); ++i) {
                if (std::abs((*this)(i, j) - v) > tol) {
                    return false;
                }
            }
        }
        return true;
    }
    /// @brief すべての要素が1に近しいか
    bool isOnes(T tol = static_cast<T>(1e-6)) const {
        return isConstant(static_cast<T>(1), tol);
    }
    /// @brief すべての要素が0に近しいか
    bool isZero(T tol = static_cast<T>(1e-6)) const {
        return isConstant(static_cast<T>(0), tol);
    }



    /**
     * 逆行列・行列式
     */

    /// @brief 行列式の計算
    /// @return 行列式の値
    /// @note 2x2, 3x3, 4x4行列のみサポート
    /// @throw std::invalid_argument 行列が正方行列でない場合
    T determinant() const {
        auto& m = *this;
        if (rows() == 2 && cols() == 2) {
            return m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0);
        } else if (rows() == 3 && cols() == 3) {
            return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1))
                 - m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0))
                 + m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
        } else if (rows() == 4 && cols() == 4) {
            return m(0, 0) * (+ m(1, 1) * (m(2, 2) * m(3, 3) - m(2, 3) * m(3, 2))
                              + m(1, 2) * (m(2, 3) * m(3, 1) - m(2, 1) * m(3, 3))
                              + m(1, 3) * (m(2, 1) * m(3, 2) - m(2, 2) * m(3, 1)))
                 - m(1, 0) * (+ m(0, 1) * (m(2, 2) * m(3, 3) - m(2, 3) * m(3, 2))
                              + m(0, 2) * (m(2, 3) * m(3, 1) - m(2, 1) * m(3, 3))
                              + m(0, 3) * (m(2, 1) * m(3, 2) - m(2, 2) * m(3, 1)))
                 + m(2, 0) * (+ m(0, 1) * (m(1, 2) * m(3, 3) - m(1, 3) * m(3, 2))
                              + m(0, 2) * (m(1, 3) * m(3, 1) - m(1, 1) * m(3, 3))
                              + m(0, 3) * (m(1, 1) * m(3, 2) - m(1, 2) * m(3, 1)))
                 - m(3, 0) * (+ m(0, 1) * (m(1, 2) * m(2, 3) - m(1, 3) * m(2, 2))
                              + m(0, 2) * (m(1, 3) * m(2, 1) - m(1, 1) * m(2, 3))
                              + m(0, 3) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)));
        } else if (rows() != cols()) {
            // 正方行列のみサポート
            throw std::invalid_argument("Determinant is only implemented for square matrices");
        } else {
            throw NotImplementedError(
                    "Determinant is only implemented for 2x2, 3x3 and 4x4 matrices");
        }
    }

    /// @brief 逆行列の計算
    /// @return 逆行列
    /// @note 2x2, 3x3, 4x4行列のみサポート
    /// @throw std::invalid_argument 行列が正方行列でない場合
    Matrix<T, N, M> inverse() const {
        auto& m = *this;
        Matrix<T, N, M> result;
        if (N == Dynamic || M == Dynamic) {
            // 動的サイズの場合は要素確保
            result.resize(rows(), cols());
        }

        T det = determinant();
        if (det == 0) {
            throw std::invalid_argument("Matrix is singular and cannot be inverted");
        }
        if (rows() == 2 && cols() == 2) {
            result(0, 0) =  m(1, 1);
            result(0, 1) = -m(0, 1);
            result(1, 0) = -m(1, 0);
            result(1, 1) =  m(0, 0);
        } else if (rows() == 3 && cols() == 3) {
            result(0, 0) = m(1, 1)*m(2, 2) - m(1, 2)*m(2, 1);
            result(0, 1) = m(0, 2)*m(2, 1) - m(0, 1)*m(2, 2);
            result(0, 2) = m(0, 1)*m(1, 2) - m(0, 2)*m(1, 1);
            result(1, 0) = m(1, 2)*m(2, 0) - m(1, 0)*m(2, 2);
            result(1, 1) = m(0, 0)*m(2, 2) - m(0, 2)*m(2, 0);
            result(1, 2) = m(0, 2)*m(1, 0) - m(0, 0)*m(1, 2);
            result(2, 0) = m(1, 0)*m(2, 1) - m(1, 1)*m(2, 0);
            result(2, 1) = m(0, 1)*m(2, 0) - m(0, 0)*m(2, 1);
            result(2, 2) = m(0, 0)*m(1, 1) - m(0, 1)*m(1, 0);
        } else if (rows() == 4 && cols() == 4) {
            result(0, 0) = + m(1, 1)*(m(2, 2)*m(3, 3) - m(2, 3)*m(3, 2))
                           - m(1, 2)*(m(2, 1)*m(3, 3) - m(2, 3)*m(3, 1))
                           + m(1, 3)*(m(2, 1)*m(3, 2) - m(2, 2)*m(3, 1));
            result(0, 1) = - m(0, 1)*(m(2, 2)*m(3, 3) - m(2, 3)*m(3, 2))
                           + m(0, 2)*(m(2, 1)*m(3, 3) - m(2, 3)*m(3, 1))
                           - m(0, 3)*(m(2, 1)*m(3, 2) - m(2, 2)*m(3, 1));
            result(0, 2) = + m(0, 1)*(m(1, 2)*m(3, 3) - m(1, 3)*m(3, 2))
                           - m(0, 2)*(m(1, 1)*m(3, 3) - m(1, 3)*m(3, 1))
                           + m(0, 3)*(m(1, 1)*m(3, 2) - m(1, 2)*m(3, 1));
            result(0, 3) = - m(0, 1)*(m(1, 2)*m(2, 3) - m(1, 3)*m(2, 2))
                           + m(0, 2)*(m(1, 1)*m(2, 3) - m(1, 3)*m(2, 1))
                           - m(0, 3)*(m(1, 1)*m(2, 2) - m(1, 2)*m(2, 1));

            result(1, 0) = - m(1, 0)*(m(2, 2)*m(3, 3) - m(2, 3)*m(3, 2))
                           + m(1, 2)*(m(2, 0)*m(3, 3) - m(2, 3)*m(3, 0))
                           - m(1, 3)*(m(2, 0)*m(3, 2) - m(2, 2)*m(3, 0));
            result(1, 1) = + m(0, 0)*(m(2, 2)*m(3, 3) - m(2, 3)*m(3, 2))
                           - m(0, 2)*(m(2, 0)*m(3, 3) - m(2, 3)*m(3, 0))
                           + m(0, 3)*(m(2, 0)*m(3, 2) - m(2, 2)*m(3, 0));
            result(1, 2) = - m(0, 0)*(m(1, 2)*m(3, 3) - m(1, 3)*m(3, 2))
                           + m(0, 2)*(m(1, 0)*m(3, 3) - m(1, 3)*m(3, 0))
                           - m(0, 3)*(m(1, 0)*m(3, 2) - m(1, 2)*m(3, 0));
            result(1, 3) = + m(0, 0)*(m(1, 2)*m(2, 3) - m(1, 3)*m(2, 2))
                           - m(0, 2)*(m(1, 0)*m(2, 3) - m(1, 3)*m(2, 0))
                           + m(0, 3)*(m(1, 0)*m(2, 2) - m(1, 2)*m(2, 0));

            result(2, 0) = + m(1, 0)*(m(2, 1)*m(3, 3) - m(2, 3)*m(3, 1))
                           - m(1, 1)*(m(2, 0)*m(3, 3) - m(2, 3)*m(3, 0))
                           + m(1, 3)*(m(2, 0)*m(3, 1) - m(2, 1)*m(3, 0));
            result(2, 1) = - m(0, 0)*(m(2, 1)*m(3, 3) - m(2, 3)*m(3, 1))
                           + m(0, 1)*(m(2, 0)*m(3, 3) - m(2, 3)*m(3, 0))
                           - m(0, 3)*(m(2, 0)*m(3, 1) - m(2, 1)*m(3, 0));
            result(2, 2) = + m(0, 0)*(m(1, 1)*m(3, 3) - m(1, 3)*m(3, 1))
                           - m(0, 1)*(m(1, 0)*m(3, 3) - m(1, 3)*m(3, 0))
                           + m(0, 3)*(m(1, 0)*m(3, 1) - m(1, 1)*m(3, 0));
            result(2, 3) = - m(0, 0)*(m(1, 1)*m(2, 3) - m(1, 3)*m(2, 1))
                           + m(0, 1)*(m(1, 0)*m(2, 3) - m(1, 3)*m(2, 0))
                           - m(0, 3)*(m(1, 0)*m(2, 1) - m(1, 1)*m(2, 0));

            result(3, 0) = - m(1, 0)*(m(2, 1)*m(3, 2) - m(2, 2)*m(3, 1))
                           + m(1, 1)*(m(2, 0)*m(3, 2) - m(2, 2)*m(3, 0))
                           - m(1, 2)*(m(2, 0)*m(3, 1) - m(2, 1)*m(3, 0));
            result(3, 1) = + m(0, 0)*(m(2, 1)*m(3, 2) - m(2, 2)*m(3, 1))
                           - m(0, 1)*(m(2, 0)*m(3, 2) - m(2, 2)*m(3, 0))
                           + m(0, 2)*(m(2, 0)*m(3, 1) - m(2, 1)*m(3, 0));
            result(3, 2) = - m(0, 0)*(m(1, 1)*m(3, 2) - m(1, 2)*m(3, 1))
                           + m(0, 1)*(m(1, 0)*m(3, 2) - m(1, 2)*m(3, 0))
                           - m(0, 2)*(m(1, 0)*m(3, 1) - m(1, 1)*m(3, 0));
            result(3, 3) = + m(0, 0)*(m(1, 1)*m(2, 2) - m(1, 2)*m(2, 1))
                           - m(0, 1)*(m(1, 0)*m(2, 2) - m(1, 2)*m(2, 0))
                           + m(0, 2)*(m(1, 0)*m(2, 1) - m(1, 1)*m(2, 0));
        } else if (rows() != cols()) {
            // 正方行列のみサポート
            throw std::invalid_argument("Inverse is only implemented for square matrices");
        } else {
            throw NotImplementedError(
                    "Inverse is only implemented for 2x2, 3x3 and 4x4 matrices");
        }

        return result / det;
    }


    /**
     * 静的メンバ関数
     */

    /// @brief 全ての要素が指定された値で初期化された行列を生成 (動的サイズ版)
    /// @param new_rows 行数
    /// @param new_cols 列数
    /// @param value 初期化する値
    /// @return 初期化された行列
    /// @throw std::invalid_argument new_rowsがNと異なる場合、またはnew_colsがMと異なる場合
    template<int N_ = N>
    static typename std::enable_if_t<N_ == Dynamic || M == Dynamic, Matrix<T, N, M>>
    Constant(size_t new_rows, size_t new_cols, T value) {
        if ((N != Dynamic && static_cast<size_t>(N) != new_rows) ||
            (M != Dynamic && static_cast<size_t>(M) != new_cols)) {
            throw std::invalid_argument("Incompatible matrix dimensions");
        }

        Matrix<T, N, M> result(new_rows, new_cols);
        for (size_t j = 0; j < new_cols; ++j) {
            for (size_t i = 0; i < new_rows; ++i) {
                result(i, j) = value;
            }
        }
        return result;
    }

    /// @brief 全ての要素が指定された値で初期化された行列を生成 (固定サイズ版)
    /// @param value 初期化する値
    /// @return 初期化された行列
    template<int N_ = N>
    static typename std::enable_if_t<N_ != Dynamic && M != Dynamic, Matrix<T, N, M>>
    Constant(T value) {
        Matrix<T, N, M> result;
        for (size_t j = 0; j < M; ++j) {
            for (size_t i = 0; i < N; ++i) {
                result(i, j) = value;
            }
        }
        return result;
    }

    /// @brief ゼロ行列を作成
    /// @param new_rows 行数
    /// @param new_cols 列数
    /// @return ゼロ行列
    /// @throw std::invalid_argument new_rowsがNと異なる場合、またはnew_colsがMと異なる場合
    template<int N_ = N>
    static typename std::enable_if_t<N_ == Dynamic || M == Dynamic, Matrix<T, N, M>>
    Zero(size_t new_rows, size_t new_cols) {
        return Constant(new_rows, new_cols, 0.0);
    }

    /// @brief ゼロ行列を作成 (固定サイズ版)
    /// @return ゼロ行列
    template<int N_ = N>
    static typename std::enable_if_t<N_ != Dynamic && M != Dynamic, Matrix<T, N, M>>
    Zero() { return Constant(0.0); }

    /// @brief 単位行列を作成 (動的サイズ版)
    /// @param new_rows 行数
    /// @param new_cols 列数
    /// @return 単位行列
    template<int N_ = N>
    static typename std::enable_if_t<N_ == Dynamic || M == Dynamic, Matrix<T, N, M>>
    Identity(size_t new_rows, size_t new_cols) {
        Matrix<T, N, M> result = Zero(new_rows, new_cols);
        for (size_t i = 0; i < std::min(new_rows, new_cols); ++i) {
            result(i, i) = static_cast<T>(1);
        }
        return result;
    }

    /// @brief 単位行列を作成 (固定サイズ版)
    /// @return 単位行列
    template<int N_ = N>
    static typename std::enable_if_t<N_ != Dynamic && N_ == M, Matrix<T, N, M>>
    Identity() {
        Matrix<T, N, M> result = Zero();
        for (size_t i = 0; i < static_cast<size_t>(N); ++i) {
            result(i, i) = static_cast<T>(1);
        }
        return result;
    }

    /// @brief 単位ベクトルを作成
    /// @param i 非ゼロ要素のインデックス (0-based)
    template<int N_ = N, int M_ = M>
    static typename std::enable_if_t<
            (N_ == 1 && M_ != Dynamic) || (N_ != Dynamic && M_ == 1), Matrix<T, N, M>>
    Unit(size_t i) {
        if (i >= std::max(static_cast<size_t>(N), static_cast<size_t>(M))) {
            throw std::out_of_range("Index out of range for unit vector");
        }
        Matrix<T, N, M> result = Zero();
        if (N == 1) {
            result(0, i) = 1.0;
        } else {
            result(i, 0) = 1.0;
        }
        return result;
    }
    /// @brief 単位ベクトル (1, 0, 0) を作成
    template<int N_ = N>
    static typename std::enable_if_t<(N_ == 3 && M == 1), Matrix<T, 3, 1>>
    UnitX() {
        return Matrix<T, 3, 1>{ 1.0, 0.0, 0.0 };
    }
    /// @brief 単位ベクトル (0, 1, 0) を作成
    template<int N_ = N>
    static typename std::enable_if_t<(N_ == 3 && M == 1), Matrix<T, 3, 1>>
    UnitY() {
        return Matrix<T, 3, 1>{ 0.0, 1.0, 0.0 };
    }
    /// @brief 単位ベクトル (0, 0, 1) を作成
    template<int N_ = N>
    static typename std::enable_if_t<(N_ == 3 && M == 1), Matrix<T, 3, 1>>
    UnitZ() {
        return Matrix<T, 3, 1>{ 0.0, 0.0, 1.0 };
    }
};

/// @brief スカラー・行列乗算（非メンバ関数）
/// @tparam T 行列の要素型
/// @tparam N 行数
/// @tparam M 列数
/// @param scalar 乗算するスカラー値
/// @param mat 乗算する行列
/// @return 乗算結果の行列
template<typename T, int N, int M>
Matrix<T, N, M> operator*(T scalar, const Matrix<T, N, M>& mat) {
    return mat * scalar;
}

/// @brief 出力演算子
/// @param os 出力ストリーム
/// @param matrix 行列
/// @return 出力ストリーム
/// @note Eigenのデフォルトのフォーマットと異なることに注意.
///       2行3列の行列は`((1, 2, 3), (4, 5, 6))`のように出力される.
template<typename T, int N, int M>
std::ostream& operator<<(std::ostream& os, const Matrix<T, N, M>& matrix) {
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



/// @brief 縦ベクトルの型エイリアス
/// @tparam T ベクトルの要素型
/// @tparam N ベクトルの次元数
template<typename T, int N>
using Vector = Matrix<T, N, 1>;
/// @brief 横ベクトルの型エイリアス
/// @tparam T ベクトルの要素型
/// @tparam M ベクトルの次元数
template<typename T, int M>
using RowVector = Matrix<T, 1, M>;

/// @brief 2x2行列の型エイリアス (float型)
using Matrix2f = Matrix<float, 2, 2>;
/// @brief 2x3行列の型エイリアス (float型)
using Matrix23f = Matrix<float, 2, 3>;
/// @brief 2x4行列の型エイリアス (float型)
using Matrix24f = Matrix<float, 2, 4>;
/// @brief 2行・動的列数行列の型エイリアス (float型)
using Matrix2Xf = Matrix<float, 2, Dynamic>;
/// @brief 3x3行列の型エイリアス (float型)
using Matrix3f = Matrix<float, 3, 3>;
/// @brief 3x2行列の型エイリアス (float型)
using Matrix32f = Matrix<float, 3, 2>;
/// @brief 3x4行列の型エイリアス (float型)
using Matrix34f = Matrix<float, 3, 4>;
/// @brief 3行・動的列数行列の型エイリアス (float型)
using Matrix3Xf = Matrix<float, 3, Dynamic>;
/// @brief 4x4行列の型エイリアス (float型)
using Matrix4f = Matrix<float, 4, 4>;
/// @brief 4x2行列の型エイリアス (float型)
using Matrix42f = Matrix<float, 4, 2>;
/// @brief 4x3行列の型エイリアス (float型)
using Matrix43f = Matrix<float, 4, 3>;
/// @brief 4行・動的列数行列の型エイリアス (float型)
using Matrix4Xf = Matrix<float, 4, Dynamic>;
/// @brief 動的行数・2列行列の型エイリアス (float型)
using MatrixX2f = Matrix<float, Dynamic, 2>;
/// @brief 動的行数・3列行列の型エイリアス (float型)
using MatrixX3f = Matrix<float, Dynamic, 3>;
/// @brief 動的行数・4列行列の型エイリアス (float型)
using MatrixX4f = Matrix<float, Dynamic, 4>;
/// @brief 動的行数・動的列数行列の型エイリアス (float型)
using MatrixXf = Matrix<float, Dynamic, Dynamic>;
/// @brief 縦2次元ベクトルの型エイリアス (float型)
using Vector2f = Matrix<float, 2, 1>;
/// @brief 縦3次元ベクトルの型エイリアス (float型)
using Vector3f = Matrix<float, 3, 1>;
/// @brief 縦4次元ベクトルの型エイリアス (float型)
using Vector4f = Matrix<float, 4, 1>;
/// @brief 縦動的次元ベクトルの型エイリアス (float型)
using VectorXf = Matrix<float, Dynamic, 1>;
/// @brief 横2次元ベクトルの型エイリアス (float型)
using RowVector2f = Matrix<float, 1, 2>;
/// @brief 横3次元ベクトルの型エイリアス (float型)
using RowVector3f = Matrix<float, 1, 3>;
/// @brief 横4次元ベクトルの型エイリアス (float型)
using RowVector4f = Matrix<float, 1, 4>;
/// @brief 横動的次元ベクトルの型エイリアス (float型)
using RowVectorXf = Matrix<float, 1, Dynamic>;

/// @brief 2x2行列の型エイリアス (double型)
using Matrix2d = Matrix<double, 2, 2>;
/// @brief 2x3行列の型エイリアス (double型)
using Matrix23d = Matrix<double, 2, 3>;
/// @brief 2x4行列の型エイリアス (double型)
using Matrix24d = Matrix<double, 2, 4>;
/// @brief 2行・動的列数行列の型エイリアス (double型)
using Matrix2Xd = Matrix<double, 2, Dynamic>;
/// @brief 3x3行列の型エイリアス (double型)
using Matrix3d = Matrix<double, 3, 3>;
/// @brief 3x2行列の型エイリアス (double型)
using Matrix32d = Matrix<double, 3, 2>;
/// @brief 3x4行列の型エイリアス (double型)
using Matrix34d = Matrix<double, 3, 4>;
/// @brief 3行・動的列数行列の型エイリアス (double型)
using Matrix3Xd = Matrix<double, 3, Dynamic>;
/// @brief 4x4行列の型エイリアス (double型)
using Matrix4d = Matrix<double, 4, 4>;
/// @brief 4x2行列の型エイリアス (double型)
using Matrix42d = Matrix<double, 4, 2>;
/// @brief 4x3行列の型エイリアス (double型)
using Matrix43d = Matrix<double, 4, 3>;
/// @brief 4行・動的列数行列の型エイリアス (double型)
using Matrix4Xd = Matrix<double, 4, Dynamic>;
/// @brief 動的行数・2列行列の型エイリアス (double型)
using MatrixX2d = Matrix<double, Dynamic, 2>;
/// @brief 動的行数・3列行列の型エイリアス (double型)
using MatrixX3d = Matrix<double, Dynamic, 3>;
/// @brief 動的行数・4列行列の型エイリアス (double型)
using MatrixX4d = Matrix<double, Dynamic, 4>;
/// @brief 動的行数・動的列数行列の型エイリアス (double型)
using MatrixXd = Matrix<double, Dynamic, Dynamic>;
/// @brief 縦2次元ベクトルの型エイリアス (double型)
using Vector2d = Matrix<double, 2, 1>;
/// @brief 縦3次元ベクトルの型エイリアス (double型)
using Vector3d = Matrix<double, 3, 1>;
/// @brief 縦4次元ベクトルの型エイリアス (double型)
using Vector4d = Matrix<double, 4, 1>;
/// @brief 縦動的次元ベクトルの型エイリアス (double型)
using VectorXd = Matrix<double, Dynamic, 1>;
/// @brief 横2次元ベクトルの型エイリアス (double型)
using RowVector2d = Matrix<double, 1, 2>;
/// @brief 横3次元ベクトルの型エイリアス (double型)
using RowVector3d = Matrix<double, 1, 3>;
/// @brief 横4次元ベクトルの型エイリアス (double型)
using RowVector4d = Matrix<double, 1, 4>;
/// @brief 横動的次元ベクトルの型エイリアス (double型)
using RowVectorXd = Matrix<double, 1, Dynamic>;

/// @brief 回転角度と回転軸から回転行列を生成する関数
/// @param angle 回転角度 [rad]
/// @param axis 回転軸 (単位ベクトル)
/// @return 回転行列
/// @throw std::invalid_argument 回転軸がゼロベクトルの場合
/// @note Eigenと異なり、関数であることに注意.
template<typename T>
Matrix<T, 3, 3> AngleAxis(const T angle, const Vector<T, 3>& axis) {
    auto norm = axis.norm();
    if (norm == 0) {
        throw std::invalid_argument("Rotation axis cannot be a zero vector");
    }

    T x = axis(0), y = axis(1), z = axis(2);
    T c = std::cos(angle);
    T s = std::sin(angle);
    T t = 1 - c;

    Matrix<T, 3, 3> rotation_matrix;
    rotation_matrix(0, 0) = t * x * x + c;
    rotation_matrix(0, 1) = t * x * y - s * z;
    rotation_matrix(0, 2) = t * x * z + s * y;
    rotation_matrix(1, 0) = t * y * x + s * z;
    rotation_matrix(1, 1) = t * y * y + c;
    rotation_matrix(1, 2) = t * y * z - s * x;
    rotation_matrix(2, 0) = t * z * x - s * y;
    rotation_matrix(2, 1) = t * z * y + s * x;
    rotation_matrix(2, 2) = t * z * z + c;

    return rotation_matrix;
}

/// @brief 回転角度と回転軸から回転行列を生成する関数 (double型版)
/// @param angle 回転角度 [rad]
/// @param axis 回転軸 (単位ベクトル)
/// @return 回転行列
/// @throw std::invalid_argument 回転軸がゼロベクトルの場合
/// @note Eigenと異なり、関数であることに注意.
inline auto AngleAxisd = [](double angle, const Vector<double, 3>& axis) {
    return AngleAxis<double>(angle, axis);
};
/// @brief 回転角度と回転軸から回転行列を生成する関数 (float型版)
/// @param angle 回転角度 [rad]
/// @param axis 回転軸 (単位ベクトル)
/// @return 回転行列
/// @throw std::invalid_argument 回転軸がゼロベクトルの場合
/// @note Eigenと異なり、関数であることに注意.
inline auto AngleAxisf = [](float angle, const Vector<float, 3>& axis) {
    return AngleAxis<float>(angle, axis);
};

}  // namespace igesio

#endif  // IGESIO_ENABLE_EIGEN

namespace igesio {

/// @brief ベクトル/行列を文字列化する
/// @param mat ベクトル/行列
/// @param transpose 転置して表示するかどうか.
///        例えば縦ベクトルの場合、trueならば`(1, 2, 3)^T`のように表示される.
template<typename T, int N, int M>
std::string ToString(const Matrix<T, N, M>& mat, bool transpose = false) {
    std::ostringstream oss;
    if (transpose) {
        oss << mat.transpose() << "^T";
    } else {
        oss << mat;
    }
    return oss.str();
}



/**
 * ユーティリティ関数
 */

/// @brief 2つのベクトル間の角度をラジアンで計算する
/// @param a ベクトル1
/// @param b ベクトル2
/// @param in_degrees 角度を度で取得する場合は`true`、ラジアンで取得する場合は`false`(デフォルト)
/// @return ベクトルaとbのなす角
/// @throw std::invalid_argument ベクトルのサイズが異なる場合、またはゼロベクトルが含まれる場合
template<typename T, int N1, int N2>
double AngleBetween(const Vector<T, N1>& a, const Vector<T, N2>& b,
                    bool in_degrees = false) {
    // 単位ベクトルに正規化
    double na = a.norm();
    double nb = b.norm();
    if (na == 0.0 || nb == 0.0) {
        throw std::invalid_argument("Cannot compute angle with zero-length vector.");
    }
    // サイズが異なる場合は例外を投げる
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vectors must have the same dimension.");
    }

    // 内積を計算 ([-1, 1]の範囲に収める)
    double cos_theta = a.dot(b) / (na * nb);
    cos_theta = std::clamp(cos_theta, -1.0, 1.0);
    double angle_rad = std::acos(cos_theta);
    if (in_degrees) {
        return angle_rad * (180.0 / kPi);
    } else {
        return angle_rad;
    }
}

}  // namespace igesio

#endif  // IGESIO_NUMERICS_MATRIX_H_
