# Composite Curve (Type 102)

Defined at [curves/composite_curve.h](./../../../include/igesio/entities/curves/composite_curve.h)

## Contents

- [Contents](#contents)

## Parameters

### Specific Parameters

| Member Function | Description |
|---|---|
| `GetCurveCount()` <br> (`int`) | Number of constituent curves, $n$ |
| `GetCurveAt(index)` <br> (`std::shared_ptr<const ICurve>`) | The `index`-th constituent curve, $C_i$ |

### Curve Definition

#### Curve $C(t)$

A Composite Curve entity forms a single curve by concatenating multiple curve entities. Any curve entity that implements the `ICurve` interface can be set as each constituent curve. Let $C_i(t_i)\  (t_i \in [t_{s,i}, t_{e,i}])$ be the $i$-th ($i = 0, 1, \ldots, n-1$) constituent curve, where $t_i$ is the parameter of $C_i$, and $t_{s,i}$ and $t_{e,i}$ are the start and end parameters of $C_i$, respectively. Then, the parametric curve $C(u)$ of the entire Composite Curve entity is defined as follows:

$$C(t) = \begin{cases}
    C_0(t - t(0) + t_{s,0}), & t \in [t(0), t(1)) \\\
    C_1(t - t(1) + t_{s,1}), & t \in [t(1), t(2)) \\\
    \vdots & \vdots \\\
    C_{n-1}(t - t(n-1) + t_{s,n-1}), & t \in [t(n-1), t(n)]
\end{cases}$$

where $t(i)$ is the cumulative parameter with respect to the start parameter of the $i$-th curve, and is defined as follows:

$$t(i) = \sum_{k=0}^{i-1} (t_{e,k} - t_{s,k}), \quad \text{for } i = 0, 1, \ldots, n$$

For example, if $n=3$, and the parameter range of $C_0$ is $t_0 \in [1, 2]$, the parameter range of $C_1$ is $t_1 \in [3.5, 7]$, and the parameter range of $C_2$ is $t_2 \in [-2, 0]$, then the cumulative parameter $t(i)$ is calculated as follows:

| - | $t_{s,i}$ | $t_{e,i}$ | $t_{e,i} - t_{s,i}$ | $t(i)$ |
|:-:|:-:|:-:|:-:|:-:|
| $i=0$ | 1   | 2 | 1   | **0**   |
| $i=1$ | 3.5 | 7 | 3.5 | **1**   |
| $i=2$ | -2  | 0 | 2   | **4.5** |
| $i=3$ | -   | - | -   | **6.5** |

Also, adjacent curves $C_i$ and $C_{i+1}$ must satisfy the following continuity condition (derivative continuity is not required):

$$C_i(t_{e,i}) = C_{i+1}(t_{s,i+1}), \quad \text{for } i = 0, 1, \ldots, n-2$$

#### Derivatives $C'(t), C''(t)$

The derivatives of the Composite Curve entity are defined using the derivatives of each constituent curve as follows:

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

For various features calculated from the curve $C(t)$ and the derivatives $C'(t), C''(t)$, please refer to [Geometric Properties](./../geometric_properties.md).
