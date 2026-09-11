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

　開始角度 $\theta_s$ と終了角度 $\theta_e$ の間には、常に $\theta_s < \theta_e$ の関係が存在するため、IGESファイル上の円弧は常に反時計回りに定義されます。一方、本ライブラリの`CircularArc`は時計回りの円弧も表現可能です（`IsClockwise()`）。どちらの円弧でもパラメータ範囲は $[\theta_s, \theta_s + \Delta]$ ($\Delta$ は角度範囲`SweepAngle()`) ですが、時計回りではパラメータの増加に対して幾何角度が減るため、`EndAngle()` $= \theta_s - \Delta$ は`StartAngle()`より小さくなります。

　IGESファイルへ書き出す際(`ConvertToIntermediate`/`WriteIges`)、時計回りの円弧は規格に適合する2エンティティに展開されます。定義空間で直線 $y = y_c$ について鏡映した反時計回りの円弧と、中心を通る $X_T$ 軸平行線まわりの $\pi$ 回転を表す変換行列（Type 124）です。鏡映した円弧は元の円弧のDE枠に出力されるため、他エンティティ（複合曲線など）からの参照はそのまま有効であり、元の円弧が変換行列を参照していた場合は新しい変換行列がそれに連鎖します。時計回りの円弧をType 142/144のパラメータ空間曲線として用いることは、読み込み側がこの変換行列を正しく解釈することに依存するため推奨しません。

#### 導関数 $C'(t), C''(t)$

　円弧の1階および2階の導関数は、次のように定義されます。

$$
C'(t)  = \begin{bmatrix} -r \sin{t} \\\ r \cos{t} \\\ 0 \end{bmatrix}, \quad
C''(t) = \begin{bmatrix} -r \cos{t} \\\ -r \sin{t} \\\ 0 \end{bmatrix}
$$

　曲線 $C(t)$ および導関数 $C'(t), C''(t)$ から計算される各種特徴量については、[Geometric Properties (曲線の幾何学的性質)](./../geometric_properties_ja.md) を参照してください。
