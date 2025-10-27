# Ruled Surface (Type 118)

Defined at [surfaces/ruled_surface.h](./../../../include/igesio/entities/surfaces/ruled_surface.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
  - [曲面の定義](#曲面の定義)
    - [曲面 $S(u, v)$](#曲面-su-v)
    - [偏導関数 $S\_u, S\_v, S\_{uu}, S\_{uv}, S\_{vv}$](#偏導関数-s_u-s_v-s_uu-s_uv-s_vv)
- [Appendix](#appendix)
  - [偏導関数の導出](#偏導関数の導出)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetCurve1()` <br> `SetCurve1(curve)` <br> (`std::shared_ptr<const ICurve>`) | 曲線1 $C_1(t)$ へのポインタ |
| `GetCurve2()` <br> `SetCurve2(curve)` <br> (`std::shared_ptr<const ICurve>`) | 曲線2 $C_2(s)$ へのポインタ |
| `IsReversed()` <br> `SetReversed(reversed)` <br> (`bool`) | 2つの曲線のパラメータ範囲を反転させるか $\text{DIR-FLAG}$ |
| `IsDevelopable()` <br> `SetDevelopable(developable)` <br> (`bool`) | 可展面かどうか $\text{DEV-FLAG}$ |

> $\text{DEV-FLAG}$については、現在の実装では無視されます。IGESファイルから読み込んだ場合はその設定がそのまま反映され、新たに作成する場合は常に`false`となります。

### 曲面の定義

#### 曲面 $S(u, v)$

　ルールド面は2つの曲線 $C_1(t)\ (t \in [t_{\text{min}}, t_{\text{max}}])$ と $C_2(s)\ (s \in [s_{\text{min}}, s_{\text{max}}])$ を直線で結ぶことで定義される曲面であり、そのパラメトリック曲面 $S(u, v)$ は次のように定義されます。

$$S(u, v) = (1 - v) C_1(t) + v C_2(s), \quad u,v \in [0, 1]$$

ここで, $C_1(t)$ の媒介変数 $t$ と $C_2(s)$ の媒介変数 $s$ は、次のように $u$ に対応付けられます。

$$\begin{aligned}
    t &= t_{\text{min}} + u (t_{\text{max}} - t_{\text{min}}) \\\
    s &= \begin{cases}
        s_{\text{min}} + u (s_{\text{max}} - s_{\text{min}}), & \text{if } \text{DIR-FLAG} = false \\\
        s_{\text{max}} - u (s_{\text{max}} - s_{\text{min}}), & \text{if } \text{DIR-FLAG} = true
    \end{cases}
\end{aligned}$$

> IGES5.3では、このパラメータ範囲の変換の際に、元の曲線のパラメータ範囲 $t_{\text{min}}, t_{\text{max}}, s_{\text{min}}, s_{\text{max}}$ を使用する方法 (form=1) と、長さに基づいて新たに計算されたパラメータ範囲を使用する方法 (form=0) の2通りが定義されています。現在、form 0の方法は未実装です。

#### 偏導関数 $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$

　ルールド面の偏導関数 $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$ は以下のように定義されます。詳細な導出については、[Appendix-偏導関数の導出](#偏導関数の導出)を参照してください。

**一階偏導関数：**

$$\begin{aligned}
    S_u(u, v) &= (1 - v) C_1'(t) (\Delta t) + v C_2'(s) \sigma (\Delta s) \\\
    S_v(u, v) &= -C_1(t) + C_2(s)
\end{aligned}$$

**二階偏導関数：**

$$\begin{aligned}
    S_{uu}(u, v) &= (1 - v) C_1''(t) (\Delta t)^2 + v C_2''(s) (\Delta s)^2 \\\
    S_{uv}(u, v) &= -C_1'(t) (\Delta t) + C_2'(s) \sigma (\Delta s) \\\
    S_{vv}(u, v) &= 0
\end{aligned}$$

ここで, $\Delta t, \Delta s$ および $\sigma$ は次のように定義されます。

$$\Delta t = t_{\text{max}} - t_{\text{min}}, \quad \Delta s = s_{\text{max}} - s_{\text{min}}$$

$$\sigma = \begin{cases}
    1, & \text{if } \text{DIR-FLAG} = false \\\
   -1, & \text{if } \text{DIR-FLAG} = true
\end{cases}$$

　曲面 $S(u,v)$ およびその偏導関数から計算可能な曲面の幾何学的特性については、[geometric_properties_ja.md](./../geometric_properties_ja.md#曲面の幾何学的特性) を参照してください。

## Appendix

### 偏導関数の導出

　ルールド面の偏導関数を定義する前に、まず以下の定数を定義します。

$$\Delta t = t_{\text{max}} - t_{\text{min}}, \quad \Delta s = s_{\text{max}} - s_{\text{min}}$$

$$\sigma = \begin{cases}
    1, & \text{if } \text{DIR-FLAG} = false \\\
   -1, & \text{if } \text{DIR-FLAG} = true
\end{cases}$$

　これらを用いると, $u$ の変数 $t =: t(u), s =: s(u)$ に関する導関数は次のように表されます。

$$t(u) = t_{\text{min}} + u \Delta t, \Longrightarrow \frac{dt}{du} = \Delta t, \quad \frac{d^2 t}{du^2} = 0$$

$$s(u) = s_{\text{min}} + \sigma u \Delta s, \Longrightarrow \frac{ds}{du} = \sigma \Delta s, \quad \frac{d^2 s}{du^2} = 0$$

　また、曲線 $C_1(t), C_2(s)$ の $t, s$ に関する導関数をそれぞれ $C_1'(t), C_1''(t), C_2'(s), C_2''(s)$ と表すことにします。このとき、ルールド面 $S(u, v)$ の偏導関数は次のように定義されます。

**一階偏導関数：**

$$\begin{aligned}
    S_u(u, v) &= \frac{\partial}{\partial u} \left((1 - v) C_1(t(u)) + v C_2(s(u))\right) \\\
    &= (1 - v) \frac{d C_1}{d t} \frac{d t}{d u} + v \frac{d C_2}{d s} \frac{d s}{d u} \\\
    &= (1 - v) C_1'(t) \Delta t + v C_2'(s) \sigma \Delta s \\\
    &\quad \\\
    S_v(u, v) &= \frac{\partial}{\partial v} \left((1 - v) C_1(t(u)) + v C_2(s(u))\right) \\\
    &= -C_1(t) + C_2(s)
\end{aligned}$$

**二階偏導関数：**

$$\begin{aligned}
    S_{uu}(u, v) &= \frac{\partial}{\partial u} \left((1 - v) C_1'(t) \Delta t + v C_2'(s) \sigma \Delta s\right) \\\
    &= (1 - v) \frac{d C_1'}{d t} \frac{d t}{d u} \Delta t + v \frac{d C_2'}{d s} \frac{d s}{d u} \sigma \Delta s \\\
    &= (1 - v) C_1''(t) (\Delta t)^2 + v C_2''(s) (\Delta s)^2 \\\
    &\quad \\\
    S_{uv}(u, v) &= \frac{\partial}{\partial v} \left((1 - v) C_1'(t) \Delta t + v C_2'(s) \sigma \Delta s\right) \\\
    &= -C_1'(t) \Delta t + C_2'(s) \sigma \Delta s \\\
    &\quad \\\
    S_{vv}(u, v) &= \frac{\partial}{\partial v} \left(-C_1(t) + C_2(s)\right) \\\
    &= 0
\end{aligned}$$

**n階偏導関数：**

　上の記述を一般化すると、ルールド面 $S(u, v)$ の $(n_u, n_v)$ 階偏導関数 $S^{(u^{n_u}, v^{n_v})}(u, v)$ は以下のように表されます。

$$S^{(u^{n_u}, v^{n_v})}(u, v) = \begin{cases}
    (1 - v) C_1^{(n_u)}(t) (\Delta t)^{n_u} + v C_2^{(n_u)}(s) (\sigma \Delta s)^{n_u}, & n_v = 0 \\\
    -C_1^{(n_u)}(t) (\Delta t)^{n_u} + C_2^{(n_u)}(s) (\sigma \Delta s)^{n_u}, & n_v = 1 \\\
    0, & n_v \geq 2
\end{cases}$$
