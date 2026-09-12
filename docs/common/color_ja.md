# Color

Defined in [`igesio/common/color.h`](./../../include/igesio/common/color.h)

## 目次

- [目次](#目次)
- [概要](#概要)
- [生成](#生成)
- [変換](#変換)
- [派生・判定](#派生判定)
- [α成分の扱い](#α成分の扱い)
- [IGESスケールとの関係](#igesスケールとの関係)

## 概要

　`igesio::Color`は、本ライブラリ全体で色を扱うための値型です。Directory Entryから解決したエンティティの色（`EntityBase::GetColor()`）やColor Definitionエンティティ（Type 314）の定義色、`Assembly`ノードの色オーバーライド、描画の各色（メイン色・ハイライト色・背景色・光源色）など、色を受け渡す箇所ではすべてこの型を使います。

　メンバ変数の`r`/`g`/`b`/`a`はいずれも $[0, 1]$ で、`double`型として保持します。他の色表現（IGES形式の0〜100、8bitの0〜255、描画用の`float`配列、16進カラーコード等）との変換はメンバ関数として提供するため、利用側で変換処理を書くことはありません。

| メンバ | 説明 |
|:---|:---|
| `r`, `g`, `b` | 赤、緑、青成分 ($[0, 1]$) |
| `a` | 不透明度 ($[0, 1]$; 1.0が不透明) |

　`Color`は集成体です。以下の形式で初期化でき、定数式でも使用可能です。

```cpp
constexpr igesio::Color kOrange{1.0, 0.6, 0.0};        // 省略時はa = 1.0
constexpr igesio::Color kHalfRed{1.0, 0.0, 0.0, 0.5};
igesio::Color black{};                                 // (0, 0, 0, 1)
```

　構築時に各成分の値の範囲検証は行いません。不正な値が格納されていないかは`IsInUnitRange()`で判定し、有効な色を要求するAPI（`MakeColorDefinition`など）などではそれぞれの側で検査します。

## 生成

| 関数 | 説明 |
|:---|:---|
| `Color::FromRGB255(r, g, b, a = 255)` | 8bit成分（0〜255）から生成する。`constexpr`. <br> 検証は行わず、範囲外の入力は単位範囲外の色になる |
| `Color::FromIgesRGB(r, g, b)` <br> `Color::FromIgesRGB(std::array<double, 3>)` | IGES形式の色表現（RGBのみ、0.0〜100.0）から生成する。`a = 1.0`,`constexpr` |
| `Color::FromFloatRGBA(std::array<float, 4>)` | `float`のRGBA配列 ($[0, 1]$) から生成する。`constexpr` |
| `Color::FromFloatRGB(std::array<float, 3>, a = 1.0)` | `float`のRGB配列 ($[0, 1]$) から生成する。`constexpr` |
| `Color::TryParseHex(text)` | `"#RRGGBB"`・`"RRGGBB"`・`"#RRGGBBAA"`・`"RRGGBBAA"`（大文字/小文字は区別しない）から生成する。形式が不正なら`std::nullopt` |
| `Color::FromHex(text)` | `TryParseHex`と同じ形式から生成し、形式が不正なら`std::invalid_argument`を投げる |

```cpp
auto c1 = igesio::Color::FromRGB255(127, 255, 76);   // (0.498, 1.0, 0.298, 1.0)
auto c2 = igesio::Color::FromIgesRGB(50.0, 100.0, 30.0);
auto c3 = igesio::Color::FromHex("#7fff4c");
if (auto c4 = igesio::Color::TryParseHex("#7FFF4C80")) {
    // c4->a == 128 / 255.0
}
```

## 変換

| 関数 | 説明 |
|:---|:---|
| `ToFloatRGBA()` | `std::array<float, 4>` ($[0, 1]$). GPUのuniform値やImGuiのウィジェット等描画に渡す際に使用する。`constexpr` |
| `ToFloatRGB()` | `std::array<float, 3>` ($[0, 1]$). 同上。α成分は捨てる。`constexpr` |
| `ToIgesRGB()` | IGES形式（0.0〜100.0）の`std::array<double, 3>`。α成分は捨てる。`constexpr` |
| `ToRGB255()` | `std::array<int, 3>`（0〜255） |
| `ToHex(with_alpha = false)` | `"#RRGGBB"`（大文字）。`with_alpha = true`なら`"#RRGGBBAA"` |

　`FromRGB255` → `ToRGB255`は0〜255の全値で可逆であり、`float` → `Color` → `float`での変換も可逆です。

## 派生・判定

| 関数 | 説明 |
|:---|:---|
| `WithAlpha(alpha)` | 不透明度を差し替えたコピーを返す。`constexpr` |
| `IsInUnitRange()` | `a`を含む全成分の値が $[0, 1]$ の範囲内であれば`true`。範囲外またはNaNを含む場合は`false`。`constexpr` |
| `Clamped()` | 全成分を $[0, 1]$ に正規化したコピーを返す。`constexpr` |
| `SquaredDistanceRGB(other)` | RGB空間のユークリッド距離の二乗（αは無視）。最近接標準色の探索に使う。`constexpr` |
| `operator[](index)` | 添字による成分の取得（0: r, 1: g, 2: b, 3: a）。`index > 3`なら`std::out_of_range`。`constexpr` |
| `operator==` / `operator!=` | 4成分すべての厳密比較。近似比較は用意していないため、必要な場合は成分ごとに許容誤差付きで比較する |

## α成分の扱い

| 受け取る側 | α成分 |
|:---|:---|
| IGESの色形式（`EntityBase::GetColor()`・`DEColor::GetRGB()`・`IColorDefinition::GetRGB()`） | 常に`1.0`（IGESの色は不透明度を持たない） |
| `MakeColorDefinition(color)` / `ColorDefinition::SetRGB(color)` | 無視する。Type 314にはRGBのみを格納する |
| `DisplayState::color_override`（`Assembly::SetColorOverride`） | 無視する。不透明度は`SetOpacityOverride`で別途オーバーライドする |
| `EntityRenderer::SetAmbientColor` | 無視する |
| `IEntityGraphics::SetColor`, `DrawContext::highlight_color`, `EntityRenderer::SetBackgroundColor`, `Light::color` | OpenGL側に送るRGBA値として使用する |

## IGESスケールとの関係

　IGESはColor Definitionエンティティ（Type 314）のRGB成分を0.0〜100.0の値として保持します。（無変更で書き戻したときにParameter Dataが同一になるようにするため）`ColorDefinition`はファイルから読み込んだ値をそのまま保持し、`GetRGB()` / `GetColor()`が返す`Color`はその都度再計算します。一方、`MakeColorDefinition`/`SetRGB`は`Color`を受け取り、`ToIgesRGB()`で変換した値を格納します。

　Directory Entryの色番号（`ColorNumber`; 1〜8）の標準色は`igesio::entities::ToColor(ColorNumber)`で`Color`として取得でき、`Color`に最も近い標準色は`igesio::entities::ClosestColorNumber(color)`で求めることが可能です（いずれも[`igesio/entities/de/de_color.h`](./../../include/igesio/entities/de/de_color.h)）。
