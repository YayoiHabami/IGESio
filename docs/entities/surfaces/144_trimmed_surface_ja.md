# Trimmed (Parametric) Surface (Type 144)

Defined at [surfaces/trimmed_surface.h](./../../../include/igesio/entities/surfaces/trimmed_surface.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
  - [有効定義域](#有効定義域)
    - [パラメータ矩形 D](#パラメータ矩形-d)
    - [外側境界と内側境界](#外側境界と内側境界)
    - [有効定義域 Ω](#有効定義域-ω)
  - [曲面の定義](#曲面の定義)
- [Appendix](#appendix)
  - [内外判定の実装](#内外判定の実装)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetSurface()` <br> `SetSurface(surface)` <br> (`std::shared_ptr<const ISurface>`) | トリミング対象の曲面 $S(u, v)$ へのポインタ |
| `IsOuterBoundaryOfD()` <br> (`bool`) | 外側境界がパラメータ矩形 $D$ の境界かどうか ($N_1 = 0$ → `true`) |
| `GetOuterBoundary()` <br> `SetOuterBoundary(outer)` <br> (`std::shared_ptr<const CurveOnAParametricSurface>`) | 外側境界曲線 (Type 142) へのポインタ。`IsOuterBoundaryOfD()`が`true`の場合は`nullptr`を返す |
| `GetInnerBoundaryCount()` <br> (`size_t`) | 内側境界曲線の数 $N_2$ |
| `GetInnerBoundaryAt(index)` <br> `AddInnerBoundary(boundary)` <br> `RemoveInnerBoundaryAt(index)` <br> (`std::shared_ptr<const CurveOnAParametricSurface>`) | 内側境界曲線 (Type 142) へのポインタ。インデックス`index` (0始まり) の内側境界曲線を取得・追加・削除する |

### 有効定義域

#### パラメータ矩形 D

　トリミング対象の曲面 $S(u, v)$ は、パラメータ範囲 $[a, b] \times [c, d]$ で定義される矩形領域 $D$ を持つ。すなわち、

$$D = \{\ (u, v)\ |\ a \leq u \leq b,\ c \leq v \leq d\ \}$$

#### 外側境界と内側境界

　トリム済み曲面の有効定義域は、外側境界と内側境界によって定義される。いずれの境界曲線も、パラメータ空間 $(u, v)$ 内の単純閉曲線であり、[`CurveOnAParametricSurface (Type 142)`](../curves/142_curve_on_a_parametric_surface_ja.md) で表される。
v
外側境界の指定方法は $N_1$ の値によって異なる。

- $N_1 = 0$ (`IsOuterBoundaryOfD()` = `true`) : 外側境界はパラメータ矩形 $D$ の境界と一致する。$PTO$ はヌルポインタとなる。
- $N_1 = 1$ (`IsOuterBoundaryOfD()` = `false`) : 外側境界は $D$ 内に定義された単純閉曲線 $\partial\Omega_{\text{out}}$ で与えられる。$PTO$ はその境界曲線へのポインタである。

内側境界は $N_2$ 本の単純閉曲線 $\partial\Omega_{\text{in},1},\ \ldots,\ \partial\Omega_{\text{in},N_2}$ で構成される。各内側境界は以下の条件を満たさなければならない。

- 各内側境界の閉領域は互いに素である（共有点を持たない）。
- 各内側境界は外側境界の内部に完全に含まれる。

#### 有効定義域 Ω

　単純閉曲線 $C$ の内部領域を $\mathrm{int}(C)$、外部領域を $\mathrm{ext}(C)$ と表すことにする。このとき、トリム済み曲面の有効定義域 $\Omega$ は次のように定義される。

$$\Omega = \overline{\mathrm{int}(\partial\Omega_{\text{out}})} \cap \bigcap_{i=1}^{N_2} \overline{\mathrm{ext}(\partial\Omega_{\text{in},i})}$$

ここで $\overline{(\cdot)}$ は閉包（境界曲線を含む閉じた領域）を表す。すなわち、 $\Omega$ は外側境界の内部かつ全ての内側境界の外部となる領域（境界を含む）である。

$N_1 = 0$ かつ $N_2 = 0$ の場合（外側境界が $D$ の境界でありかつ内側境界がない）、有効定義域は $\Omega = D$ となり、実質的にトリミングなしの曲面と等価である。

### 曲面の定義

　トリム済み曲面のパラメータ化 $S(u, v)$ は、トリミング対象の曲面と同一である。`TrimmedSurface`の`TryGetDerivatives`は、与えられた点 $(u, v)$ が有効定義域 $\Omega$ の内部にあるかを判定し、内部であればトリミング対象の曲面の偏導関数をそのまま返す。 $(u, v)$ が $\Omega$ の外部にある場合は`std::nullopt`を返す。

```
TryGetDerivatives(u, v, n)
  ├─ (u, v) が Ω 外 → std::nullopt
  └─ (u, v) が Ω 内 → surface_->TryGetDerivatives(u, v, n) に委譲
```

`GetParameterRange()`はトリミング対象の曲面のパラメータ範囲をそのまま返す（トリミングによってパラメータ化は変わらない）。

`GetDefinedBoundingBox()`はトリミング対象の曲面のバウンディングボックスをそのまま返す（トリム後の実際の3D領域より大きい場合があるが、保守的な上界として有効である）。

## Appendix

### 内外判定の実装

　`TryGetDerivatives`が内部で呼び出す`IsInTrimmedDomain(u, v)`は、パラメータ空間 $(u, v)$ 上の点が有効定義域 $\Omega$ 内にあるかを判定する。判定には`ComputeContainmentPolygons`（`entities/curves/algorithms.h`）および`IsPointInPolygon`（`numerics/polygon.h`）を使用する。

判定の流れは次の通りである。

```
IsInTrimmedDomain(u, v)
  │
  ├─ N1=0 かつ N2=0 → 常にtrue（最速パス）
  │
  ├─ domain_cache_が未構築 → BuildDomainCache()を呼び出す
  │    ├─ N1=1 の場合:
  │    │    outer_boundary_.GetBaseCurve() → B_out(t)を取得
  │    │    ComputeContainmentPolygons(B_out, 32, (0,0,1)) → cache.outerに格納
  │    └─ 各 inner_boundaries_[i]:
  │         inner.GetBaseCurve() → B_in_i(t) を取得
  │         ComputeContainmentPolygons(B_in_i, 32, (0,0,1)) → cache.inner[i]に格納
  │
  ├─ N1=1 の場合:
  │    IsPointInPolygon((u, v, 0), cache.outer) が false → false（外側境界の外側）
  │
  └─ 各内側境界について:
       IsPointInPolygon((u, v, 0), cache.inner[i]) が true → false（穴の内部）

  上記のいずれにも該当しない場合 → true
```

　`BuildDomainCache()`が構築する`DomainCache`は初回判定時にのみ構築され、以降は再利用される（遅延構築）。曲面や境界曲線が変更された場合はキャッシュが無効化され、次回の`IsInTrimmedDomain`呼び出し時に再構築される。

　境界曲線の基底曲線 $B(t) = (u(t), v(t))$ はパラメータ空間 $(u, v)$ 上の曲線であり、内外判定は3次元空間ではなくパラメータ空間上で行われることに注意する。`ComputeContainmentPolygons`に渡す法線ベクトルには $(0, 0, 1)$ を使用する（パラメータ空間は $z=0$ 平面として扱う）。
