# Line (Type 110)

Defined at [curves/line.h](./../../../include/igesio/entities/curves/line.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
    - [`LineType`](#linetype)
  - [曲線の定義](#曲線の定義)
    - [曲線 $C(t)$](#曲線-ct)
    - [導関数 $C'(t), C''(t)$](#導関数-ct-ct)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetLineType()` <br> (`LineType`) | 線分の種類 |
| `GetDefinedAnchorPoints()` <br> (`std::pair<Vector3d, Vector3d>`) | 定義空間における, 線分の始点 $P_1$ と終点 $P_2$ |
| `GetAnchorPoints()` <br> (`std::pair<Vector3d, Vector3d>`) | 線分の始点 $P_1$ と終点 $P_2$ |

#### `LineType`

　`LineType`列挙体は、エンティティのフォーム番号（`GetFormNumber()`）に一対一で対応する、線分の種類を表します。詳細は以下の通りです。

| form | 列挙体値 | 説明 |
|---|---|---|
| 0 | `kSegment` | 始点 $P_1$ と終点 $P_2$ の2つの端点を持つ線分 |
| 1 | `kRay` | 始点 $P_1$ を端点とし、終点 $P_2$ 方向に無限に伸びる半直線 |
| 2 | `kLine` | 始点 $P_1$ と終点 $P_2$ を通る無限に伸びる直線 |

### 曲線の定義

#### 曲線 $C(t)$

　線分のパラメトリック曲線 $C(t)$ は、次のように定義されます。

$$C(t) = P_1 + t (P_2 - P_1)$$

ここで、媒介変数 $t$ の範囲は、`LineType` によって以下のように異なります。

$$t \in \begin{cases}
  [0, 1] & \text{if kSegment} \\\
  [0, \infty) & \text{if kRay} \\\
  (-\infty, \infty) & \text{if kLine}
\end{cases}$$

#### 導関数 $C'(t), C''(t)$

　線分の1階および2階の導関数は、次のように定義されます。

$$C'(t) = P_2 - P_1, \quad C''(t) = 0$$

　曲線 $C(t)$ および導関数 $C'(t), C''(t)$ から計算される各種特徴量については、[Geometric Properties (曲線の幾何学的性質)](./../geometric_properties_ja.md) を参照してください。
