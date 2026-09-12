# Color

Defined in [`igesio/common/color.h`](./../../include/igesio/common/color.h)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Overview](#overview)
- [Construction](#construction)
- [Conversion](#conversion)
- [Derivation and Queries](#derivation-and-queries)
- [Handling of the Alpha Component](#handling-of-the-alpha-component)
- [Relationship with the IGES Scale](#relationship-with-the-iges-scale)

## Overview

`igesio::Color` is the value type that represents an RGBA color throughout the library. It is used wherever a color is passed around: the color of an entity resolved from the Directory Entry (`EntityBase::GetColor()`), the defined color of a Color Definition entity (Type 314), the color override of an `Assembly` node, and the rendering colors (main color, highlight color, background color, light color).

The canonical scale is $[0, 1]$ for every component, held as four `double` members. Conversions to and from other scales (IGES 0–100, 8-bit 0–255, `float` arrays for GPU uniforms, hexadecimal color codes) are provided as member functions, so the calling code never writes a scale factor itself.

| Member | Meaning |
|:---|:---|
| `r`, `g`, `b` | Red, green, and blue components ( $[0, 1]$ ) |
| `a` | Opacity ( $[0, 1]$; 1.0 is fully opaque) |

`Color` is an aggregate. It can be brace-initialized and used in constant expressions:

```cpp
constexpr igesio::Color kOrange{1.0, 0.6, 0.0};        // a = 1.0 when omitted
constexpr igesio::Color kHalfRed{1.0, 0.0, 0.0, 0.5};
igesio::Color black{};                                 // (0, 0, 0, 1)
```

No range validation is performed on construction. Whether a color lies within the unit range is queried with `IsInUnitRange()`; the APIs that require a valid color (for example `MakeColorDefinition`) perform the check themselves.

## Construction

| Function | Description |
|:---|:---|
| `Color::FromRGB255(r, g, b, a = 255)` | From 8-bit components (0–255). `constexpr`. Not validated: out-of-range input yields a color outside the unit range |
| `Color::FromIgesRGB(r, g, b)` <br> `Color::FromIgesRGB(std::array<double, 3>)` | From RGB on the IGES scale (0.0–100.0). `a = 1.0`. `constexpr` |
| `Color::FromFloatRGBA(std::array<float, 4>)` | From a `float` RGBA array ( $[0, 1]$ ). `constexpr` |
| `Color::FromFloatRGB(std::array<float, 3>, a = 1.0)` | From a `float` RGB array ( $[0, 1]$ ). `constexpr` |
| `Color::TryParseHex(text)` | Parses `"#RRGGBB"`, `"RRGGBB"`, `"#RRGGBBAA"`, or `"RRGGBBAA"` (case-insensitive). Returns `std::nullopt` when the text is malformed |
| `Color::FromHex(text)` | Same as `TryParseHex`, but throws `std::invalid_argument` when the text is malformed |

```cpp
auto c1 = igesio::Color::FromRGB255(127, 255, 76);   // (0.498, 1.0, 0.298, 1.0)
auto c2 = igesio::Color::FromIgesRGB(50.0, 100.0, 30.0);
auto c3 = igesio::Color::FromHex("#7fff4c");
if (auto c4 = igesio::Color::TryParseHex("#7FFF4C80")) {
    // c4->a == 128 / 255.0
}
```

## Conversion

| Function | Description |
|:---|:---|
| `ToFloatRGBA()` | `std::array<float, 4>` ( $[0, 1]$ ). Used for GPU uniforms and ImGui widgets. `constexpr` |
| `ToFloatRGB()` | `std::array<float, 3>` ( $[0, 1]$ ). The alpha component is dropped. `constexpr` |
| `ToIgesRGB()` | `std::array<double, 3>` on the IGES scale (0.0–100.0). The alpha component is dropped. `constexpr` |
| `ToRGB255()` | `std::array<int, 3>` (0–255). Each component is clamped to $[0, 1]$ and then rounded half up |
| `ToHex(with_alpha = false)` | `"#RRGGBB"` (uppercase). With `with_alpha = true`, `"#RRGGBBAA"`. Uses the same clamping and rounding as `ToRGB255` |

`FromRGB255` → `ToRGB255` is lossless for every value in 0–255, and `float` → `Color` → `float` is lossless.

## Derivation and Queries

| Function | Description |
|:---|:---|
| `WithAlpha(alpha)` | Returns a copy with the opacity replaced. `constexpr` |
| `IsInUnitRange()` | `true` when every component (including `a`) lies within $[0, 1]$. `false` when any component is NaN. `constexpr` |
| `Clamped()` | Returns a copy with every component clamped to $[0, 1]$. `constexpr` |
| `SquaredDistanceRGB(other)` | Squared Euclidean distance in RGB space (alpha is ignored). Used to find the closest standard color. `constexpr` |
| `operator[](index)` | Component access by index (0: r, 1: g, 2: b, 3: a). Throws `std::out_of_range` when `index > 3`. `constexpr` |
| `operator==` / `operator!=` | Exact comparison of all four components. There is no approximate comparison; compare per component with a tolerance when needed |

## Handling of the Alpha Component

`Color` always carries an opacity, but not every consumer uses it:

| Consumer | Alpha |
|:---|:---|
| IGES colors (`EntityBase::GetColor()`, `DEColor::GetRGB()`, `IColorDefinition::GetRGB()`) | Always `1.0`. IGES colors have no opacity |
| `MakeColorDefinition(color)` / `ColorDefinition::SetRGB(color)` | Ignored. Only RGB is stored in the Type 314 entity |
| `DisplayState::color_override` (`Assembly::SetColorOverride`) | Ignored. Opacity is overridden separately with `SetOpacityOverride` |
| `EntityRenderer::SetAmbientColor` | Ignored |
| `IEntityGraphics::SetColor`, `DrawContext::highlight_color`, `EntityRenderer::SetBackgroundColor`, `Light::color` | Used as the RGBA value sent to OpenGL |

## Relationship with the IGES Scale

IGES stores the RGB components of a Color Definition entity (Type 314) on a 0.0–100.0 scale. `ColorDefinition` keeps the values exactly as read from the file so that an IGES file written back without modification reproduces the same Parameter Data; the `Color` returned by `GetRGB()` / `GetColor()` is derived from those values on demand. Conversely, `MakeColorDefinition` and `SetRGB` take a `Color` on the unit scale and store `ToIgesRGB()`.

The standard colors of the Directory Entry color number (`ColorNumber`, 1–8) are obtained as a `Color` with `igesio::entities::ToColor(ColorNumber)`, and the closest standard color of a `Color` with `igesio::entities::ClosestColorNumber(color)` (both in [`igesio/entities/de/de_color.h`](./../../include/igesio/entities/de/de_color.h)).
