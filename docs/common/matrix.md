# `Matrix` Class

## Overview

　The IGESio library provides linear algebra functionality to handle matrices and vectors in the processing of IGES (Initial Graphics Exchange Specification) files. While IGESio recommends using Eigen, a high-performance linear algebra library, it also provides its own matrix and vector classes for cases where minimizing dependencies is desired or specific environmental constraints exist.

> Target File:
>
> - `igesio/common/matrix.h`

## Table of Contents

- [Overview](#overview)
- [Table of Contents](#table-of-contents)
- [Design Philosophy](#design-philosophy)
  - [Eigen Compatibility](#eigen-compatibility)
    - [API Compatibility](#api-compatibility)
  - [Limitations](#limitations)
    - [Performance Characteristics](#performance-characteristics)
- [IGESio Matrix Class Detailed Specifications](#igesio-matrix-class-detailed-specifications)
  - [Template Parameters](#template-parameters)
  - [Type Aliases](#type-aliases)
- [Member Functions](#member-functions)
  - [Constructors](#constructors)
  - [Operation Features](#operation-features)
    - [Element Access etc.](#element-access-etc)
    - [Arithmetic Operations etc.](#arithmetic-operations-etc)
    - [Other Calculations](#other-calculations)
    - [Output Features](#output-features)
- [Usage Examples](#usage-examples)
  - [Example of Processing Using the Matrix Class](#example-of-processing-using-the-matrix-class)
  - [Constructor and Initialization](#constructor-and-initialization)
    - [Creating Fixed-Size Matrices](#creating-fixed-size-matrices)
    - [Creating Dynamic-Size Matrices](#creating-dynamic-size-matrices)
  - [Static Member Functions](#static-member-functions)
    - [Creating Zero and Identity Matrices](#creating-zero-and-identity-matrices)
    - [Creating Constant Matrices](#creating-constant-matrices)
  - [Basic Operations](#basic-operations)
  - [Matrix-Vector and Matrix-Matrix Products](#matrix-vector-and-matrix-matrix-products)
- [Why This Class is Needed](#why-this-class-is-needed)
  - [Background and Purpose](#background-and-purpose)
  - [Role of Matrices in IGES Processing](#role-of-matrices-in-iges-processing)
  - [How to Switch Libraries](#how-to-switch-libraries)
    - [When Using Eigen (Recommended)](#when-using-eigen-recommended)
    - [When Using Custom Implementation](#when-using-custom-implementation)

## Design Philosophy

### Eigen Compatibility

　This library switches between the following two modes using the `IGESIO_ENABLE_EIGEN` macro:

- **Eigen Mode**: Uses Eigen library type aliases
- **Custom Implementation Mode**: Uses custom `Matrix` class template

For details, see [Why This Class is Needed](#why-this-class-is-needed).

#### API Compatibility

　IGESio's custom Matrix class is designed to maintain compatibility with Eigen's basic API:

```cpp
// The following code works the same way with both Eigen and IGESio custom implementation
igesio::Vector3d v1{1.0, 2.0, 3.0};
igesio::Vector3d v2{4.0, 5.0, 6.0};

double dot_result = v1.dot(v2);                // dot product
igesio::Vector3d cross_result = v1.cross(v2);  // cross product (3D vectors only)
double norm_value = v1.norm();                 // norm

// Matrix operations
igesio::Matrix3d mat = igesio::Matrix3d::Identity();
igesio::Vector3d result = mat * v1;  // matrix-vector product
```

### Limitations

　The custom implementation `Matrix` class has the following limitations:

| Feature | Eigen | IGESio Custom Implementation |
|---------|-------|------------------------------|
| Number of Rows | Arbitrary | 2 or 3 only |
| Number of Columns | Arbitrary | 1-3 or dynamic |
| Arithmetic Operations | ✅ | ✅ |
| [Other Simple Linear Algebra Operations](#other-calculations) | ✅ | ✅ |
| LU Decomposition | ✅ | ❌ |
| Eigenvalue Computation | ✅ | ❌ |
| Advanced Linear Algebra | ✅ | ❌ |

#### Performance Characteristics

- **Eigen**: Highly optimized with consideration for SIMD instructions and cache efficiency
- **IGESio Custom Implementation**: Simple implementation without lazy evaluation, resulting in performance inferior to Eigen

## IGESio Matrix Class Detailed Specifications

　This library defines the `igesio::detail::Matrix` class for handling matrices with 2 or 3 rows. This class has a (somewhat) similar interface to Eigen's matrix class and supports matrix and vector operations. **This class does not have API compatibility with `Eigen::Matrix`, so please do not use it directly**. Instead, use the [type aliases](#type-aliases) below.

- **Number of Rows**: 2 or 3 (specified by template parameter `N`)
- **Number of Columns**: Fixed (1-3) or dynamic (specified by template parameter `M`)
- **Internal Data**: Stored in column-major format
- **Supported Operations**
    - Element access
    - Matrix/vector addition and subtraction
    - Scalar multiplication and division
    - Matrix-matrix and matrix-vector products
    - Resizing (for dynamic column count; only column count can be changed with `conservativeResize`)
    - Several other basic linear algebra operations

### Template Parameters

- `N`: Number of rows (2 or 3)
- `M`: Number of columns (one of the following)
    - `igesio::Dynamic` (-1): Dynamic column count
    - 1-3: Fixed column count

```cpp
namespace igesio::detail {

template<int N, int M>
class Matrix

}  // namespace igesio::detail
```

### Type Aliases

　Type aliases are provided for commonly used types. Basically, do not use the `igesio::detail::Matrix` class directly, but **use the following type aliases**.

```cpp
namespace igesio {

using Matrix2d  = detail::Matrix<2, 2>;       // 2x2 matrix
using Matrix23d = detail::Matrix<2, 3>;       // 2x3 matrix
using Matrix32d = detail::Matrix<3, 2>;       // 3x2 matrix
using Matrix3d  = detail::Matrix<3, 3>;       // 3x3 matrix
using Matrix2Xd = detail::Matrix<2, Dynamic>; // 2 rows × dynamic columns matrix
using Matrix3Xd = detail::Matrix<3, Dynamic>; // 3 rows × dynamic columns matrix
using Vector2d  = detail::Matrix<2, 1>;       // 2D vector
using Vector3d  = detail::Matrix<3, 1>;       // 3D vector

}  // namespace igesio
```

## Member Functions

### Constructors

　The custom implementation `igesio::Matrix` class in this library supports the following 4 types of constructors. In the table below, `M` represents the matrix

| Constructor | Description |
|---|---|
| `Matrix()` | Default constructor.<br>For fixed column count, it initializes with the specified column count; for dynamic column count, a matrix with 0 columns is created. Element values are undefined. |
| `Matrix(rows, cols)` | Available for dynamic column count matrices.<br>`rows` must match the matrix row count `N` (2 for `Matrix2d`). |
| `Matrix2d m = {{1, 2}, {3, 4}}` | Matrix initialization using initializer list.<br>For dynamic size matrices, the column count is determined based on the number of elements in the initializer list. |
| `Vector2d v = {1.0, 2.0}` | Vector initialization using initializer list.<br>The number of elements in the initializer list must match the vector's row count `N`. |

　Also, the following static member functions allow easy creation of zero matrices and identity matrices.

| Member Function | Description |
|---|---|
| `Matrix Zero()` | Creates a zero matrix (fixed size version).<br>Returns a matrix with all elements initialized to 0.0. |
| `Matrix Zero(int rows, int cols)` | Creates a zero matrix (dynamic size version).<br>`rows` is the number of rows (must match `N`), `cols` specifies the number of columns. |
| `Matrix Identity()` | Creates an identity matrix (fixed size version).<br>Returns a square matrix with diagonal elements as 1.0 and others as 0.0.<br>**Available only for square matrices**. |
| `Matrix Identity(int rows, int cols)` | Creates an identity matrix (dynamic size version).<br>`rows` is the number of rows (must match `N`), `cols` specifies the number of columns.<br>**Available only for square matrices**. |
| `Matrix Constant(double value)` | Creates a constant matrix (fixed size version).<br>Returns a matrix with all elements initialized to `value`. |
| `Matrix Constant(int rows, int cols, double value)` | Creates a constant matrix (dynamic size version).<br>`rows` is the number of rows (must match `N`), `cols` is the number of columns, `value` specifies the initial value. |

### Operation Features

　This section explains the operation features supported by the `igesio::Matrix` class. The table below distinguishes between matrix and vector operations.

**Notation**
- Matrix: $M$ (code example: `m`)
- Vector: $\bm{v}$ (code example: `v`)
- Scalar: $s$ (code example: `s`)

**Scope**
- Operations explicitly marked as "vector": Available only for vectors
- Operations marked as "matrix": Available for both matrices and vectors

#### Element Access etc.

| Code Example | Return Value | Description |
|:--:|:--:|---|
| `m.rows()` | `size_t` | Returns the number of rows. |
| `m.cols()` | `size_t` | Returns the number of columns. For dynamic column count, returns the current column count. |
| `m.size()` | `size_t` | Returns the number of elements in the matrix (rows × columns). |
| `m(i, j)` | `double&`<br>`const double&` | Accesses matrix elements. `i` is the row index, `j` is the column index. |
| `m(i)`<br>`m[i]` | `double&`<br>`const double&` | Accesses vector elements. `i` is the index.<br>**Available only for vectors (1-column matrices)**. |
| `m.col(j)` | `Vector2d`<br>or<br>`Vector3d` | Returns a vector containing the elements of column `j`. |

　Also, the matrix size can be changed using the following member function. Since the custom implementation supports only fixed-row matrices, always specify `igesio::NoChange` for the first argument and specify the column count in the second argument.

- `m.conservativeResize(igesio::NoChange, new_cols)`

#### Arithmetic Operations etc.

　The following is a list of arithmetic operations supported by the custom implementation `igesio::Matrix` class in this library. For addition and subtraction, fixed column count and dynamic column count cannot be mixed.

| Operation | Formula | Description/Code Example |
|:--:|:--:|---|
| Addition | $M_1 + M_2$ | Available when $M_1$ and $M_2$ have the same size<br>`m1 + m2`<br>`m1 += m2` |
| Subtraction | $M_1 - M_2$ | Available when $M_1$ and $M_2$ have the same size<br>`m1 - m2`<br>`m1 -= m2` |
| Scalar Multiplication | $s \cdot M$ <br> $M \cdot s$ | `s * m`<br>`m * s`<br>`m *= s` |
| Scalar Division | $M / s$ | `m / s`<br>`m /= s` |
| Matrix-Vector Product | $M \bm{v}$ | Available when matrix column count matches vector row count<br>`m * v` |
| Matrix-Matrix Product | $M_1 M_2$ | Available when $M_1$ column count matches $M_2$ row count<br>[Result size is $M_1$ rows × $M_2$ columns](#matrix-vector-and-matrix-matrix-products)<br>`m1 * m2` |
| Dot Product | $\bm{v}_1 \cdot \bm{v}_2$ | Available when vectors have the same size<br>`v1.dot(v2)` |
| Cross Product | $\bm{v}_1 \times \bm{v}_2$ | Available only for 3D vectors<br>`v1.cross(v2)` |
| Hadamard Product | $M_1 \circ M_2$ | Available when $M_1$ and $M_2$ have the same size<br>Element-wise product of matrices/vectors<br>`m1.cwiseProduct(m2)` |
| Hadamard Quotient | $M_1 \oslash M_2$ | Available when $M_1$ and $M_2$ have the same size<br>Element-wise division of matrices/vectors<br>`m1.cwiseQuotient(m2)` |

#### Other Calculations

　The following table shows other calculation features supported by the `igesio::Matrix` class. Here, $m_{i,j}$ represents the element in row $i$ and column $j$ of matrix $M$.

| Return Value | Formula | Description/Code Example |
|:--:|:--:|---|
| `Matrix` | $1 / m_{i,j}$ | Calculates element-wise reciprocal.<br>`m.cwiseInverse()` |
| `Matrix` | $\sqrt{m_{i,j}}$ | Calculates element-wise square root.<br>`m.cwiseSqrt()` |
| `Matrix` | $\|m_{i,j}\|$ | Calculates element-wise absolute value.<br>`m.cwiseAbs()` |
| `double` | $\sum m_{i,j}^2$ | Calculates the sum of squares of matrix elements.<br>`m.squaredNorm()` |
| `double` | $\sqrt{\sum m_{i,j}^2}$ | Calculates the norm (Euclidean distance) of matrix elements.<br>`m.norm()` |
| `double` | $\sum m_{i,j}$ | Calculates the sum of matrix elements.<br>`m.sum()` |
| `double` | $\prod m_{i,j}$ | Calculates the product of matrix elements.<br>`m.prod()` |

#### Output Features

```cpp
// Matrix output
igesio::Matrix2d mat = {{1.0, 2.0}, {3.0, 4.0}};
std::cout << mat << std::endl;
// Output: ((1, 2), (3, 4))

// Dynamic size matrix output
igesio::Matrix2Xd dynMat(2, 3);
dynMat = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
std::cout << dynMat << std::endl;
// Output: ((1, 2, 3), (4, 5, 6))
```

## Usage Examples

### Example of Processing Using the Matrix Class

```cpp
#include <iostream>
#include "igesio/common/matrix.h"

int main() {
        using namespace igesio;

        // Creating a 2×2 rotation matrix
        double angle = M_PI / 4;  // 45 degrees
        Matrix2d rotation{{std::cos(angle), -std::sin(angle)},
                                            {std::sin(angle),  std::cos(angle)}};

        // Creating a 2D point
        Vector2d point{1.0, 0.0};

        // Applying rotation transformation
        Vector2d rotated_point = rotation * point;
        std::cout << "Rotated point: " << rotated_point << std::endl;

        // Cross product calculation for 3D vectors
        Vector3d v1{1.0, 0.0, 0.0};
        Vector3d v2{0.0, 1.0, 0.0};
        Vector3d cross_product = v1.cross(v2);
        std::cout << "Cross product: " << cross_product << std::endl;

        // Using dynamic size matrices
        Matrix3Xd points(3, 4);  // Storing 4 3D points
        points << 1, 2, 3, 4,
                            0, 1, 2, 3,
                            0, 0, 1, 2;

        // Applying transformation to each column (point)
        Matrix3d transform = Matrix3d::Identity() * 2.0;  // 2x scaling
        Matrix3Xd transformed_points = transform * points;

        return 0;
}
```

### Constructor and Initialization

#### Creating Fixed-Size Matrices

```cpp
// Creating a 2x2 matrix
// [1 2]
// [3 4]
igesio::Matrix2d mat2x2;
mat2x2(0, 0) = 1.0; mat2x2(0, 1) = 2.0;
mat2x2(1, 0) = 3.0; mat2x2(1, 1) = 4.0;

// Initialization using initializer list is also possible
// [3 4]
// [5 6]
mat2x2 = {{3, 4}, {5, 6}};

// Creating a 3D vector
// [1 2 3]^T
igesio::Vector3d vec3d;
vec3d(0) = 1.0;  // Vectors allow single index access
vec3d(1) = 2.0;
vec3d(2) = 3.0;

// Initialization using initializer list is also possible
// [1 2 3]^T
vec3d = {1.0, 2.0, 3.0};
```

#### Creating Dynamic-Size Matrices

　For dynamic size matrices, you can change the size using the `conservativeResize` member function. Always specify `igesio::NoChange` for the first argument and specify the column count in the second argument. The row count is fixed (2 or 3) and cannot be changed.

```cpp
// Creating a 3 rows × dynamic columns matrix
igesio::Matrix3Xd mat3xN(3, 5);  // 3 rows, 5 columns
mat3xN = {{ 1.0,  2.0,  3.0,  4.0,  5.0},
                    { 6.0,  7.0,  8.0,  9.0,  10.0},
                    {11.0, 12.0, 13.0, 14.0, 15.0}};

// Resize to 3 rows, 8 columns
// [ 1  2  3  4  5  0  0  0]
// [ 6  7  8  9 10  0  0  0]
// [11 12 13 14 15  0  0  0]
mat3xN.conservativeResize(igesio::NoChange, 8);

// Resize to 3 rows, 4 columns
// [ 1  2  3  4]
// [ 6  7  8  9]
// [11 12 13 14]
mat3xN.conservativeResize(igesio::NoChange, 4);
```

### Static Member Functions

#### Creating Zero and Identity Matrices

```cpp
// Fixed size zero matrix
// [0 0]
// [0 0]
auto zero_mat = igesio::Matrix2d::Zero();

// Dynamic size zero matrix
// [0 0 0 0]
// [0 0 0 0]
// [0 0 0 0]
auto zero_dyn = igesio::Matrix3Xd::Zero(3, 4);

// Identity matrix
// [1 0 0]
// [0 1 0]
// [0 0 1]
auto identity = igesio::Matrix3d::Identity();
```

#### Creating Constant Matrices

```cpp
// 3x3 matrix with all elements as 5.0
// [5 5 5]
// [5 5 5]
// [5 5 5]
auto const_mat = igesio::Matrix3d::Constant(5.0);

// Dynamic size constant matrix (2x4 matrix initialized with 3.14)
// [3.14 3.14 3.14 3.14]
// [3.14 3.14 3.14 3.14]
auto const_dyn = igesio::Matrix2Xd::Constant(2, 4, 3.14);
```

### Basic Operations

```cpp
igesio::Matrix2d a = igesio::Matrix2d::Identity();
igesio::Matrix2d b = igesio::Matrix2d::Constant(2.0);

// Matrix addition and subtraction
auto sum = a + b;
auto diff = a - b;
a += b;  // Assignment operations are also possible

// Scalar multiplication and division
auto scaled = a * 3.0;
auto divided = a / 2.0;
scaled *= 0.5;  // Assignment operations are also possible
```

### Matrix-Vector and Matrix-Matrix Products

```cpp
igesio::Matrix3d transform = igesio::Matrix3d::Identity();
igesio::Vector3d point = {1.0, 2.0, 3.0};

// Matrix-vector product
// [1 0 0]   [1]   [1]
// [0 1 0] * [2] = [2]
// [0 0 1]   [3]   [3]
igesio::Vector3d transformed = transform * point;
```

　Matrix-matrix products are also supported. In the following example, we calculate the product of a 2×3 matrix and a 3×2 matrix, resulting in a 2×2 matrix. Note that the result size of matrix multiplication is left matrix rows × right matrix columns, and **if the right matrix has dynamic column count, the result will also have dynamic size**.

$$\begin{bmatrix} 1 & 2 & 3 \\ 4 & 5 & 6 \end{bmatrix} \begin{bmatrix} 7 & 8 \\ 9 & 10 \\ 11 & 12 \end{bmatrix} = \begin{bmatrix} 58 & 64 \\ 139 & 154 \end{bmatrix}$$

```cpp
// Matrix-matrix product (both fixed size)
// => Result is a fixed size 2x2 matrix
igesio::Matrix23d mat23 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
igesio::Matrix32d mat32 = {{7.0, 8.0}, {9.0, 10.0}, {11.0, 12.0}};
igesio::Matrix2d result1 = mat23 * mat32;

// Matrix-matrix product (left side is dynamic size)
// => Result is a fixed size 2x2 matrix
igesio::Matrix2Xd mat2xN(2, 3);  // 2 rows, 3 columns dynamic size matrix
mat2xN = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
igesio::Matrix2d result2 = mat2xN * mat32;

// Matrix-matrix product (right side is dynamic size)
// => Result is a dynamic size 2x2 matrix
igesio::Matrix3Xd mat3xN(3, 2);  // 3 rows, 2 columns dynamic size matrix
mat3xN = {{7.0, 8.0}, {9.0, 10.0}, {11.0, 12.0}};
igesio::Matrix2Xd result3 = mat23 * mat3xN;
```

<!-- TODO: When entity coordinate transformation is implemented, add its explanation here -->

## Why This Class is Needed

### Background and Purpose

　The IGESio library provides dual implementation for the following reasons:

1. **Dependency Selectivity**: While Eigen is an excellent library, some projects may want to reduce dependencies
2. **Embedded Environment Usage**: In resource-constrained environments, lightweight implementations may be necessary
3. **Learning and Debugging Purposes**: Custom implementation makes it easier to understand internal workings
4. **License Requirements**: Some projects may need to meet specific license conditions

### Role of Matrices in IGES Processing

　In IGES format, matrix operations are frequently used for 3D geometric data transformations (translation, rotation, scaling) and coordinate system transformations. IGESio mainly uses matrices for the following purposes:

- Representation of 2D/3D coordinate values
- Coordinate transformation of 3D point clouds
- Orientation changes using rotation matrices
- Scaling transformations
- Projection transformations

### How to Switch Libraries

#### When Using Eigen (Recommended)

```cpp
// To enable Eigen in CMakeLists
target_compile_definitions(your_target PRIVATE IGESIO_ENABLE_EIGEN)

// Or define the macro at compile time
g++ -DIGESIO_ENABLE_EIGEN your_code.cpp
```

In this case, the following Eigen classes are used:

```cpp
#include "igesio/common/matrix.h"

// These are actually aliases to Eigen classes
igesio::Matrix2d    // Eigen::Matrix2d
igesio::Vector3d    // Eigen::Vector3d
igesio::Matrix3Xd   // Eigen::Matrix3Xd
```

#### When Using Custom Implementation

```cpp
// When IGESIO_ENABLE_EIGEN is not defined, custom implementation is automatically used
#include "igesio/common/matrix.h"

// Custom Matrix class is used
igesio::Matrix2d    // igesio::detail::Matrix<2, 2>
igesio::Vector3d    // igesio::detail::Matrix<3, 1>
```
