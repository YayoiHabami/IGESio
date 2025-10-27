# Composite Curve (Type 102)

Defined at [curves/composite_curve.h](./../../../include/igesio/entities/curves/composite_curve.h)

## 目次

- [目次](#目次)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetCurveCount()` <br> (`int`) | 構成曲線の数 $n$ |
| `GetCurveAt(index)` <br> (`std::shared_ptr<const ICurve>`) | `index`番目の構成曲線 $C_i$ |

### 曲線の定義

#### 曲線 $C(t)$

　Composite Curve エンティティは、複数の曲線エンティティを連結して一つの曲線を形成するエンティティです。各構成曲線として、`ICurve`インターフェースを実装した任意の曲線エンティティを設定できます。 $i\  (= 0, 1, \ldots, n-1)$ 番目の構成曲線を $C_i(t_i)\  (t_i \in [t_{s,i}, t_{e,i}])$ とします。ここで, $t_i$ は $C_i$ のパラメータ, $t_{s,i}, t_{e,i}$ はそれぞれ $C_i$ の開始および終了パラメータです。このとき、Composite Curve エンティティ全体のパラメトリック曲線 $C(u)$ は、以下のように定義されます。

$$C(t) = \begin{cases}
    C_0(t - t(0) + t_{s,0}), & t \in [t(0), t(1)) \\\
    C_1(t - t(1) + t_{s,1}), & t \in [t(1), t(2)) \\\
    \vdots & \vdots \\\
    C_{n-1}(t - t(n-1) + t_{s,n-1}), & t \in [t(n-1), t(n)]
\end{cases}$$

ただし, $t(i)$ は $i$ 番目の曲線の開始パラメータを基準とした累積パラメータであり、以下のように定義されます。

$$t(i) = \sum_{k=0}^{i-1} (t_{e,k} - t_{s,k}), \quad \text{for } i = 0, 1, \ldots, n$$

例えば $n=3$ であり, $C_0$ のパラメータ範囲が $t_0 \in [1, 2]$, $C_1$ のパラメータ範囲が $t_1 \in [3.5, 7]$, $C_2$ のパラメータ範囲が $t_2 \in [-2, 0]$ である場合、累積パラメータ $t(i)$ は以下のように計算されます。

| - | $t_{s,i}$ | $t_{e,i}$ | $t_{e,i} - t_{s,i}$ | $t(i)$ |
|:-:|:-:|:-:|:-:|:-:|
| $i=0$ | 1   | 2 | 1   | **0**   |
| $i=1$ | 3.5 | 7 | 3.5 | **1**   |
| $i=2$ | -2  | 0 | 2   | **4.5** |
| $i=3$ | -   | - | -   | **6.5** |

　また、隣接する曲線 $C_i$ と $C_{i+1}$ は、以下の連続性の条件を満たす必要があります（導関数の連続性は要求されません）。

$$C_i(t_{e,i}) = C_{i+1}(t_{s,i+1}), \quad \text{for } i = 0, 1, \ldots, n-2$$

#### 導関数 $C'(t), C''(t)$

　Composite Curve エンティティの導関数は、各構成曲線の導関数を用いて以下のように定義されます。

$$C'(t) = \begin{cases}
    C_0'(t - t(0) + t_{s,0}), & t \in [t(0), t(1)) \\\
    C_1'(t - t(1) + t_{s,1}), & t \in [t(1), t(2)) \\\
    \vdots & \vdots \\\
    C_{n-1}'(t - t(n-1) + t_{s,n-1}), & t \in [t(n-1), t(n)]
\end{cases}$$

$$C''(t) = \begin{cases}
    C_0''(t - t(0) + t_{s,0}), & t \in [t(0), t(1)) \\\
    C_1''(t - t(1) + t_{s,1}), & t \in [t(1), t(2)) \\\
    \vdots & \vdots \\\
    C_{n-1}''(t - t(n-1) + t_{s,n-1}), & t \in [t(n-1), t(n)]
\end{cases}$$

曲線 $C(t)$ および導関数 $C'(t), C''(t)$ から計算される各種特徴量については、[Geometric Properties (曲線の幾何学的性質)](./../geometric_properties_ja.md) を参照してください。
