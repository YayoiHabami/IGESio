# Tabulated Cylinder (Type 122)

Defined at [surfaces/tabulated_cylinder.h](./../../../include/igesio/entities/surfaces/tabulated_cylinder.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
  - [曲面の定義](#曲面の定義)
    - [曲面 $S(u, v)$](#曲面-su-v)
    - [偏導関数 $S\_u, S\_v, S\_{uu}, S\_{uv}, S\_{vv}$](#偏導関数-s_u-s_v-s_uu-s_uv-s_vv)
- [Appendix](#appendix)
  - [曲面の式の導出](#曲面の式の導出)
  - [偏導関数の導出](#偏導関数の導出)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetDirectrix()` <br> `SetDirectrix(curve)` <br> (`std::shared_ptr<const ICurve>`) | 準線曲線 $C(t)$ へのポインタ |
| `GetDirection()` <br> `SetDirection(vec, length)` <br> (`Vector3d`) | 生成方向ベクトル $D$ |
| `GetLocationVector()` <br> `SetLocationVector(vec)` <br> (`Vector3d`) | 位置ベクトル $P_0$ |

### 曲面の定義

#### 曲面 $S(u, v)$

　Tabulated Cylinder (Type 122) エンティティは、準線曲線 $C(t)\ (t \in [t_{\text{min}}, t_{\text{max}}])$ を生成方向ベクトル $D$ に沿って平行移動させることで定義される曲面であり、そのパラメトリック曲面 $S(u, v)$ は次のように定義されます。

$$S(u, v) = C(t) + v D, \quad u \in [0, 1], \quad v \in [0, 1]$$

ここで、準線曲線 $C(t)$ の媒介変数 $t$ は、次のように $u$ に対応付けられます。

$$t = t_{\text{min}} + u (t_{\text{max}} - t_{\text{min}})$$

#### 偏導関数 $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$

　Tabulated Cylinderの偏導関数 $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$ は以下のように定義されます。詳細な導出については、[Appendix-偏導関数の導出](#偏導関数の導出)を参照してください。

**一階偏導関数：**

$$\begin{aligned}
    S_u(u, v) &= C'(t) (\Delta t) \\\
    S_v(u, v) &= D
\end{aligned}$$

**二階偏導関数：**

$$\begin{aligned}
    S_{uu}(u, v) &= C''(t) (\Delta t)^2 \\\
    S_{uv}(u, v) &= 0 \\\
    S_{vv}(u, v) &= 0
\end{aligned}$$

## Appendix

### 曲面の式の導出

　Tabulated Cylinder (Type 122) エンティティの曲面の式は、準線曲線 $C(t)$ を生成方向ベクトル $D$ に沿って平行移動させることで導出されます。ここで、生成方向ベクトル $D$ は以下の2つの方法で定義可能です。

(1) 位置ベクトル $P_0$ と曲線の始点 $C(t_{\text{min}})$ を結ぶベクトルとして定義する方法：

　IGES5.3で標準の方法として規定されており、IGESファイルに保存されるのも曲線 $C(t)$ へのポインタと位置ベクトル $P_0$ のみです。`SetLocationVector(vec)`を使用して設定可能であり、生成方向ベクトル $D$ は以下のように計算されます。

$$D = P_0 - C(t_{\text{min}})$$

(2) 直接生成方向ベクトル $D$ を指定する方法：

　本ライブラリでは、`SetDirection(vec, length)`関数を使用して、直接生成方向ベクトル $D$ を指定することも可能です。この場合、`length`引数は生成方向ベクトルの大きさを指定し、`vec`引数はその方向を示します。この際、`vec`は正規化されず、`length * vec`が生成方向ベクトル $D$ となります。

　この2つの方法により定義された生成方向ベクトル $D$ を用いて、Tabulated Cylinderの曲面 $S(u, v)$ は以下のように表されます。

$$S(u, v) = C(t) + v D, \quad u \in [0, 1], \quad v \in [0, 1]$$

ここで、準線曲線 $C(t)$ の媒介変数 $t$ は、次のように $u$ に対応付けられます。

$$t = t_{\text{min}} + u (t_{\text{max}} - t_{\text{min}})$$

### 偏導関数の導出

　偏導関数の導出の前に、まず以下の記号を定義します。

$$\Delta t = t_{\text{max}} - t_{\text{min}}$$

　この記号を用いて、Tabulated Cylinderの曲面 $S(u, v)$ の偏導関数は以下のように導出されます。

**一階偏導関数：**

$$\begin{aligned}
    S_u(u, v) &= \frac{\partial}{\partial u} (C(t) + v D) \\\
    &= \frac{dC}{dt} \frac{dt}{du} + 0 \\\
    &= C'(t) (\Delta t) \\\
    &\quad \\\
    S_v(u, v) &= \frac{\partial}{\partial v} (C(t) + v D) \\\
    &= D
\end{aligned}$$

**二階偏導関数：**

$$\begin{aligned}
    S_{uu}(u, v) &= \frac{\partial}{\partial u} S_u(u, v) \\\
    &= C''(t) (\Delta t)^2 \\\
    &\quad \\\
    S_{uv}(u, v) &= \frac{\partial}{\partial v} S_u(u, v) \\\
    &= 0 \\\
    &\quad \\\
    S_{vv}(u, v) &= \frac{\partial}{\partial v} S_v(u, v) \\\
    &= 0
\end{aligned}$$

　曲面 $S(u,v)$ およびその偏導関数から計算可能な曲面の幾何学的特性については、[geometric_properties_ja.md](./../geometric_properties_ja.md#曲面の幾何学的特性) を参照してください。

**n階偏導関数：**

　パラメトリック曲面 $S(u,v)$ を $u, v$ に関して $n_u, n_v$ 回微分した偏導関数は、以下のように表されます。

$$S^{(n_u, n_v)}(u, v) = \begin{cases}
    C(t) + v D, & n_u = 0, n_v = 0 \\\
    C^{(n_u)}(t) (\Delta t)^{n_u}, & n_v = 0 \\\
    D, & n_u = 0, n_v = 1 \\\
    0, & \text{otherwise}
\end{cases}$$

