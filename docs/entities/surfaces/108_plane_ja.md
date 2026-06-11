# Plane (Type 108)

Defined at [surfaces/plane.h](./../../../include/igesio/entities/surfaces/plane.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
    - [`Plane` (Form 0)](#plane-form-0)
    - [`BoundedPlane` (Form 1 / -1)](#boundedplane-form-1---1)
  - [平面の定義](#平面の定義)
    - [平面の方程式](#平面の方程式)
    - [定義空間フレーム](#定義空間フレーム)
  - [Form番号](#form番号)
- [曲面の定義](#曲面の定義)
  - [無限平面 (Form 0)](#無限平面-form-0)
  - [有界平面 (Form 1 / -1)](#有界平面-form-1---1)
  - [表示シンボル](#表示シンボル)
- [Appendix](#appendix)
  - [無限パラメータ範囲の扱い](#無限パラメータ範囲の扱い)

## Parameters

　平面エンティティ (Type 108) は、フォーム番号によって2つのクラスに分かれる。フォーム0は定義域が全平面の無限平面 `Plane` であり、フォーム1および-1は平面上の単純閉曲線で定義域を制限した有界平面 `BoundedPlane` である。いずれの場合も、平面は4つの係数 $A, B, C, D$ によって定義空間内に定義される。

### 固有のパラメータ

#### `Plane` (Form 0)

| メンバ関数 | 説明 |
|---|---|
| `GetCoefficients()` <br> (`const std::array<double, 4>&`) | 平面係数 $\lbrace A, B, C, D\rbrace$ の取得 |
| `GetFrame()` <br> (`PlaneFrame`) | 定義空間フレーム (原点・基底ベクトル) の取得。[定義空間フレーム](#定義空間フレーム)を参照 |
| `GetSymbolLocation()` <br> (`Vector3d`) | 表示シンボルの位置点 $(X, Y, Z)$ の取得 (表示専用) |
| `GetSymbolSize()` <br> (`double`) | 表示シンボルのサイズ $SIZE$ の取得 (0 でシンボルなし) |

#### `BoundedPlane` (Form 1 / -1)

| メンバ関数 | 説明 |
|---|---|
| `GetCoefficients()` <br> (`const std::array<double, 4>&`) | 平面係数 $\lbrace A, B, C, D\rbrace$ の取得 |
| `GetFrame()` <br> (`PlaneFrame`) | 定義空間フレーム (原点・基底ベクトル) の取得 |
| `GetSymbolLocation()` <br> (`Vector3d`) | 表示シンボルの位置点 $(X, Y, Z)$ の取得 (表示専用) |
| `GetSymbolSize()` <br> (`double`) | 表示シンボルのサイズ $SIZE$ の取得 (0 でシンボルなし) |
| `IsNegative()` <br> (`bool`) | 負領域 (Form -1, 穴) かどうかの取得 |
| `GetBoundaryCurve()` <br> `SetBoundaryCurve(boundary)` <br> (`std::shared_ptr<const ICurve>`) | 平面上の単純閉曲線 ($PTR$) の取得・設定 |

### 平面の定義

#### 平面の方程式

　平面は、定義空間において4つの係数 $A, B, C, D$ によって定義される。定義空間座標 $(X_T, Y_T, Z_T)$ を持つ平面上のすべての点について、次式が成り立つ。

$$A \cdot X_T + B \cdot Y_T + C \cdot Z_T = D$$

ここで、 $A, B, C$ の少なくとも1つは非ゼロでなければならない。ベクトル $(A, B, C)$ は平面の法線方向を表す。係数 $(A, B, C)$ がすべてゼロの場合、コンストラクタおよびファクトリ関数は`igesio::EntityValueError`を送出する。

#### 定義空間フレーム

　`GetFrame()`は、平面係数から一意に定まる定義空間フレーム`PlaneFrame`を返す。フレームは原点 $\text{origin}$、面内の2つの単位ベクトル $e_u, e_v$、および単位法線 $\hat{n}$ から構成される。

$$\hat{n} = \frac{(A, B, C)}{\|(A, B, C)\|}, \quad \text{origin} = \frac{D}{\|(A, B, C)\|^2} (A, B, C)$$

ここで $\text{origin}$ は、定義空間原点から平面へ下ろした垂線の足である。 $e_u$ は $\hat{n}$ に直交する単位ベクトル、 $e_v = \hat{n} \times e_u$ であり、 $e_u \times e_v = \hat{n}$ を満たす右手系を構成する。

### Form番号

　Type 108 のフォーム番号は、定義域の制限の有無と、有界領域の正負を表す。

| Form | クラス | 意味 | $PTR$ |
|:---:|---|---|---|
| $1$ | `BoundedPlane` | 有界平面 (正領域) | 平面上の単純閉曲線へのポインタ |
| $0$ | `Plane` | 無限平面 | ヌルポインタ (ゼロ) |
| $-1$ | `BoundedPlane` | 有界平面 (負領域・穴) | 平面上の単純閉曲線へのポインタ |

　フォーム1とフォーム-1は、幾何形状そのものは同一であり、領域の符号 (sense) のみが異なる。フォーム1は有界な平面領域を正 (実体)、フォーム-1は負 (穴) として扱う。

## 曲面の定義

### 無限平面 (Form 0)

　`Plane`は、定義空間における曲面を、定義空間フレームを用いて次のようにパラメータ化する。

$$S(u, v) = \text{origin} + u \cdot e_u + v \cdot e_v$$

パラメータ範囲は両方向とも無限である。すなわち`GetParameterRange()`は $\lbrace -\infty, +\infty, -\infty, +\infty \rbrace$ を返す。偏導関数は次のとおりで、`TryGetDefinedDerivatives`は領域全体で値を返す。

$$S_u = e_u, \quad S_v = e_v, \quad S^{(i, j)} = 0 \ (i + j \geq 2)$$

`GetDefinedBoundingBox()`は、面内2方向 ($e_u, e_v$) を無限直線として、法線方向のサイズを0とするバウンディングボックス (厚みゼロの2次元無限スラブ) を返す。

### 有界平面 (Form 1 / -1)

　`BoundedPlane`は、平面 $S(u, v)$ の定義域を、平面上にある単純閉曲線 ($PTR$) によって制限した曲面である。この境界曲線は、始点と終点のみが一致する単純閉曲線でなければならない。`BoundedPlane`は内側境界 (穴) を持たず、`GetInnerBoundaryCount()`は常に0を返す。

　`BoundedPlane`は`IRestrictedSurface`を実装し、トリム済み曲面と同一の枠組みで定義域を扱う。具体的には次のように構成される。

- 基底曲面 ($\text{GetBaseSurface}$) には、同じ係数から構築した定義空間の`Plane` (恒等変換) を用いる。
- UV空間の外側境界 ($\text{GetOuterUVBoundary}$) は、境界曲線をモデル空間で離散化し、各点をモデル空間フレームへ内積射影して $(u, v)$ 化した閉ポリラインとして構成する。

　境界曲線はモデル空間で平面上に乗るため、この内積射影により $S(B(t)) = C(t)$ が厳密に満たされる。ここで $B(t)$ はUV空間の境界曲線、 $C(t)$ はモデル空間の境界曲線である。

　`GetDefinedBoundingBox()`は、UV境界のサンプル点を定義空間へ写した点群の軸平行バウンディングボックスを返す。境界曲線が未解決の場合は、基底曲面 (無限平面) のバウンディングボックスにフォールバックする。

### 表示シンボル

　パラメータ $X, Y, Z, SIZE$ は、システム依存の表示シンボルを定義空間内に位置づけるための表示専用パラメータであり、平面の幾何そのものには影響しない。本ライブラリでは値を保持し、読み込み・書き出しの往復で保存するのみである。 $SIZE$ を0とする (または省略する) ことは、表示シンボルを意図しないことを示す。

## Appendix

### 無限パラメータ範囲の扱い

　無限平面はパラメータ範囲が無限であるため、メッシュ生成や交差判定でサンプリング範囲を有限に打ち切る必要がある。`Plane`単体に対しては、共有定数`kInfiniteParamClamp` (`entities/interfaces/i_surface.h`) を用いて無限端を打ち切る。

　`BoundedPlane`のように基底曲面が無限パラメータ範囲を持つ制限面については、`GetRestrictedDomainUVBounds` (`entities/interfaces/i_restricted_surface.h`) が、ドメイン (外周∪内周の包含多角形) のUVバウンディングボックスにマージンを加えた有限パラメータ窓を返す。これにより、有界領域に即したグリッド範囲・交差サンプリング範囲が得られる。
