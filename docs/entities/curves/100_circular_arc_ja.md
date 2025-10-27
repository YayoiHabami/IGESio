# Circular Arc (Type 100)

Defined at [curves/circular_arc.h](./../../../include/igesio/entities/curves/circular_arc.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
  - [曲線の定義](#曲線の定義)
    - [曲線 $C(t)$](#曲線-ct)
    - [導関数 $C'(t), C''(t)$](#導関数-ct-ct)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `Center()` <br> (`Vector3d`) | 円弧中心 $O_c = (x_c, y_c, z_t)$ |
| `Radius()` <br> (`double`) | 半径 $r$ |
| `StartAngle()` <br> (`double`) | 開始角度 $\theta_s$ [rad] |
| `EndAngle()` <br> (`double`) | 終了角度 $\theta_e$ [rad] |

> Circular Arc (Type 100) エンティティは、定義空間上の $z = z_t$ 平面内に位置します (`NDim() == 2`)。

### 曲線の定義

#### 曲線 $C(t)$

　円弧のパラメトリック曲線 $C(t)$ は、次のように定義されます。

$$C(t) = \begin{bmatrix} x_c + r \cos{t} \\\ y_c + r \sin{t} \\\ z_t \end{bmatrix}, \quad t \in [\theta_s, \theta_e]$$

ここで, $O_c = (x_c, y_c, z_t)$ は円弧の中心, $r$ は半径, $\theta_s, \theta_e$ は開始角度と終了角度です。また、開始角度 $\theta_s$ と終了角度 $\theta_e$ は、以下の条件を満たします。

$$\left\lbrace\begin{aligned}
  &\quad\quad\  \theta_s < \theta_e \\\
  0 &\leq \quad \theta_s &< 2\pi \\\
  0 &\leq \quad \theta_e - \theta_s &< 2\pi
\end{aligned}\right.$$

　開始角度 $\theta_s$ と終了角度 $\theta_e$ の間には、常に $\theta_s < \theta_e$ の関係が存在するため、円弧は常に反時計回りに定義されます。そのため、時計回りの円弧を定義する場合には、`CircularArc::OverwriteTransformationMatrix(matrix)`関数を使用して、3次元空間内で回転処理を行う変換行列を指定する必要があります。

#### 導関数 $C'(t), C''(t)$

　円弧の1階および2階の導関数は、次のように定義されます。

$$
C'(t)  = \begin{bmatrix} -r \sin{t} \\\ r \cos{t} \\\ 0 \end{bmatrix}, \quad
C''(t) = \begin{bmatrix} -r \cos{t} \\\ -r \sin{t} \\\ 0 \end{bmatrix}
$$

　曲線 $C(t)$ および導関数 $C'(t), C''(t)$ から計算される各種特徴量については、[Geometric Properties (曲線の幾何学的性質)](./../geometric_properties_ja.md) を参照してください。
