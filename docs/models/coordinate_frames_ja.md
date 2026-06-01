# 座標フレームと変換ビュー

## 目次

- [目次](#目次)
- [概要](#概要)
- [変換鎖とフレーム](#変換鎖とフレーム)
- [エンティティ層の配置適用API](#エンティティ層の配置適用api)
- [Assembly層のフレーム指定API](#assembly層のフレーム指定api)
- [変換ビュー（CurveView / SurfaceView）](#変換ビューcurveview--surfaceview)
- [配置の永続化（Flatten / Materialize）](#配置の永続化flatten--materialize)
- [利用例（工具接触点列の生成）](#利用例工具接触点列の生成)

## 概要

IGESioの主要用途の一つはCAMであり、曲線・曲面を単体で扱うこと（u固定でvを掃引して工具接触点列を生成する、面を離散化するなど）が多い。このため、エンティティを[アセンブリ階層](./assembly_ja.md)に載せた状態でも、単体でも、同じ感覚で幾何問い合わせができることを重視している。

この目的のため、次の三つの仕組みを用意している。

- エンティティ層の配置適用API: `ICurve` / `ISurface`の問い合わせ関数に、配置行列を後掛けする引数を追加する。
- Assembly層のフレーム指定API: 「どの座標系で取得したいか」を`CoordFrame`で指定し、`Assembly`が配置行列を解決する。
- 変換ビュー: 配置を畳み込んだ`ICurve` / `ISurface`を、元エンティティを書き換えずに生成する。

## 変換鎖とフレーム

エンティティの定義空間からワールドまでの変換鎖は次の通りである。ここで $A_n$ は直近の所有Assembly、 $A_\text{root}$ は最上位を表す。

$$ P_\text{world} = G_\text{root} \cdots G_{k+1} \cdots G_n \cdot M_\text{entity} \cdot P_\text{def} $$

- $P_\text{def}$ : 定義空間の値
- $M_\text{entity}$ : エンティティ自身のDE変換（124）
- $G_i$ : 経路上の`Assembly`ノード $A_i$ の大域変換

取得したいフレームは、適用する変換が一列に並ぶ形になる。

| フレーム | 適用する変換 | 意味 |
| --- | --- | --- |
| 定義空間 | なし | $P_\text{def}$ そのもの |
| エンティティ自身 | $M_\text{entity}$ | 自分のDE変換のみ。Assembly非依存 |
| 祖先 $A_k$ の定義空間 | $M_\text{entity} \cdot G_n \cdots G_{k+1}$ | $A_k$ 配下の座標（ $A_k$ 自身の $G_k$ は含めない） |
| ワールド/モデル空間 | 全鎖 | 最終配置 |

「祖先 $A_k$ の定義空間」は $A_k$ 自身の大域変換 $G_k$ を含まないことに注意する。 $A_k$ を基準としたとき、掛けるのは $A_k$ 配下へ配置するための変換のみであり、 $A_k$ を親フレームへ置く $G_k$ は含めない。ワールドが欲しい場合は基準をルートとし、ルートまでの全ての $G$ を含める。

## エンティティ層の配置適用API

`ICurve` / `ISurface`の問い合わせ関数は、定義空間系統と配置適用系統の二系統で構成される。命名は次の方針で固定し、フレームの違いは引数で表す。

- `TryGetDefinedXxx()`: 定義空間（ $P_\text{def}$ ）を返す。
- `TryGetXxx()`: $M_\text{entity}$ を適用した実空間の値を返す。
- `TryGetXxx(..., placement)`: $M_\text{entity}$ 適用後の値に、さらに配置行列`placement`を後掛けする。戻り値は $\text{placement} \cdot (M_\text{entity} \cdot P_\text{def})$ となる。

`placement`の既定は単位行列であり、その場合は従来挙動（ $M_\text{entity}$ のみ適用）と一致する。点とベクトルでは変換の扱いを分ける。位置（点）は $R \cdot p + T$ 、各階微分（ベクトル）は $R \cdot v$ として変換する。スケールなし（124相当）を前提とする。

```cpp
// 定義空間
auto p_def = curve.TryGetDefinedPointAt(t);
// M_entity 適用後（実空間）
auto p_local = curve.TryGetPointAt(t);
// 配置を後掛け（placement·M_entity·P_def）
auto p_placed = curve.TryGetPointAt(t, placement);
```

導関数についても、定義空間版`TryGetDerivatives(t, n)`と配置適用版`TryGetDerivatives(t, n, placement)`を持つ。定義空間版は定義空間の導関数を返し、配置適用版は0階を点、1階以上をベクトルとして変換した値を返す。

## Assembly層のフレーム指定API

エンティティ単体では「どの階層を基準にするか」を行列で指定する必要があるが、多段アセンブリではその行列の取り違え（ $G_k$ を含む/含まない）が起きやすい。そこで`Assembly`層では、フレームを値で明示する`CoordFrame`を導入し、`Assembly`が配置行列を解決する。

`CoordFrame`（`include/igesio/models/assembly.h`）は静的生成関数で構築する。

| 生成関数 | フレーム | 種類（`Kind`） |
| --- | --- | --- |
| `CoordFrame::Definition()` | 定義空間 | `kDefinition` |
| `CoordFrame::EntityLocal()` | エンティティ自身（ $M_\text{entity}$ のみ） | `kEntityLocal` |
| `CoordFrame::World()` | ワールド/モデル空間 | `kWorld` |
| `CoordFrame::RelativeTo(base_id)` | 指定Assemblyの定義空間（その $G_k$ は含めない） | `kRelative` |

基準Assemblyは生ポインタではなく`ObjectID`で保持する（ダングリング回避）。`Assembly`は所有Assemblyから親リンクを上へ辿り、ID等価比較で経路の $G$ の積を解決する。

`ResolvePlacement(id, frame)`は、当該フレームで$M_\text{entity}$適用後の値に後掛けすべき配置行列を返す。idがツリーに無い場合、または`frame`が`kDefinition`（配置概念がない）の場合は`std::nullopt`を返す。`RelativeTo`の基準が所有Assemblyの祖先でない場合は`std::invalid_argument`を送出する。

```cpp
// ワールド配置を解決し、エンティティ問い合わせへ渡す
auto placement = root.ResolvePlacement(id, igesio::models::CoordFrame::World());
if (placement) {
    auto p_world = curve.TryGetPointAt(t, *placement);
}
```

## 変換ビュー（CurveView / SurfaceView）

同一エンティティを複数階層から同時に別フレームで取得しうるため、元エンティティを書き換えずに配置を表現する必要がある。そこで、`ICurve` / `ISurface`のみを継承する変換ビュー（Decoratorパターン）を導入している（`include/igesio/entities/views/`）。

`CurveView` / `SurfaceView`の性質は次の通りである。

- 非破壊: 元エンティティを共有（`std::shared_ptr<const ICurve>`等）して参照のみ保持し、変更しない。複数フレームからの同時取得が安全。
- 型爆発なし: `ICurve` / `ISurface`にのみ依存するため、2クラスで全曲線・全曲面型に対応する。
- 置換可能: 同じインターフェースのため、`const ICurve&` / `const ISurface&`を受ける既存の交差・サンプリング・離散化アルゴリズムへそのまま渡せる。
- 読み取り専用・スナップショット意味論: 生成時点の累積変換`placement_`を固定して持つ。後でAssembly変換を変えても追従しない（必要なら作り直す）。形状編集APIは持たない。
- 固有IDと委譲: ビュー固有の`ObjectID`（`kEntityView`）を持つが、型・フォーム番号は元エンティティへ委譲する。元エンティティのIDは`GetSourceID()`で取得できる（通常エンティティは`GetID()`と同じ値を返す）。`GetSourceID()`はピッキング結果（ビュー）から元エンティティを特定し、選択を集約するのに用いる。

ビューにおける「定義空間」は、元エンティティの $M_\text{entity}$ を適用した後の座標（所有Assemblyのローカルフレーム）を指す。`TryGet*`はこれに`placement_`を後掛けし、 $\text{placement\_} \cdot (M_\text{entity} \cdot P_\text{def})$ を返す。ビューのビューを生成した場合は、行列を畳み込んで元の`ICurve` / `ISurface`へ繋ぎ直す。

ビューは`Assembly`のファクトリ関数で生成する。

| メンバ関数 | 内容 |
| --- | --- |
| `GetCurveView(id, frame)` | 指定フレームの`CurveView`を生成（曲線でない場合は`nullptr`） |
| `GetSurfaceView(id, frame)` | 指定フレームの`SurfaceView`を生成（曲面でない場合は`nullptr`） |

`frame`に`kDefinition`を指定した場合は`std::invalid_argument`を送出する（ビューは $M_\text{entity}$ 適用を前提とするため）。

## 配置の永続化（Flatten / Materialize）

`Assembly`・変換ビューはIGESファイルへ出力しない（非永続）。アセンブリの配置をファイルへ永続化したい場合は、大域変換をエンティティのDE変換（124）へ畳み込む。これは`include/igesio/models/flatten.h`が提供する。

- `Materialize(entity, placement)`: エンティティを複製し、`placement`をDE変換（124）へ畳み込む。`placement`が単位行列なら元の124を共有し（新124なし）、非単位なら新124 $= \text{placement} \cdot M_\text{entity}$ を生成して複製の変換を差し替える。複製は`entities::CloneEntity`による浅い複製（参照は元と共有）であり、新しいIDを採番する。
- `Flatten(src)`: `Assembly`階層の配置を畳み込み、子`Assembly`を持たないフラットな`IgesData`を返す。独立（非物理従属）エンティティのみを所有Assemblyのワールド配置でMaterializeし、物理従属の子は親の畳み込んだ124を介して追従する。共有124はMaterializeで個別化されるため、DAG構造がツリーへ自然に解消する。

いずれもスケールなし（124相当）を前提とする。Materializeで実体化したエンティティは通常エンティティとなり、writerのID→DEポインタ割当に普通に乗る。

## 利用例（工具接触点列の生成）

変換ビューは「生成1回・サンプル多数」の用途に好適である。ワールド配置のビューを一度生成すれば、以後は既存のサンプラへ`const ISurface&`としてそのまま渡せる。

```cpp
// 面をワールド配置の SurfaceView として取得する
auto frame = igesio::models::CoordFrame::World();
auto surface = root.GetSurfaceView(surface_id, frame);
if (!surface) return;

// u を固定し、v を掃引して接触点列を得る（*surface は const ISurface&）
const double u = 0.5;
for (int i = 0; i <= n; ++i) {
    const double v = static_cast<double>(i) / n;
    if (auto p = surface->TryGetPointAt(u, v)) {
        // *p はワールド座標
    }
}
```

ビューはスナップショットのため、生成後にAssembly変換を変更しても追従しない。配置を変えた場合は作り直す。
