/**
 * @file common/test_matrix.cpp
 * @brief Matrixクラスのテスト
 * @author Yayoi Habami
 * @date 2025-06-07
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <initializer_list>
#include <string>

#include "igesio/common/matrix.h"


namespace {

using InitMatrix = std::initializer_list<std::initializer_list<double>>;
using InitVector = std::initializer_list<double>;

/// @brief 1次元の初期化子リストを文字列に変換するヘルパー関数
/// @param init_list 1次元の初期化子リスト
/// @return 変換された文字列 `(1, 2, 3)` のような形式
std::string ToString(const InitVector& init_list) {
    std::string result = "(";
    for (const auto& elem : init_list) {
        result += std::to_string(elem);
        if (&elem != &*std::prev(init_list.end())) {  // 最後の要素でない場合
            result += ", ";
        }
    }
    result += ")";
    return result;
}

/// @brief 入れ子形式の初期化子リストを文字列に変換するヘルパー関数
/// @param init_list 入れ子形式の初期化子リスト
/// @return 変換された文字列 `((1, 2, 3), (4, 5, 6))` のような形式
std::string ToString(const InitMatrix& init_list) {
    std::string result = "(";
    for (const auto& row : init_list) {
        result += "(";
        for (const auto& elem : row) {
            result += std::to_string(elem);
            if (&elem != &*std::prev(row.end())) {
                // 最後の要素でない場合
                result += ", ";
            }
        }
        result += ")";
        if (&row != &*std::prev(init_list.end())) {
            // 最後の行でない場合
            result += ", ";
        }
    }
    result += ")";
    return result;
}

/// @brief Matrix型の各要素と初期化リストの各要素が等しいかを検証する
/// @param mat 検証対象のMatrix型オブジェクト
/// @param ref 比較対象の初期化リスト
template<int N, int M>
void ValidateMatrixElements(
        const igesio::detail::Matrix<N, M>& mat, const InitMatrix& ref) {
    // サイズの事前検証
    ASSERT_EQ(mat.rows(), static_cast<int>(ref.size()))
        << "Matrix row count does not match initializer list size"
        << " (expected: " << ToString(ref) << ", actual: " << mat << ")";

    size_t row_idx = 0;
    for (const auto& row : ref) {
        ASSERT_EQ(mat.cols(), static_cast<int>(row.size()))
            << "Matrix column count does not match initializer list row size at row " << row_idx
            << " (expected: " << row.size() << ", actual: " << mat.cols() << ")";

        size_t col_idx = 0;
        for (const auto& expected_value : row) {
            EXPECT_DOUBLE_EQ(mat(row_idx, col_idx), expected_value)
                << "Matrix element mismatch at position (" << row_idx << ", " << col_idx
                << "): expected value" << expected_value
                <<", actual value" << mat(row_idx, col_idx)
                << " (expected: " << ToString(ref) << ", actual: " << mat << ")";
            ++col_idx;
        }
        ++row_idx;
    }
}

/// @brief Vector型の各要素と初期化リストの各要素が等しいかを検証する
/// @param vec 検証対象のVector型オブジェクト
/// @param ref 比較対象の初期化リスト
template<int N, int M>
void ValidateColVectorElements(
        const igesio::detail::Matrix<N, M>& vec, const InitVector& ref) {
    if (vec.cols() != 1) {
        FAIL() << "Expected a column vector (single column), but got " << vec.cols() << " columns."
               << " (expected: " << ToString(ref) << ", actual: " << vec << ")";
    }

    ASSERT_EQ(vec.rows(), static_cast<int>(ref.size()))
        << "Vector row count does not match initializer list size"
        << " (expected: " << ToString(ref) << ", actual: " << vec << ")";

    size_t row_idx = 0;
    for (const auto& expected_value : ref) {
        EXPECT_DOUBLE_EQ(vec(row_idx, 0), expected_value)
            << "Vector element mismatch at position (" << row_idx << ", 0): expected value "
            << expected_value << ", actual value " << vec(row_idx, 0)
            << " (expected: " << ToString(ref) << ", actual: " << vec << ")";
        ++row_idx;
    }
}

}  // namespace



/**
 * 静的メンバ関数のテスト
 */

TEST(MatrixStaticMethodsTest, Constant) {
    // 固定サイズ版のテスト
    auto mat3x3 = igesio::Matrix3d::Constant(5.0);
    ValidateMatrixElements(mat3x3, {{5.0, 5.0, 5.0}, {5.0, 5.0, 5.0}, {5.0, 5.0, 5.0}});

    auto mat2x3 = igesio::Matrix23d::Constant(-2.5);
    ValidateMatrixElements(mat2x3, {{-2.5, -2.5, -2.5}, {-2.5, -2.5, -2.5}});

    // 動的サイズ版のテスト
    auto mat3xDyn = igesio::Matrix3Xd::Constant(3, 2, 7.0);
    ValidateMatrixElements(mat3xDyn, {{7.0, 7.0}, {7.0, 7.0}, {7.0, 7.0}});

    auto mat2xDyn = igesio::Matrix2Xd::Constant(2, 5, 0.0);
    ValidateMatrixElements(mat2xDyn, {{0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0, 0.0}});

    // エラーケースのテスト - 不正な行数
    EXPECT_THROW((igesio::Matrix3Xd::Constant(2, 3, 1.0)),  // rowsがNと異なる
                 std::invalid_argument);

    EXPECT_THROW((igesio::Matrix2Xd::Constant(5, 1, 1.0)),  // rowsがNと異なる
                 std::invalid_argument);
}

TEST(MatrixStaticMethodsTest, Zero) {
    // 固定サイズ版のテスト
    auto mat3x3 = igesio::Matrix3d::Zero();
    ValidateMatrixElements(mat3x3, {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}});

    auto mat2x3 = igesio::Matrix23d::Zero();
    ValidateMatrixElements(mat2x3, {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}});

    // 動的サイズ版のテスト
    auto mat3xDyn = igesio::Matrix3Xd::Zero(3, 2);
    ValidateMatrixElements(mat3xDyn, {{0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}});

    auto mat2xDyn = igesio::Matrix2Xd::Zero(2, 5);
    ValidateMatrixElements(mat2xDyn, {{0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0, 0.0}});

    // エラーケースのテスト - 不正な行数
    EXPECT_THROW((igesio::Matrix3Xd::Zero(2, 3)),  // rowsがNと異なる
                 std::invalid_argument);

    EXPECT_THROW((igesio::Matrix2Xd::Zero(5, 1)),  // rowsがNと異なる
                 std::invalid_argument);
}

TEST(MatrixStaticMethodsTest, Identity) {
    // 固定サイズ版のテスト
    auto mat3x3 = igesio::Matrix3d::Identity();
    ValidateMatrixElements(mat3x3, {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}});

    auto mat2x2 = igesio::Matrix2d::Identity();
    ValidateMatrixElements(mat2x2, {{1.0, 0.0}, {0.0, 1.0}});

    // 動的サイズ版のテスト
    auto mat3xDyn = igesio::Matrix3Xd::Identity(3, 2);
    ValidateMatrixElements(mat3xDyn, {{1.0, 0.0}, {0.0, 1.0}, {0.0, 0.0}});

    auto mat2xDyn = igesio::Matrix2Xd::Identity(2, 5);
    ValidateMatrixElements(mat2xDyn, {{1.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0, 0.0}});

    // エラーケースのテスト - 不正な行数
    EXPECT_THROW((igesio::Matrix3Xd::Identity(2, 3)),  // rowsがNと異なる
                 std::invalid_argument);

    EXPECT_THROW((igesio::Matrix2Xd::Identity(5, 1)),  // rowsがNと異なる
                 std::invalid_argument);
}



/**
 * コンストラクタのテスト
 */

TEST(MatrixConstructorTest, DefaultConstructor) {
    // 固定サイズの行列
    igesio::Matrix23d mat2x3;
    ASSERT_EQ(mat2x3.rows(), 2);
    ASSERT_EQ(mat2x3.cols(), 3);
    ASSERT_EQ(mat2x3.size(), 6);
    ValidateMatrixElements(mat2x3, {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}});

    // 動的サイズの行列
    igesio::Matrix3Xd mat3xDyn;
    ASSERT_EQ(mat3xDyn.rows(), 3);
    ASSERT_EQ(mat3xDyn.cols(), 0);  // 初期状態では列数は未設定
    ASSERT_EQ(mat3xDyn.size(), 0);

    // 動的サイズの行列に対して列数を設定
    igesio::Matrix3Xd mat3xDyn2(3, 4);
    ASSERT_EQ(mat3xDyn2.rows(), 3);
    ASSERT_EQ(mat3xDyn2.cols(), 4);
    ASSERT_EQ(mat3xDyn2.size(), 12);
    ValidateMatrixElements(mat3xDyn2, {{0.0, 0.0, 0.0, 0.0},
                                       {0.0, 0.0, 0.0, 0.0},
                                       {0.0, 0.0, 0.0, 0.0}});
}

// 初期化リストを使用したコンストラクタのテスト
TEST(MatrixConstructorTest, InitializerListConstructor) {
    // ベクトル
    igesio::Vector2d vec2x1 = {1.0, 2.0};
    ValidateColVectorElements(vec2x1, {1.0, 2.0});
    igesio::Vector3d vec3x1 = {1.0, 2.0, 3.0};
    ValidateColVectorElements(vec3x1, {1.0, 2.0, 3.0});

    // 固定サイズの行列
    igesio::Matrix23d mat2x3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    ASSERT_EQ(mat2x3.rows(), 2);
    ASSERT_EQ(mat2x3.cols(), 3);
    ASSERT_EQ(mat2x3.size(), 6);
    ValidateMatrixElements(mat2x3, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}});

    // 動的サイズの行列
    igesio::Matrix3Xd mat3xDyn = {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}};
    ASSERT_EQ(mat3xDyn.rows(), 3);
    ASSERT_EQ(mat3xDyn.cols(), 2);
    ASSERT_EQ(mat3xDyn.size(), 6);
    ValidateMatrixElements(mat3xDyn, {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}});

    // 動的サイズの行列に対し、異なる列数を指定すると行数が変化する
    mat3xDyn = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}};
    ASSERT_EQ(mat3xDyn.rows(), 3);
    ASSERT_EQ(mat3xDyn.cols(), 3);
    ASSERT_EQ(mat3xDyn.size(), 9);
    ValidateMatrixElements(mat3xDyn, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}});

    // エラーケース - 行数がNと異なる
    EXPECT_THROW((igesio::Matrix2Xd({{1.0, 2.0}, {3.0}})), std::invalid_argument);

    // エラーケース - 列数がMと異なる（固定サイズの場合）
    EXPECT_THROW((igesio::Matrix23d({{1.0, 2.0}, {3.0}})), std::invalid_argument);
}



/**
 * 基本的なメンバ関数（演算子オーバーロードを除く）のテスト
 */

// rows, cols, sizeのテスト
TEST(MatrixMethodsTest, RowsColsSize) {
    igesio::Matrix32d mat3x2;
    ASSERT_EQ(mat3x2.rows(), 3);
    ASSERT_EQ(mat3x2.cols(), 2);
    ASSERT_EQ(mat3x2.size(), 6);

    igesio::Matrix2Xd mat2xDyn(2, 5);
    ASSERT_EQ(mat2xDyn.rows(), 2);
    ASSERT_EQ(mat2xDyn.cols(), 5);
    ASSERT_EQ(mat2xDyn.size(), 10);
}

// 要素アクセスのテスト (ベクトル版: M=1)
TEST(MatrixMethodsTest, VectorElementAccess) {
    igesio::Vector3d vec3x1;
    vec3x1(0) = 1.0;
    vec3x1(1) = 2.0;
    vec3x1(2) = 3.0;

    ValidateColVectorElements(vec3x1, {1.0, 2.0, 3.0});
}

// 要素アクセスのテスト (行列版: M>1)
TEST(MatrixMethodsTest, MatrixElementAccess) {
    igesio::Matrix23d mat2x3;
    mat2x3(0, 0) = 1.0;
    mat2x3(0, 1) = 2.0;
    mat2x3(0, 2) = 3.0;
    mat2x3(1, 0) = 4.0;
    mat2x3(1, 1) = 5.0;
    mat2x3(1, 2) = 6.0;

    ValidateMatrixElements(mat2x3, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}});
}

// サイズ変更のテスト
TEST(MatrixMethodsTest, ConservativeResize) {
    igesio::Matrix2Xd mat2xDyn(2, 3);
    mat2xDyn(0, 0) = 1.0;
    mat2xDyn(0, 1) = 2.0;
    mat2xDyn(0, 2) = 3.0;
    mat2xDyn(1, 0) = 4.0;
    mat2xDyn(1, 1) = 5.0;
    mat2xDyn(1, 2) = 6.0;

    // 列数を変更
    mat2xDyn.conservativeResize(igesio::NoChange, 5);
    ASSERT_EQ(mat2xDyn.cols(), 5);
    ASSERT_EQ(mat2xDyn.size(), 10);
    ValidateMatrixElements(mat2xDyn, {{1.0, 2.0, 3.0, 0.0, 0.0},
                                       {4.0, 5.0, 6.0, 0.0, 0.0}});

    // 行数を変更しない
    EXPECT_THROW((mat2xDyn.conservativeResize(3, igesio::NoChange)),
                 std::invalid_argument);
}



/**
 * 演算子オーバーロードのテスト
 */

// 行列加算のテスト
TEST(MatrixOperatorTest, Addition) {
    // 固定サイズ行列の加算
    auto mat1 = igesio::Matrix23d::Constant(1.0);
    auto mat2 = igesio::Matrix23d::Constant(2.0);
    auto result = mat1 + mat2;
    ValidateMatrixElements(result, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // 動的サイズ行列の加算
    auto mat3 = igesio::Matrix2Xd::Constant(2, 3, 1.5);
    auto mat4 = igesio::Matrix2Xd::Constant(2, 3, 2.5);
    auto result2 = mat3 + mat4;
    ValidateMatrixElements(result2, {{4.0, 4.0, 4.0}, {4.0, 4.0, 4.0}});

    // 列数不一致エラー（動的サイズ）
    auto mat5 = igesio::Matrix2Xd(2, 2);
    auto mat6 = igesio::Matrix2Xd(2, 3);
    EXPECT_THROW(mat5 + mat6, std::invalid_argument);
}

// 行列加算代入のテスト
TEST(MatrixOperatorTest, AdditionAssignment) {
    // 固定サイズ行列の加算代入
    auto mat1 = igesio::Matrix23d::Constant(1.0);
    auto mat2 = igesio::Matrix23d::Constant(2.0);
    mat1 += mat2;
    ValidateMatrixElements(mat1, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // 動的サイズ行列の加算代入
    auto mat3 = igesio::Matrix2Xd::Constant(2, 3, 1.5);
    auto mat4 = igesio::Matrix2Xd::Constant(2, 3, 2.5);
    mat3 += mat4;
    ValidateMatrixElements(mat3, {{4.0, 4.0, 4.0}, {4.0, 4.0, 4.0}});

    // 列数不一致エラー（動的サイズ）
    auto mat5 = igesio::Matrix2Xd(2, 2);
    auto mat6 = igesio::Matrix2Xd(2, 3);
    EXPECT_THROW(mat5 += mat6, std::invalid_argument);
}

// 行列減算のテスト
TEST(MatrixOperatorTest, Subtraction) {
    // 固定サイズ行列の減算
    auto mat1 = igesio::Matrix23d::Constant(5.0);
    auto mat2 = igesio::Matrix23d::Constant(2.0);
    auto result = mat1 - mat2;
    ValidateMatrixElements(result, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // 動的サイズ行列の減算
    auto mat3 = igesio::Matrix2Xd::Constant(2, 3, 4.5);
    auto mat4 = igesio::Matrix2Xd::Constant(2, 3, 1.5);
    auto result2 = mat3 - mat4;
    ValidateMatrixElements(result2, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // 列数不一致エラー（動的サイズ）
    auto mat5 = igesio::Matrix2Xd(2, 2);
    auto mat6 = igesio::Matrix2Xd(2, 3);
    EXPECT_THROW(mat5 - mat6, std::invalid_argument);
}

// 行列減算代入のテスト
TEST(MatrixOperatorTest, SubtractionAssignment) {
    // 固定サイズ行列の減算代入
    auto mat1 = igesio::Matrix23d::Constant(5.0);
    auto mat2 = igesio::Matrix23d::Constant(2.0);
    mat1 -= mat2;
    ValidateMatrixElements(mat1, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // 動的サイズ行列の減算代入
    auto mat3 = igesio::Matrix2Xd::Constant(2, 3, 4.5);
    auto mat4 = igesio::Matrix2Xd::Constant(2, 3, 1.5);
    mat3 -= mat4;
    ValidateMatrixElements(mat3, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // 列数不一致エラー（動的サイズ）
    auto mat5 = igesio::Matrix2Xd(2, 2);
    auto mat6 = igesio::Matrix2Xd(2, 3);
    EXPECT_THROW(mat5 -= mat6, std::invalid_argument);
}

// スカラー乗算のテスト
TEST(MatrixOperatorTest, ScalarMultiplication) {
    // 固定サイズ行列のスカラー乗算
    auto mat1 = igesio::Matrix23d::Constant(2.0);
    auto result = mat1 * 3.0;
    ValidateMatrixElements(result, {{6.0, 6.0, 6.0}, {6.0, 6.0, 6.0}});

    // 動的サイズ行列のスカラー乗算
    auto mat2 = igesio::Matrix2Xd::Constant(2, 3, 1.5);
    auto result2 = mat2 * 2.0;
    ValidateMatrixElements(result2, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // ゼロ倍
    auto result3 = mat1 * 0.0;
    ValidateMatrixElements(result3, {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}});

    // 負数倍
    auto result4 = mat1 * -2.0;
    ValidateMatrixElements(result4, {{-4.0, -4.0, -4.0}, {-4.0, -4.0, -4.0}});
}

// スカラー乗算代入のテスト
TEST(MatrixOperatorTest, ScalarMultiplicationAssignment) {
    // 固定サイズ行列のスカラー乗算代入
    auto mat1 = igesio::Matrix23d::Constant(2.0);
    mat1 *= 3.0;
    ValidateMatrixElements(mat1, {{6.0, 6.0, 6.0}, {6.0, 6.0, 6.0}});

    // 動的サイズ行列のスカラー乗算代入
    auto mat2 = igesio::Matrix2Xd::Constant(2, 3, 1.5);
    mat2 *= 2.0;
    ValidateMatrixElements(mat2, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // ゼロ倍
    auto mat3 = igesio::Matrix23d::Constant(5.0);
    mat3 *= 0.0;
    ValidateMatrixElements(mat3, {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}});
}

// スカラー除算のテスト
TEST(MatrixOperatorTest, ScalarDivision) {
    // 固定サイズ行列のスカラー除算
    auto mat1 = igesio::Matrix23d::Constant(6.0);
    auto result = mat1 / 2.0;
    ValidateMatrixElements(result, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // 動的サイズ行列のスカラー除算
    auto mat2 = igesio::Matrix2Xd::Constant(2, 3, 9.0);
    auto result2 = mat2 / 3.0;
    ValidateMatrixElements(result2, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // ゼロ除算エラー
    EXPECT_THROW(mat1 / 0.0, std::invalid_argument);
}

// スカラー除算代入のテスト
TEST(MatrixOperatorTest, ScalarDivisionAssignment) {
    // 固定サイズ行列のスカラー除算代入
    auto mat1 = igesio::Matrix23d::Constant(6.0);
    mat1 /= 2.0;
    ValidateMatrixElements(mat1, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // 動的サイズ行列のスカラー除算代入
    auto mat2 = igesio::Matrix2Xd::Constant(2, 3, 9.0);
    mat2 /= 3.0;
    ValidateMatrixElements(mat2, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});

    // ゼロ除算エラー
    auto mat3 = igesio::Matrix23d::Constant(1.0);
    EXPECT_THROW(mat3 /= 0.0, std::invalid_argument);
}

// 非メンバ関数スカラー乗算のテスト
TEST(MatrixOperatorTest, NonMemberScalarMultiplication) {
    // 固定サイズ行列
    auto mat1 = igesio::Matrix23d::Constant(2.0);
    auto result = 3.0 * mat1;
    ValidateMatrixElements(result, {{6.0, 6.0, 6.0}, {6.0, 6.0, 6.0}});

    // 動的サイズ行列
    auto mat2 = igesio::Matrix2Xd::Constant(2, 3, 1.5);
    auto result2 = 2.0 * mat2;
    ValidateMatrixElements(result2, {{3.0, 3.0, 3.0}, {3.0, 3.0, 3.0}});
}

// 行列・ベクトル積のテスト
TEST(MatrixOperatorTest, MatrixVectorMultiplication) {
    // 2x2行列と2次元ベクトルの積
    igesio::Matrix2d mat2x2;
    mat2x2(0, 0) = 1.0;
    mat2x2(0, 1) = 2.0;
    mat2x2(1, 0) = 3.0;
    mat2x2(1, 1) = 4.0;

    igesio::Vector2d vec2;
    vec2(0) = 5.0;
    vec2(1) = 6.0;

    auto result = mat2x2 * vec2;
    ValidateColVectorElements(result, {17.0, 39.0});  // [1*5+2*6, 3*5+4*6]

    // 3x3行列と3次元ベクトルの積
    igesio::Matrix3d mat3x3 = {{1.0, 2.0, 3.0},
                               {4.0, 5.0, 6.0},
                               {7.0, 8.0, 9.0}};

    igesio::Vector3d vec3;
    vec3(0) = 1.0;
    vec3(1) = 2.0;
    vec3(2) = 3.0;

    auto result2 = mat3x3 * vec3;
    ValidateColVectorElements(result2, {14.0, 32.0, 50.0});  // [1+4+9, 4+10+18, 7+16+27]

    // 動的サイズ行列とベクトルの積
    igesio::Matrix2Xd matDyn = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};

    auto result3 = matDyn * vec3;
    ValidateColVectorElements(result3, {14.0, 32.0});

    // 次元不一致エラー（動的サイズ）
    igesio::Matrix2Xd matDyn2(2, 2);
    EXPECT_THROW(matDyn2 * vec3, std::invalid_argument);
}

// 行列・行列積のテスト
TEST(MatrixOperatorTest, MatrixMatrixMultiplication) {
    // 2x3行列と3x2行列の積
    igesio::Matrix23d mat2x3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    igesio::Matrix32d mat3x2 = {{7.0, 8.0}, {9.0, 10.0}, {11.0, 12.0}};

    auto result = mat2x3 * mat3x2;
    // [[1*7+2*9+3*11, ...], ...]
    ValidateMatrixElements(result, {{58.0, 64.0}, {139.0, 154.0}});

    // 動的サイズ行列の積
    igesio::Matrix2Xd matDyn2 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    igesio::Matrix3Xd matDyn3 = {{7.0, 8.0}, {9.0, 10.0}, {11.0, 12.0}};
    auto result2 = matDyn2 * matDyn3;
    // 上と同じ
    ValidateMatrixElements(result2, {{58.0, 64.0}, {139.0, 154.0}});

    // 次元不一致エラー
    igesio::Matrix2Xd matDyn4(2, 2);
    EXPECT_THROW(matDyn2 * matDyn4, std::invalid_argument);
}



/**
 * 型エイリアスを使用したテスト
 */

TEST(MatrixTypeAliasTest, TypeAliases) {
    // 固定サイズ行列の型エイリアス
    auto mat2x2 = igesio::Matrix2d::Constant(1.0);
    ValidateMatrixElements(mat2x2, {{1.0, 1.0}, {1.0, 1.0}});
    auto mat3x3 = igesio::Matrix3d::Constant(2.0);
    ValidateMatrixElements(mat3x3, {{2.0, 2.0, 2.0}, {2.0, 2.0, 2.0}, {2.0, 2.0, 2.0}});

    // 動的サイズ行列の型エイリアス
    auto mat2xDyn = igesio::Matrix2Xd(2, 3);
    mat2xDyn = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    ValidateMatrixElements(mat2xDyn, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}});
}

// detail内で定義されている非メンバ関数のテスト
// operator* (行列・ベクトル積)、（スカラー・行列積）
TEST(MatrixTypeAliasTest, NonMemberFunctions) {
    // 行列・ベクトル積
    igesio::Matrix2d mat2x2 = {{1.0, 2.0}, {3.0, 4.0}};
    igesio::Vector2d vec2;
    vec2(0) = 5.0;
    vec2(1) = 6.0;
    auto result = mat2x2 * vec2;
    ValidateColVectorElements(result, {17.0, 39.0});  // [1*5+2*6, 3*5+4*6]

    // スカラー・行列積
    auto mat3x3 = igesio::Matrix3d::Constant(2.0);
    auto result2 = 3.0 * mat3x3;
    ValidateMatrixElements(result2, {{6.0, 6.0, 6.0}, {6.0, 6.0, 6.0}, {6.0, 6.0, 6.0}});
}
