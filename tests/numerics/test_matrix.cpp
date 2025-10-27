/**
 * @file numerics/test_matrix.cpp
 * @brief Matrixクラスのテスト
 * @author Yayoi Habami
 * @date 2025-06-07
 * @copyright 2025 Yayoi Habami
 * @note このファイルでは、double型の行列に対してテストを行います。
 */
#include <gtest/gtest.h>

#include <initializer_list>
#include <string>

#include "igesio/numerics/matrix.h"


namespace {

using InitMatrix = std::initializer_list<std::initializer_list<double>>;
using InitVector = std::initializer_list<double>;

template<int N, int M>
using Matrixd = igesio::Matrix<double, N, M>;

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
        const Matrixd<N, M>& mat, const InitMatrix& ref) {
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
        const Matrixd<N, M>& vec, const InitVector& ref) {
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

// ベクトル要素アクセス operator[] のテスト
TEST(MatrixMethodsTest, VectorBracketOperator) {
    // 非const版のテスト
    igesio::Vector3d vec3x1;
    vec3x1[0] = 1.0;
    vec3x1[1] = 2.0;
    vec3x1[2] = 3.0;

    ValidateColVectorElements(vec3x1, {1.0, 2.0, 3.0});

    // const版のテスト
    const igesio::Vector3d const_vec3x1 = {4.0, 5.0, 6.0};
    EXPECT_DOUBLE_EQ(const_vec3x1[0], 4.0);
    EXPECT_DOUBLE_EQ(const_vec3x1[1], 5.0);
    EXPECT_DOUBLE_EQ(const_vec3x1[2], 6.0);

    // operator() との一貫性テスト
    igesio::Vector2d vec2;
    vec2[0] = 1.5;
    vec2[1] = 2.5;
    EXPECT_DOUBLE_EQ(vec2[0], vec2(0));
    EXPECT_DOUBLE_EQ(vec2[1], vec2(1));

    // 値の変更テスト
    vec2[0] = 3.5;
    EXPECT_DOUBLE_EQ(vec2[0], 3.5);
    EXPECT_DOUBLE_EQ(vec2(0), 3.5);
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

// col()メソッドのテスト
TEST(MatrixMethodsTest, ColMethod) {
    // 固定サイズ行列のテスト
    igesio::Matrix23d mat2x3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};

    // 0列目の取得
    auto col0 = mat2x3.col(0);
    ValidateColVectorElements(col0, {1.0, 4.0});

    // 1列目の取得
    auto col1 = mat2x3.col(1);
    ValidateColVectorElements(col1, {2.0, 5.0});

    // 2列目の取得
    auto col2 = mat2x3.col(2);
    ValidateColVectorElements(col2, {3.0, 6.0});

    // 動的サイズ行列のテスト
    igesio::Matrix3Xd mat3xDyn = {{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}};

    // 0列目の取得
    auto dynCol0 = mat3xDyn.col(0);
    ValidateColVectorElements(dynCol0, {1.0, 3.0, 5.0});

    // 1列目の取得
    auto dynCol1 = mat3xDyn.col(1);
    ValidateColVectorElements(dynCol1, {2.0, 4.0, 6.0});

    // 範囲外アクセスのエラーテスト
    EXPECT_THROW(mat2x3.col(3), std::out_of_range);
    EXPECT_THROW(mat3xDyn.col(2), std::out_of_range);

    // 単一列行列（ベクトル）のテスト
    igesio::Vector3d vec3 = {1.0, 2.0, 3.0};
    auto vecCol0 = vec3.col(0);
    ValidateColVectorElements(vecCol0, {1.0, 2.0, 3.0});

    // ベクトルで範囲外アクセス
    EXPECT_THROW(vec3.col(1), std::out_of_range);
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
 * ベクトル専用演算のテスト
 */

// 内積のテスト (ベクトル専用)
TEST(MatrixVectorOperationsTest, DotProduct) {
    // 2次元ベクトルの内積
    igesio::Vector2d vec1 = {1.0, 2.0};
    igesio::Vector2d vec2 = {3.0, 4.0};
    double result = vec1.dot(vec2);
    EXPECT_DOUBLE_EQ(result, 11.0);  // 1*3 + 2*4 = 11

    // 3次元ベクトルの内積
    igesio::Vector3d vec3 = {1.0, 2.0, 3.0};
    igesio::Vector3d vec4 = {4.0, 5.0, 6.0};
    double result2 = vec3.dot(vec4);
    EXPECT_DOUBLE_EQ(result2, 32.0);  // 1*4 + 2*5 + 3*6 = 32

    // ゼロベクトルとの内積
    igesio::Vector3d zero_vec = igesio::Vector3d::Zero();
    double result3 = vec3.dot(zero_vec);
    EXPECT_DOUBLE_EQ(result3, 0.0);

    // 単位ベクトルとの内積
    igesio::Vector2d unit_vec = {1.0, 0.0};
    igesio::Vector2d test_vec = {5.0, 3.0};
    double result4 = test_vec.dot(unit_vec);
    EXPECT_DOUBLE_EQ(result4, 5.0);

    // 自分自身との内積（ノルムの二乗）
    igesio::Vector2d self_vec = {3.0, 4.0};
    double result5 = self_vec.dot(self_vec);
    EXPECT_DOUBLE_EQ(result5, 25.0);  // 3*3 + 4*4 = 25
}

// 外積のテスト (3次元ベクトル専用)
TEST(MatrixVectorOperationsTest, CrossProduct) {
    // 基本的な外積テスト
    igesio::Vector3d vec1 = {1.0, 0.0, 0.0};
    igesio::Vector3d vec2 = {0.0, 1.0, 0.0};
    auto result = vec1.cross(vec2);
    ValidateColVectorElements(result, {0.0, 0.0, 1.0});  // i × j = k

    // 逆順の外積テスト（反交換律）
    auto result2 = vec2.cross(vec1);
    ValidateColVectorElements(result2, {0.0, 0.0, -1.0});  // j × i = -k

    // 一般的なベクトルの外積
    igesio::Vector3d vec3 = {1.0, 2.0, 3.0};
    igesio::Vector3d vec4 = {4.0, 5.0, 6.0};
    auto result3 = vec3.cross(vec4);
    // (2*6-3*5, 3*4-1*6, 1*5-2*4) = (-3, 6, -3)
    ValidateColVectorElements(result3, {-3.0, 6.0, -3.0});

    // 平行ベクトルの外積（ゼロベクトル）
    igesio::Vector3d vec5 = {2.0, 4.0, 6.0};
    igesio::Vector3d vec6 = {1.0, 2.0, 3.0};  // vec5の半分
    auto result4 = vec5.cross(vec6);
    ValidateColVectorElements(result4, {0.0, 0.0, 0.0});

    // 自分自身との外積（ゼロベクトル）
    auto result5 = vec3.cross(vec3);
    ValidateColVectorElements(result5, {0.0, 0.0, 0.0});

    // ゼロベクトルとの外積
    igesio::Vector3d zero_vec = igesio::Vector3d::Zero();
    auto result6 = vec1.cross(zero_vec);
    ValidateColVectorElements(result6, {0.0, 0.0, 0.0});

    // 単位ベクトル間の外積テスト
    igesio::Vector3d i_unit = {1.0, 0.0, 0.0};
    igesio::Vector3d j_unit = {0.0, 1.0, 0.0};
    igesio::Vector3d k_unit = {0.0, 0.0, 1.0};

    auto ij_cross = i_unit.cross(j_unit);
    ValidateColVectorElements(ij_cross, {0.0, 0.0, 1.0});

    auto jk_cross = j_unit.cross(k_unit);
    ValidateColVectorElements(jk_cross, {1.0, 0.0, 0.0});

    auto ki_cross = k_unit.cross(i_unit);
    ValidateColVectorElements(ki_cross, {0.0, 1.0, 0.0});
}



/**
 * 要素ごとの演算のテスト
 */

// 要素ごとの積のテスト (アダマール積)
TEST(MatrixElementwiseOperationsTest, CwiseProduct) {
    // 固定サイズ行列の要素ごとの積
    igesio::Matrix23d mat1 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    igesio::Matrix23d mat2 = {{2.0, 3.0, 4.0}, {5.0, 6.0, 7.0}};
    auto result = mat1.cwiseProduct(mat2);
    ValidateMatrixElements(result, {{2.0, 6.0, 12.0}, {20.0, 30.0, 42.0}});

    // 動的サイズ行列の要素ごとの積
    igesio::Matrix2Xd mat3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    igesio::Matrix2Xd mat4 = {{2.0, 3.0, 4.0}, {5.0, 6.0, 7.0}};
    auto result2 = mat3.cwiseProduct(mat4);
    ValidateMatrixElements(result2, {{2.0, 6.0, 12.0}, {20.0, 30.0, 42.0}});

    // ベクトルの要素ごとの積
    igesio::Vector3d vec1 = {1.0, 2.0, 3.0};
    igesio::Vector3d vec2 = {4.0, 5.0, 6.0};
    auto result3 = vec1.cwiseProduct(vec2);
    ValidateColVectorElements(result3, {4.0, 10.0, 18.0});

    // ゼロ行列との要素ごとの積
    auto zero_mat = igesio::Matrix23d::Zero();
    auto result4 = mat1.cwiseProduct(zero_mat);
    ValidateMatrixElements(result4, {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}});

    // 単位行列との要素ごとの積
    auto identity_mat = igesio::Matrix3d::Identity();
    igesio::Matrix3d mat5 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}, {7.0, 8.0, 9.0}};
    auto result5 = mat5.cwiseProduct(identity_mat);
    ValidateMatrixElements(result5, {{1.0, 0.0, 0.0}, {0.0, 5.0, 0.0}, {0.0, 0.0, 9.0}});

    // 列数不一致エラー（動的サイズ）
    igesio::Matrix2Xd mat6(2, 2);
    igesio::Matrix2Xd mat7(2, 3);
    EXPECT_THROW(mat6.cwiseProduct(mat7), std::invalid_argument);
}

// 要素ごとの除算のテスト
TEST(MatrixElementwiseOperationsTest, CwiseQuotient) {
    // 固定サイズ行列の要素ごとの除算
    igesio::Matrix23d mat1 = {{6.0, 8.0, 12.0}, {20.0, 30.0, 42.0}};
    igesio::Matrix23d mat2 = {{2.0, 4.0, 3.0}, {5.0, 6.0, 7.0}};
    auto result = mat1.cwiseQuotient(mat2);
    ValidateMatrixElements(result, {{3.0, 2.0, 4.0}, {4.0, 5.0, 6.0}});

    // 動的サイズ行列の要素ごとの除算
    igesio::Matrix2Xd mat3 = {{8.0, 10.0, 15.0}, {24.0, 35.0, 48.0}};
    igesio::Matrix2Xd mat4 = {{2.0, 5.0, 3.0}, {6.0, 7.0, 8.0}};
    auto result2 = mat3.cwiseQuotient(mat4);
    ValidateMatrixElements(result2, {{4.0, 2.0, 5.0}, {4.0, 5.0, 6.0}});

    // ベクトルの要素ごとの除算
    igesio::Vector3d vec1 = {12.0, 15.0, 18.0};
    igesio::Vector3d vec2 = {3.0, 5.0, 6.0};
    auto result3 = vec1.cwiseQuotient(vec2);
    ValidateColVectorElements(result3, {4.0, 3.0, 3.0});

    // 1で除算（元の値と同じ）
    auto ones_mat = igesio::Matrix23d::Constant(1.0);
    auto result4 = mat1.cwiseQuotient(ones_mat);
    ValidateMatrixElements(result4, {{6.0, 8.0, 12.0}, {20.0, 30.0, 42.0}});

    // 列数不一致エラー（動的サイズ）
    igesio::Matrix2Xd mat5(2, 2);
    igesio::Matrix2Xd mat6(2, 3);
    EXPECT_THROW(mat5.cwiseQuotient(mat6), std::invalid_argument);
}

// 要素ごとの逆数のテスト
TEST(MatrixElementwiseOperationsTest, CwiseInverse) {
    // 固定サイズ行列の要素ごとの逆数
    igesio::Matrix23d mat1 = {{1.0, 2.0, 4.0}, {0.5, 0.25, 0.125}};
    auto result = mat1.cwiseInverse();
    ValidateMatrixElements(result, {{1.0, 0.5, 0.25}, {2.0, 4.0, 8.0}});

    // 動的サイズ行列の要素ごとの逆数
    igesio::Matrix2Xd mat2 = {{2.0, 5.0, 10.0}, {0.1, 0.2, 0.5}};
    auto result2 = mat2.cwiseInverse();
    ValidateMatrixElements(result2, {{0.5, 0.2, 0.1}, {10.0, 5.0, 2.0}});

    // ベクトルの要素ごとの逆数
    igesio::Vector3d vec1 = {1.0, 2.0, 4.0};
    auto result3 = vec1.cwiseInverse();
    ValidateColVectorElements(result3, {1.0, 0.5, 0.25});

    // 負数の逆数
    igesio::Matrix23d mat3 = {{-1.0, -2.0, -4.0}, {-0.5, -0.25, -0.125}};
    auto result4 = mat3.cwiseInverse();
    ValidateMatrixElements(result4, {{-1.0, -0.5, -0.25}, {-2.0, -4.0, -8.0}});

    // 大きな値の逆数（小さな値になる）
    igesio::Vector2d vec2 = {100.0, 1000.0};
    auto result5 = vec2.cwiseInverse();
    ValidateColVectorElements(result5, {0.01, 0.001});
}

// 要素ごとの平方根のテスト
TEST(MatrixElementwiseOperationsTest, CwiseSqrt) {
    // 固定サイズ行列の要素ごとの平方根
    igesio::Matrix23d mat1 = {{4.0, 9.0, 16.0}, {1.0, 25.0, 36.0}};
    auto result = mat1.cwiseSqrt();
    ValidateMatrixElements(result, {{2.0, 3.0, 4.0}, {1.0, 5.0, 6.0}});

    // 動的サイズ行列の要素ごとの平方根
    igesio::Matrix2Xd mat2 = {{1.0, 4.0, 9.0}, {16.0, 25.0, 49.0}};
    auto result2 = mat2.cwiseSqrt();
    ValidateMatrixElements(result2, {{1.0, 2.0, 3.0}, {4.0, 5.0, 7.0}});

    // ベクトルの要素ごとの平方根
    igesio::Vector3d vec1 = {1.0, 4.0, 9.0};
    auto result3 = vec1.cwiseSqrt();
    ValidateColVectorElements(result3, {1.0, 2.0, 3.0});

    // ゼロの平方根
    auto zero_mat = igesio::Matrix23d::Zero();
    auto result4 = zero_mat.cwiseSqrt();
    ValidateMatrixElements(result4, {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}});

    // 小数の平方根
    igesio::Vector2d vec2 = {0.25, 0.16};
    auto result5 = vec2.cwiseSqrt();
    ValidateColVectorElements(result5, {0.5, 0.4});

    // 1の平方根
    auto ones_mat = igesio::Matrix23d::Constant(1.0);
    auto result6 = ones_mat.cwiseSqrt();
    ValidateMatrixElements(result6, {{1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}});
}

// 要素ごとの絶対値のテスト
TEST(MatrixElementwiseOperationsTest, CwiseAbs) {
    // 固定サイズ行列の要素ごとの絶対値
    igesio::Matrix23d mat1 = {{-1.0, 2.0, -3.0}, {4.0, -5.0, 6.0}};
    auto result = mat1.cwiseAbs();
    ValidateMatrixElements(result, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}});

    // 動的サイズ行列の要素ごとの絶対値
    igesio::Matrix2Xd mat2 = {{-2.5, 3.5, -4.5}, {-1.5, 2.5, -3.5}};
    auto result2 = mat2.cwiseAbs();
    ValidateMatrixElements(result2, {{2.5, 3.5, 4.5}, {1.5, 2.5, 3.5}});

    // ベクトルの要素ごとの絶対値
    igesio::Vector3d vec1 = {-1.0, -2.0, -3.0};
    auto result3 = vec1.cwiseAbs();
    ValidateColVectorElements(result3, {1.0, 2.0, 3.0});

    // 正数のみの行列（変化なし）
    igesio::Matrix23d mat3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    auto result4 = mat3.cwiseAbs();
    ValidateMatrixElements(result4, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}});

    // ゼロの絶対値
    auto zero_mat = igesio::Matrix23d::Zero();
    auto result5 = zero_mat.cwiseAbs();
    ValidateMatrixElements(result5, {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}});

    // 混合（正数、負数、ゼロ）
    igesio::Vector3d vec2 = {-5.0, 0.0, 3.0};
    auto result6 = vec2.cwiseAbs();
    ValidateColVectorElements(result6, {5.0, 0.0, 3.0});

    // 小数の絶対値
    igesio::Matrix2d mat4 = {{-0.5, 0.7}, {-1.2, 2.3}};
    auto result7 = mat4.cwiseAbs();
    ValidateMatrixElements(result7, {{0.5, 0.7}, {1.2, 2.3}});
}



/**
 * Reduction operationsのテスト
 */

// 要素の二乗和のテスト
TEST(MatrixReductionOperationsTest, SquaredNorm) {
    // 固定サイズ行列の二乗和
    igesio::Matrix23d mat2x3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    double result = mat2x3.squaredNorm();
    EXPECT_DOUBLE_EQ(result, 91.0);  // 1^2 + 2^2 + 3^2 + 4^2 + 5^2 + 6^2 = 1 + 4 + 9 + 16 + 25 + 36 = 91

    // ベクトルの二乗和
    igesio::Vector3d vec3 = {3.0, 4.0, 5.0};
    double result2 = vec3.squaredNorm();
    EXPECT_DOUBLE_EQ(result2, 50.0);  // 3^2 + 4^2 + 5^2 = 9 + 16 + 25 = 50

    // ゼロ行列の二乗和
    auto zero_mat = igesio::Matrix23d::Zero();
    double result3 = zero_mat.squaredNorm();
    EXPECT_DOUBLE_EQ(result3, 0.0);

    // 単位行列の二乗和
    auto identity_mat = igesio::Matrix3d::Identity();
    double result4 = identity_mat.squaredNorm();
    EXPECT_DOUBLE_EQ(result4, 3.0);  // 1^2 + 0^2 + ... + 1^2 + 0^2 + ... + 1^2 = 3

    // 動的サイズ行列の二乗和
    igesio::Matrix2Xd mat2xDyn = {{1.0, 2.0}, {3.0, 4.0}};
    double result5 = mat2xDyn.squaredNorm();
    EXPECT_DOUBLE_EQ(result5, 30.0);  // 1^2 + 2^2 + 3^2 + 4^2 = 1 + 4 + 9 + 16 = 30

    // 負数を含む行列の二乗和
    igesio::Matrix2d mat_negative = {{-1.0, 2.0}, {-3.0, 4.0}};
    double result6 = mat_negative.squaredNorm();
    EXPECT_DOUBLE_EQ(result6, 30.0);  // (-1)^2 + 2^2 + (-3)^2 + 4^2 = 1 + 4 + 9 + 16 = 30
}

// ノルムのテスト
TEST(MatrixReductionOperationsTest, Norm) {
    // 固定サイズ行列のノルム
    igesio::Matrix2d mat2x2 = {{3.0, 4.0}, {0.0, 0.0}};
    double result = mat2x2.norm();
    EXPECT_DOUBLE_EQ(result, 5.0);  // sqrt(3^2 + 4^2) = sqrt(25) = 5

    // ベクトルのノルム（3-4-5直角三角形）
    igesio::Vector2d vec2 = {3.0, 4.0};
    double result2 = vec2.norm();
    EXPECT_DOUBLE_EQ(result2, 5.0);  // sqrt(3^2 + 4^2) = 5

    // 単位ベクトルのノルム
    igesio::Vector3d unit_vec = {1.0, 0.0, 0.0};
    double result3 = unit_vec.norm();
    EXPECT_DOUBLE_EQ(result3, 1.0);

    // ゼロベクトルのノルム
    auto zero_vec = igesio::Vector3d::Zero();
    double result4 = zero_vec.norm();
    EXPECT_DOUBLE_EQ(result4, 0.0);

    // 動的サイズ行列のノルム
    igesio::Matrix3Xd mat3xDyn = {{1.0, 2.0}, {2.0, 0.0}, {0.0, 0.0}};
    double result5 = mat3xDyn.norm();
    EXPECT_DOUBLE_EQ(result5, 3.0);  // sqrt(1^2 + 2^2 + 2^2) = sqrt(9) = 3

    // 負数を含むベクトルのノルム
    igesio::Vector2d vec_negative = {-3.0, -4.0};
    double result6 = vec_negative.norm();
    EXPECT_DOUBLE_EQ(result6, 5.0);  // sqrt((-3)^2 + (-4)^2) = 5

    // ノルムとsquaredNormの関係確認
    igesio::Vector3d vec_test = {1.0, 2.0, 3.0};
    double norm_val = vec_test.norm();
    double squared_norm_val = vec_test.squaredNorm();
    EXPECT_DOUBLE_EQ(norm_val * norm_val, squared_norm_val);
}

// 全要素の和のテスト
TEST(MatrixReductionOperationsTest, Sum) {
    // 固定サイズ行列の和
    igesio::Matrix23d mat2x3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    double result = mat2x3.sum();
    EXPECT_DOUBLE_EQ(result, 21.0);  // 1 + 2 + 3 + 4 + 5 + 6 = 21

    // ベクトルの和
    igesio::Vector3d vec3 = {1.0, 2.0, 3.0};
    double result2 = vec3.sum();
    EXPECT_DOUBLE_EQ(result2, 6.0);  // 1 + 2 + 3 = 6

    // ゼロ行列の和
    auto zero_mat = igesio::Matrix23d::Zero();
    double result3 = zero_mat.sum();
    EXPECT_DOUBLE_EQ(result3, 0.0);

    // 単位行列の和
    auto identity_mat = igesio::Matrix3d::Identity();
    double result4 = identity_mat.sum();
    EXPECT_DOUBLE_EQ(result4, 3.0);  // 1 + 0 + 0 + 0 + 1 + 0 + 0 + 0 + 1 = 3

    // 動的サイズ行列の和
    igesio::Matrix2Xd mat2xDyn = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    double result5 = mat2xDyn.sum();
    EXPECT_DOUBLE_EQ(result5, 21.0);

    // 負数を含む行列の和
    igesio::Matrix2d mat_mixed = {{-1.0, 2.0}, {3.0, -4.0}};
    double result6 = mat_mixed.sum();
    EXPECT_DOUBLE_EQ(result6, 0.0);  // -1 + 2 + 3 + (-4) = 0

    // 小数を含む行列の和
    igesio::Vector2d vec_decimal = {1.5, 2.5};
    double result7 = vec_decimal.sum();
    EXPECT_DOUBLE_EQ(result7, 4.0);  // 1.5 + 2.5 = 4.0

    // 定数行列の和
    auto const_mat = igesio::Matrix23d::Constant(2.5);
    double result8 = const_mat.sum();
    EXPECT_DOUBLE_EQ(result8, 15.0);  // 2.5 * 6 = 15.0
}

// 全要素の積のテスト
TEST(MatrixReductionOperationsTest, Prod) {
    // 固定サイズ行列の積
    igesio::Matrix23d mat2x3 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    double result = mat2x3.prod();
    EXPECT_DOUBLE_EQ(result, 720.0);  // 1 * 2 * 3 * 4 * 5 * 6 = 720

    // ベクトルの積
    igesio::Vector3d vec3 = {1.0, 2.0, 3.0};
    double result2 = vec3.prod();
    EXPECT_DOUBLE_EQ(result2, 6.0);  // 1 * 2 * 3 = 6

    // ゼロを含む行列の積（結果は0）
    igesio::Matrix2d mat_with_zero = {{1.0, 2.0}, {0.0, 3.0}};
    double result3 = mat_with_zero.prod();
    EXPECT_DOUBLE_EQ(result3, 0.0);  // 1 * 2 * 0 * 3 = 0

    // 単位行列の積（対角成分のみ1、他は0なので結果は0）
    auto identity_mat = igesio::Matrix3d::Identity();
    double result4 = identity_mat.prod();
    EXPECT_DOUBLE_EQ(result4, 0.0);  // 1 * 0 * 0 * 0 * 1 * 0 * 0 * 0 * 1 = 0

    // 全て1の行列の積
    auto ones_mat = igesio::Matrix23d::Constant(1.0);
    double result5 = ones_mat.prod();
    EXPECT_DOUBLE_EQ(result5, 1.0);  // 1 * 1 * 1 * 1 * 1 * 1 = 1

    // 動的サイズ行列の積
    igesio::Matrix2Xd mat2xDyn = {{2.0, 3.0}, {4.0, 5.0}};
    double result6 = mat2xDyn.prod();
    EXPECT_DOUBLE_EQ(result6, 120.0);  // 2 * 3 * 4 * 5 = 120

    // 負数を含む行列の積
    igesio::Vector2d vec_negative = {-2.0, 3.0};
    double result7 = vec_negative.prod();
    EXPECT_DOUBLE_EQ(result7, -6.0);  // -2 * 3 = -6

    // 偶数個の負数を含む行列の積（正の結果）
    igesio::Matrix2d mat_even_negative = {{-1.0, -2.0}, {3.0, 4.0}};
    double result8 = mat_even_negative.prod();
    EXPECT_DOUBLE_EQ(result8, 24.0);  // (-1) * (-2) * 3 * 4 = 24

    // 小数を含む行列の積
    igesio::Vector2d vec_decimal = {0.5, 2.0};
    double result9 = vec_decimal.prod();
    EXPECT_DOUBLE_EQ(result9, 1.0);  // 0.5 * 2.0 = 1.0

    // 空の動的行列（0列）の積は1（積の単位元）
    igesio::Matrix2Xd empty_mat(2, 0);
    double result11 = empty_mat.prod();
    EXPECT_DOUBLE_EQ(result11, 1.0);
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
