# `Matrix` Class

## Overview

　The IGESio library provides linear algebra functionality to handle matrices and vectors in the processing of IGES (Initial Graphics Exchange Specification) files. While IGESio recommends using Eigen, a high-performance linear algebra library, it also provides its own matrix and vector classes for cases where minimizing dependencies is desired or specific environmental constraints exist.

> Target File:
>
> - `igesio/numerics/matrix.h`

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
    - [Size Change/Conversion](#size-changeconversion)
    - [Arithmetic Operations etc.](#arithmetic-operations-etc)
    - [Other Calculations](#other-calculations)
    - [Output Features](#output-features)
  - [Other Auxiliary Functions](#other-auxiliary-functions)
    - [Calculation of Rotation Matrix](#calculation-of-rotation-matrix)
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
| Number of Rows | Arbitrary | 1-4 or dynamic |
| Number of Columns | Arbitrary | 1-4 or dynamic |
| Arithmetic Operations | ✅ | ✅ |
| [Other Simple Linear Algebra Operations](#other-calculations) | ✅ | ✅ |
| LU Decomposition | ✅ | ❌ |
| Eigenvalue Computation | ✅ | ❌ |
| Advanced Linear Algebra | ✅ | ❌ |

#### Performance Characteristics

- **Eigen**: Highly optimized with consideration for SIMD instructions and cache efficiency
- **IGESio Custom Implementation**: Simple implementation without lazy evaluation, resulting in performance inferior to Eigen

## IGESio Matrix Class Detailed Specifications

　This library defines the `igesio::Matrix` class for handling matrices with 2 or 3 rows. This class has a (somewhat) similar interface to Eigen's matrix class and supports matrix and vector operations. **This class does not have API compatibility with `Eigen::Matrix`, so please do not use it directly**. Instead, use the [type aliases](#type-aliases) below.

- **Number of Rows & Columns**: Fixed (1-4) or dynamic
- **Internal Data**: Stored in column-major format
- **Supported Operations**
    - Element access
    - Matrix/vector addition and subtraction
    - Scalar multiplication and division
    - Matrix-matrix and matrix-vector products
    - Resizing (for dynamic column count; only column count can be changed with `conservativeResize`)
    - Several other basic linear algebra operations

### Template Parameters

- `T`: Type of matrix value (`double` or `float`)
- `N`: Number of rows; `M`: Number of columns (one of the following)
  - `igesio::Dynamic` (-1): Dynamic number of columns
  - 1-4: Fixed number of columns

```cpp
namespace igesio {

template<typename T, int N, int M>
class Matrix {
// ...
```

### Type Aliases

　The following aliases are provided for frequently used types. It is recommended **to use these type aliases** instead of directly using the `igesio::Matrix` class.

　Matrix type aliases are named in the format `Matrix` + `<Size>` + `d/f` (e.g., `Matrix2d`, `Matrix3Xf`). Here, `d` represents `double` type, and `f` represents `float` type. The `<Size>` part is as follows:

| Type | `<Size>` | Example | Description |
|:---|:---|:---|:---|
| Fixed rows and columns (square matrix) | `2`, `3`, `4` | `Matrix2d` | 2x2 matrix (double type) |
| Fixed rows and dynamic columns | `2X`, `3X`, `4X` | `Matrix3Xf` | 3-row dynamic column matrix (float type) |
| Dynamic rows and fixed columns | `X2`, `X3`, `X4` | `MatrixX4d` | Dynamic-row 4-column matrix (double type) |
| Dynamic rows and dynamic columns | `X` | `MatrixXd` | Dynamic-row and dynamic-column matrix (double type) |

　Vector type aliases are named in the format `Vector/RowVector` + `<Size>` + `d/f` (e.g., `Vector2d`, `RowVector3f`). Again, `d` represents the `double` type, and `f` represents the `float` type. The `<Size>` part is as follows:

| Type | `<Size>` | Example | Description |
|:---|:---|:---|:---|
| Fixed size | `2`, `3`, `4` | `Vector2d` | 2D column vector (double type) |
| Dynamic size | `X` | `RowVectorXd` | Dynamic-size row vector (double type) |

> `Vector` is an alias for `Matrix<T, N, 1>`, representing a column vector. `RowVector` is an alias for `Matrix<T, 1, M>`, representing a row vector.

## Member Functions

### Constructors

　The custom implementation `igesio::Matrix` class in this library supports the following constructors:

| Constructor | Description |
|---|---|
| `Matrix()` | Default constructor. For fixed row and column counts, it is initialized with the specified size. If either the row or column count is dynamic, it is initialized as empty. Element values are undefined. If you want to initialize with values set, use static member functions such as `Matrix::Zero()`. |
| `Matrix(rows, cols)` | Can be used for matrices with dynamic row or column counts. `rows` specifies the number of rows, and `cols` specifies the number of columns. Element values are undefined. If you want to initialize with values set, use static member functions such as `Matrix::Zero()`. |
| `Matrix{args...}` | Constructor using variable-length arguments. Use it like `Vector3d vec(1.0, 2.0, 3.0);` or `RowVector3d row_vec(1.0, 2.0, 3.0);`. The number of arguments specified must match either the number of rows or columns. |
| `Matrix{init_list}` | Constructor using an initializer list (for vectors). Use it like `Vector2d v = {1.0, 2.0}`. The number of elements in the initializer list must match the vector's row count `N`. |
| `Matrix{init_list}` | Constructor using an initializer list. Use it like `Matrix2d m = {{1, 2}, {3, 4}}`. The number of rows must match N, and the number of columns must match M if the column count is fixed. |

　Also, the following static member functions allow easy creation of zero matrices and identity matrices.

| Member Function | Description |
|---|---|
| `Matrix::Zero()` | Creates a zero matrix (fixed size version).<br>Returns a matrix with all elements initialized to 0.0. |
| `Matrix::Zero(int rows, int cols)` | Creates a zero matrix (dynamic size version).<br>`rows` is the number of rows (must match `N`), `cols` is the number of columns. |
| `Matrix::Identity()` | Creates an identity matrix (fixed size version).<br>Returns a square matrix with diagonal elements as 1.0 and others as 0.0.<br>**Available only for square matrices**. |
| `Matrix::Identity(int rows, int cols)` | Creates an identity matrix (dynamic size version).<br>`rows` is the number of rows (must match `N`), `cols` is the number of columns.<br>**Available only for square matrices**. |
| `Matrix::Constant(double value)` | Creates a constant matrix (fixed size version).<br>Returns a matrix with all elements initialized to `value`. |
| `Matrix::Constant(int rows, int cols, double value)` | Creates a constant matrix (dynamic size version).<br>`rows` is the number of rows (must match `N`), `cols` is the number of columns, `value` is the initial value. |
| `Matrix::Unit(size_t i)` | Creates a unit vector. The argument `i` specifies the index of the non-zero element. |
| `Matrix::UnitX()` | Creates the unit vector $[1\; 0\; 0]^\top$. |
| `Matrix::UnitY()` | Creates the unit vector $[0\; 1\; 0]^\top$. |
| `Matrix::UnitZ()` | Creates the unit vector $[0\; 0\; 1]^\top$. |

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
| `v(i)`<br>`v[i]` | `double&`<br>`const double&` | Accesses vector elements. `i` is the index.<br>**Available only for vectors (1-column or 1-row matrices)**. |
| `m.col(j)` | Vector | Returns a vector containing the elements of column `j`. |
| `m.row(i)` | RowVector | Returns a vector containing the elements of row `i`. |
| `v.x()`<br>`v.y()`<br>`v.z()`<br>`v.w()` | `double&`<br>`const double&` | Accesses the x, y, z, w coordinate elements of the vector.<br>**Available only for vectors (1-column or 1-row matrices)**. |
| `m.data()` | `double*`<br>`const double*` | Returns a pointer to an array containing all elements of the matrix in column-major order. |
| `m.block(i, j, rows, cols)`<br>`m.block<rows, cols>(i, j)` | `Matrix`<br>`const Matrix` | Gets a submatrix of the matrix. `i` is the starting row, `j` is the starting column, and `rows` and `cols` are the size of the submatrix. Elements can be obtained and set. An example is shown below. |

```cpp
// Getting a fixed-size submatrix
igesio::Matrix3d mat = igesio::Matrix3d::Identity();
auto block = mat.block<2, 2>(0, 0);

// Getting a dynamic-size submatrix
igesio::MatrixXd dyn_mat = igesio::MatrixXd::Identity(4, 4);
auto dyn_block = dyn_mat.block(1, 1, 2, 2);

// Setting elements
mat.block<2, 2>(0, 0) = igesio::Matrix2d::Zero();
dyn_mat.block(1, 1, 2, 2) = igesio::MatrixXd::Zero(2, 2);
```

#### Size Change/Conversion

　The size of the matrix can be changed using the following member functions.

- `m.resize(new_rows, new_cols)`: Changes the size and initializes the elements with 0 if necessary. An error occurs if you try to resize a fixed-size matrix.
- `m.conservativeResize(new_rows, new_cols)`: Changes the size and preserves the data as much as possible. An error occurs if you try to resize a fixed-size matrix.
- `m.reshaped(new_rows, new_cols)`: Returns a new matrix with the changed size. The data of the original matrix is copied.
- `m.transpose()`: Returns the transpose matrix.
- `m.transposeInPlace()`: Transposes itself (only for square matrices or dynamic sizes).
- `m.cast<NewType>()`: Returns a matrix with the element type converted to `NewType`.

```cpp
// Creating a 3-row x dynamic-column matrix
igesio::Matrix3Xd mat3xN(3, 5);  // 3 rows, 5 columns
mat3xN = {{ 1.0,  2.0,  3.0,  4.0,  5.0},
      { 6.0,  7.0,  8.0,  9.0, 10.0},
      {11.0, 12.0, 13.0, 14.0, 15.0}};

// Resize to 3 rows x 8 columns (keep data)
// [ 1  2  3  4  5  0  0  0]
// [ 6  7  8  9 10  0  0  0]
// [11 12 13 14 15  0  0  0]
mat3xN.conservativeResize(3, 8);

// Resize to 3 rows x 4 columns (keep data)
// [ 1  2  3  4]
// [ 6  7  8  9]
// [11 12 13 14]
mat3xN.conservativeResize(3, 4);

// Resize to 2 rows x 4 columns (discard data)
// [0 0 0 0]
// [0 0 0 0]
mat3xN.resize(2, 4);
```

#### Arithmetic Operations etc.

　The following is a list of arithmetic operations supported by the custom implementation `igesio::Matrix` class in this library. For operations between matrices, it is possible to mix fixed-size and dynamic-size matrices, but the matrix sizes must match.

| Operation | Formula | Description/Code Example |
|:--:|:--:|---|
| Negation | $-M$ |  `m = -m;` |
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
| `Matrix` |  | Normalizes a vector (converts to a unit vector).<br>`m.normalized()` |
| `double` | $\sum m_{i,j}^2$ | Calculates the sum of squares of matrix elements.<br>`m.squaredNorm()` |
| `double` | $\sqrt{\sum m_{i,j}^2}$ | Calculates the norm (Euclidean distance) of matrix elements.<br>`m.norm()` |
| `double` | $\sum m_{i,j}$ | Calculates the sum of matrix elements.<br>`m.sum()` |
| `double` | $\prod m_{i,j}$ | Calculates the product of matrix elements.<br>`m.prod()` |
| `double` |  | Calculates the determinant (only for 2x2, 3x3, 4x4 matrices).<br>`m.determinant()` |

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

### Other Auxiliary Functions

#### Calculation of Rotation Matrix

　The `AngleAxis` function can be used to generate a rotation matrix from a rotation angle and a rotation axis.

```cpp
#include "igesio/numerics/matrix.h"

// Generate a rotation matrix from a rotation angle and a rotation axis
auto axis = igesio::Vector3d::UnitY();  // Use the Y-axis as the rotation axis
double angle = M_PI / 2.0;                // Rotate 90 degrees
auto rotation_matrix = igesio::AngleAxisd(angle, axis);

std::cout << rotation_matrix << std::endl;
```

　Note that unlike the `Eigen::AngleAxis` class in the Eigen library, `AngleAxis` is provided as a function. Also, the rotation axis must be a unit vector.

```cpp
/// @brief Function to generate a rotation matrix from a rotation angle and a rotation axis
template<typename T>
Matrix<T, 3, 3> AngleAxis(const T angle, const Vector<T, 3>& axis);
```

　The following helper functions are also provided: `AngleAxisd` for `double` type and `AngleAxisf` for `float` type.

## Usage Examples

### Example of Processing Using the Matrix Class

```cpp
#include <iostream>
#include "igesio/numerics/matrix.h"

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
#include "igesio/numerics/matrix.h"

// These are actually aliases to Eigen classes
igesio::Matrix2d    // Eigen::Matrix2d
igesio::Vector3d    // Eigen::Vector3d
igesio::Matrix3Xd   // Eigen::Matrix3Xd
```

#### When Using Custom Implementation

```cpp
// When IGESIO_ENABLE_EIGEN is not defined, custom implementation is automatically used
#include "igesio/numerics/matrix.h"

// Custom Matrix class is used
igesio::Matrix2d    // igesio::Matrix<double, 2, 2>
igesio::Vector3d    // igesio::Matrix<double, 3, 1>
igesio::Matrix3Xd   // igesio::Matrix<double, 3, igesio::Dynamic>
```
