# Copious Data (Type 106)

Defined at:

- [curves/copious_data_base.h](./../../../include/igesio/entities/curves/copious_data_base.h): Base class for Copious Data entities
- [curves/copious_data.h](./../../../include/igesio/entities/curves/copious_data.h): Sequence of points (forms 1-3)
- [curves/linear_path.h](./../../../include/igesio/entities/curves/linear_path.h): Polyline (form 11-13)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
    - [`CopiousDataType`](#copiousdatatype)
  - [曲線の定義](#曲線の定義)
    - [曲線 $C(t)$](#曲線-ct)
    - [導関数 $C'(t), C''(t)$](#導関数-ct-ct)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetDataType()` <br> (`CopiousDataType`) | 点群の種類を取得する |
| `GetIP()` <br> (`int`) | 点群の次元数 IP <br> - 1: 2次元点列 (全点のz座標は同じ) <br> - 2: 3次元点列 <br> - 3: 3次元点列 + 追加情報ベクトル |
| `GetCount()` <br> (`size_t`) | 点列の点数 $n$ |
| `Coordinate(i)` <br> (`Vector3d`) | 点列の`i`番目の点 $P_i = (x_i, y_i, z_i)$ |
| `Addition(i)` <br> (`Vector3d`) | 点列の`i`番目の点に対応する 追加情報ベクトル $A_i = (a_{ix}, a_{iy}, a_{iz})$ |
| `Coordinates()` <br> (`Matrix3Xd`) | 全座標値の行列 $[P_0, P_1, \ldots, P_{n-1}] \in \mathbb{R}^{3 \times n}$ |
| `Additions()` <br> (`Matrix3Xd`) | 全追加情報ベクトルの行列 $[A_0, A_1, \ldots, A_{n-1}] \in \mathbb{R}^{3 \times n}$ |
| `GetSegmentIndexAt(t)` <br> (`std::optional<size_t>`) | パラメータ $t$ に対応するセグメントのインデックス $i$ (範囲外の場合は`std::nullopt`) |
| `GetNearestVertexAt(t)` <br> (`std::pair<size_t, double>`) | パラメータ $t$ に最も近い点のインデックスと, その点への距離 |

> 追加情報ベクトルは、IGES 5.3規格書においては $I(1), J(1), K(1)$ などと記述されているものに対応します。本ドキュメントでは、他の記号との混同を避けるため, $A_i = (a_{ix}, a_{iy}, a_{iz})$ と表記しています。

> 追加情報ベクトルは、`IP=3` の場合にのみ定義されます。具体的には、`CopiousDataType::kSextuples` (form 3) および `CopiousDataType::kPolylineAndVectors` (form 13) の場合にのみ値が設定されます（それ以外では未定義）。

#### `CopiousDataType`

　`CopiousDataType`列挙体は、エンティティのフォーム番号（`GetFormNumber()`）に一対一で対応する、点群の種類を表します。詳細は以下の通りです。

| form | 列挙体値 | IP | 説明 |
|---|---|:-:|---|
| 1 | `kPlanarPoints` | 1 | 平面 $z = z_t$ 上の点列 |
| 2 | `kPoints3D` | 2 | 3次元空間内の点列 |
| 3 | `kSextuples` | 3 | 3次元空間内の点列 + 追加情報ベクトル |
| 11 | `kPlanarPolyline` | 1 | 平面 $z = z_t$ 上の折れ線 |
| 12 | `kPolyline3D` | 2 | 3次元空間内の折れ線 |
| 13 | `kPolylineAndVectors` | 3 | 3次元空間内の折れ線 + 各頂点に対応する追加情報ベクトル |
| 20 | `kCenterlineByPoints` | 1 | 点群を通る中心線 |
| 21 | `kCenterlineByCircles` | 1 | 円の中心点群を通る中心線 |
| 31 | `kGeneralHatch` | 1 | 断面ハッチング: 鋳鉄または可鍛鋳鉄およびすべての材料の一般用途 |
| 32 | `kSteelHatch` | 1 | 断面ハッチング: 鋼 |
| 33 | `kBronzeHatch` | 1 | 断面ハッチング: 青銅、真鍮、銅、および組成物 |
| 34 | `kRubberHatch` | 1 | 断面ハッチング: ゴム・プラスチックおよび電気絶縁 |
| 35 | `kTitaniumHatch` | 1 | 断面ハッチング: チタンおよび耐火物 |
| 36 | `kMarbleHatch` | 1 | 断面ハッチング: 大理石、スレート、ガラス、磁器 |
| 37 | `kZincHatch` | 1 | 断面ハッチング: ホワイトメタル、亜鉛、鉛、バビット、および合金 |
| 38 | `kAluminumHatch` | 1 | 断面ハッチング: マグネシウム、アルミニウム、およびアルミニウム合金 |
| 40 | `kWitnessLine` | 1 | 証拠線 |
| 63 | `kPlanarLoop` | 1 | 単純な閉じた平面曲線 |

　forms 1-3 は`CopiousData`クラスで、forms 11-13, 63 は`LinearPath`クラスで実装されています。forms 20, 21, 31-38, 40 は未実装です。

### 曲線の定義

#### 曲線 $C(t)$

　Copious Dataエンティティにおけるパラメトリック曲線 $C(t)$ の媒介変数 $t$ は、点列の各点間を線形補間する形で定義されます。 $i$ 番目のセグメント ( $P_i$ から $P_{i+1}$ ) の長さを $L_i = \| P_{i+1} - P_i \|$ とすると, $t$ の範囲は以下のようになります。

$$t \in \left[0, \quad \sum_{i=0}^{n-2} L_i \right]$$

　また、パラメータ値 $t$ に対応するセグメントが $i \ (= 0, 1, \ldots n-2)$ 番目である場合, $t$ は以下の条件を満たします。

$$\sum_{j=0}^{i-1} L_j \leq t < \sum_{j=0}^{i} L_j$$

**forms 1-3の場合**

　このとき、曲線 $C(t)$ は以下のように定義されます。すなわち、現在の $t$ に対応する点が、セグメント $i$ における始点 $P_i$ (または $i = n-1$ の場合は終点 $P_{n-1}$ も可能) に一致した場合にのみ座標値が定義され、それ以外の場合は未定義となります（線形補間は行われません）。本ライブラリにおいては, $t$ における線形補間座標と $P_i$ との距離が非常に小さい場合にのみその点を返し、それ以外の場合は`std::nullopt`を返すように実装されています。

$$C(t) = \begin{cases}
  P_i & (\text{if } t = \sum_{j=0}^{i-1} L_j) \\\
  P_{n-1} & (\text{if } t = \sum_{j=0}^{n-2} L_j) \\\
  \text{Undefined} & (\text{otherwise})
\end{cases}$$

**forms 11-13の場合**

　このとき、曲線 $C(t)$ は以下のように定義されます。すなわち、現在の $t$ に対応する点が、セグメント $i$ における始点 $P_i$ と終点 $P_{i+1}$ の間に位置する場合、線形補間によって座標値が定義されます。

$$C(t) = P_i + \frac{t - \sum_{j=0}^{i-1} L_j}{L_i} (P_{i+1} - P_i)$$

**forms 20, 21, 31-38, 40の場合**

未実装です。また、これらのフォームはいずれもAnnotationエンティティとして扱われるため、曲線の幾何学的特性は定義されません。

**form 63の場合**

　Copious Data (form 63) は、単純な閉じた平面曲線を表します。終点 $P_{n-1}\ (n \geq 2)$ は始点 $P_0$ に接続され、曲線は閉じたループを形成します。終点から始点を結ぶ線分を $n-1$ 番目のセグメントとみなし、その距離を $L_{n-1} = \| P_0 - P_{n-1} \|$ とすると、パラメータ $t$ の範囲と、各セグメントにおける $t$ の条件は以下のようになります。

$$t \in \left[0, \quad \sum_{i=0}^{n-1} L_i \right]$$

$$\sum_{j=0}^{i-1} L_j \leq t < \sum_{j=0}^{i} L_j \quad (i = 0, 1, \ldots, n-1)$$

　このとき、曲線 $C(t)$ は以下のように定義されます。

$$C(t) = P_i + \frac{t - \sum_{j=0}^{i-1} L_j}{L_i} (P_{(i+1) \bmod n} - P_i)$$

#### 導関数 $C'(t), C''(t)$

　Copious Dataエンティティにおけるパラメトリック曲線 $C(t)$ の1階および2階の導関数は、以下のように定義されます。 $t$ の定義領域については、いずれも前述のものに従います。

**forms 1-3の場合**

　点列については、常に導関数は未定義となります。ライブラリ上では、常に`std::nullopt`を返すように実装されています。

$$C'(t) = \text{Undefined}, \quad C''(t) = \text{Undefined}$$

**forms 11-13の場合**

$$C'(t) = \frac{P_{i+1} - P_i}{L_i}, \quad C''(t) = 0$$

**forms 20, 21, 31-38, 40の場合**

未実装です。これらのフォームはいずれもAnnotationエンティティとして扱われるため、曲線の幾何学的特性は定義されません。

**form 63の場合**

$$C'(t) = \frac{P_{(i+1) \bmod n} - P_i}{L_i}, \quad C''(t) = 0$$

　曲線 $C(t)$ および導関数 $C'(t), C''(t)$ から計算される各種特徴量については、[Geometric Properties (曲線の幾何学的性質)](./../geometric_properties_ja.md) を参照してください。
