# IGESioの中間データ構造

## はじめに

　IGESioは、IGES（Initial Graphics Exchange Specification）ファイルの読み書きを行うC++ライブラリです。IGESファイルは3次元CADデータの交換に広く使用されている標準フォーマットです。

　本ライブラリでは、IGESファイルを読み込む際に2段階の変換プロセスを採用しています：

1. **IGESファイル → 中間データ構造**（本ページで解説）
2. **中間データ構造 → データクラス**（`IGESData`クラスおよび`IEntity`継承クラス）

　本ページで説明する中間データ構造（`RawEntityDE`、`RawEntityPD`など）は、IGESファイルを読み込んだ後、最終的な`IGESData`クラスが生成される前に一時的に使用される構造体です。この2段階アプローチを採用する理由については、後述の「[なぜ中間データ構造が必要なのか？](#なぜ中間データ構造が必要なのか)」で詳しく説明します。

## 目次

- [はじめに](#はじめに)
- [目次](#目次)
- [IGESファイルの構造](#igesファイルの構造)
  - [IGESファイルの5つのセクション](#igesファイルの5つのセクション)
  - [エンティティの概念](#エンティティの概念)
- [中間データ構造の概要](#中間データ構造の概要)
  - [なぜ中間データ構造が必要なのか？](#なぜ中間データ構造が必要なのか)
    - [問題点：行番号による参照の限界](#問題点行番号による参照の限界)
    - [解決策：2段階の変換プロセス](#解決策2段階の変換プロセス)
- [主要なデータ構造](#主要なデータ構造)
  - [1. IntermediateIgesData構造体](#1-intermediateigesdata構造体)
  - [2. RawEntityDE構造体（エンティティ属性）](#2-rawentityde構造体エンティティ属性)
    - [2.1 構造体の役割](#21-構造体の役割)
    - [2.2 主要なメンバー変数](#22-主要なメンバー変数)
    - [2.3 数値とポインタの使い分け](#23-数値とポインタの使い分け)
    - [2.4 デフォルト値の管理](#24-デフォルト値の管理)
    - [2.5 実際の使用例](#25-実際の使用例)
- [基本的な使用方法](#基本的な使用方法)
  - [IGESファイルの読み込み](#igesファイルの読み込み)
  - [エンティティ情報の参照](#エンティティ情報の参照)
  - [IGESファイルの書き込み](#igesファイルの書き込み)
- [実践的な例：立方体データの処理](#実践的な例立方体データの処理)


## IGESファイルの構造

　IGESファイルを理解するために、まずIGESファイルの基本構造について説明します。

### IGESファイルの5つのセクション

　IGESファイルは、以下の5つのセクションで構成されています：

1. **スタートセクション（Start Section）**
   - ファイルの説明やコメントが記述される
   - 人間が読めるテキスト形式
   - ファイルの作成者や目的などの情報を含む

2. **グローバルセクション（Global Section）**
   - ファイル全体の設定やパラメータを定義
   - 単位系、精度、作成日時などの情報
   - ファイルの解釈に必要な共通設定

3. **ディレクトリエントリセクション（Directory Entry Section）**
   - ファイル内の各エンティティ（図形要素）の目録
   - エンティティの種類、表示属性、パラメータデータへの参照情報
   - エンティティごとに2行のデータで構成

4. **パラメータデータセクション（Parameter Data Section）**
   - 各エンティティの具体的な形状データ
   - 座標値、寸法、曲線の制御点などの数値データ
   - カンマ区切りの形式で記述

5. **ターミネートセクション（Terminate Section）**
   - 各セクションの行数を記録
   - ファイルの整合性チェックに使用

### エンティティの概念

　IGESファイルでは、点、線、円、曲面など、すべての図形要素を「エンティティ」として表現します。各エンティティは以下の2つの情報を持ちます：

- **ディレクトリエントリ（DE）**: エンティティの属性情報（色、線種、レイヤーなど）
- **パラメータデータ（PD）**: エンティティの形状を定義する具体的な数値データ

## 中間データ構造の概要

　IGESioライブラリでは、IGESファイルを読み込んだ際に、まず「中間データ構造」として、各セクションを以下のC++構造体として表現します：

```
IGESファイル
├── IntermediateIgesData（ファイル全体）
│   ├── start_section（スタートセクション）
│   ├── global_section（グローバルセクション）
│   ├── directory_entry_section（ディレクトリエントリセクション）
│   │   └── RawEntityDE の配列
│   ├── parameter_data_section（パラメータデータセクション）
│   │   └── RawEntityPD の配列
│   └── terminate_section（ターミネートセクション）
```

### なぜ中間データ構造が必要なのか？

　本ライブラリで中間データ構造を採用する理由は、**IGESファイル内でのエンティティ間の参照方式**にあります。

　IGESファイルでは、各エンティティが他のエンティティを参照する際に、「参照先レコードの行番号」を使用します。例えば、Composite Curve（複合曲線）エンティティは、複数の曲線エンティティを参照して1つの曲線を構成します。各構成要素のDEレコードの行数が19、21、23であれば、Composite CurveのPDレコードには、`19,21,23`のように行番号が記述されます。

#### 問題点：行番号による参照の限界

- **読み込みのみの場合**: 行番号をIDとして使用できるため問題なし
- **エンティティの追加・削除がある場合**: 行番号が変化するため、参照関係が破綻する

#### 解決策：2段階の変換プロセス

1. **第1段階**: IGESファイル → 中間データ構造
    - 行番号による参照をそのまま保持

2. **第2段階**: 中間データ構造 → エンティティクラス（`IEntity`継承）
    - プログラム上で一意なIDを割り当て
    - 行番号による参照を内部IDによる参照に変換

　このようにすることで、プログラム上でエンティティの追加・削除・修正を行っても、エンティティ間の依存関係を正しく管理できます。同様に、IGESファイルへの書き出し時にも、中間データ構造を介して、IGESファイルを生成します。

## 主要なデータ構造

### 1. IntermediateIgesData構造体

　ファイル全体を表現するメインの構造体です。IGESファイルの5つのセクションすべてを保持します。

```cpp
struct IntermediateIgesData {
    /// @brief スタートセクション - ファイルの説明文
    std::string start_section;

    /// @brief グローバルセクション - ファイル全体の設定
    GlobalParam global_section;

    /// @brief ディレクトリエントリセクション - エンティティの属性一覧
    std::vector<entities::RawEntityDE> directory_entry_section;

    /// @brief パラメータデータセクション - エンティティの形状データ
    std::vector<entities::RawEntityPD> parameter_data_section;

    /// @brief ターミネートセクション - 各セクションの行数
    std::array<unsigned int, 4> terminate_section;
};
```

### 2. RawEntityDE構造体（エンティティ属性）

　RawEntityDE構造体は、IGESファイル内の**ディレクトリ部（Directory Entry Section）**の情報を保持する構造体です。IGESファイルでは、各エンティティ（点、線、円などの幾何要素）について、その属性情報が2行80文字の固定フォーマットで記録されています。

#### 2.1 構造体の役割

この構造体の主な目的は以下の通りです：
- **ファイル内のインデックス機能**：各エンティティの位置や参照関係を管理
- **属性情報の保持**：表示方法、色、線種などの視覚的属性を定義
- **パラメータデータとの連携**：実際の幾何データへの参照を提供

#### 2.2 主要なメンバー変数

　以下は、`RawEntityDE`構造体の主要なメンバー変数です。各メンバーは、IGESファイルのDEセクションのフィールドに対応しています。また、「番号」列は、DEセクションでの位置 (規格書における`Field Number n`のnに相当) を示しています。

| 型 | メンバ名 | 番号 | 説明 |
|---|---|:-:|---|
| `EntityType` | `entity_type` | 1,11 | エンティティの種類（点=116, 線=110, 円=100など） |
| `unsigned int` | `parameter_data_pointer` | 2 | パラメータデータへのポインタ |
| `int` | `structure` | 3 | 構造定義（マクロやアセンブリで使用） |
| `int` | `line_font_pattern` | 4 | 線の種類（1=実線, 2=破線など） |
| `int` | `level` | 5 | レベル番号 (プロパティ適用のためのクラス)|
| `int` | `view` | 6 | ビュー情報（どのビューで表示するか） |
| `int` | `transformation_matrix` | 7 | 変換行列（回転・移動） |
| `int` | `label_display_associativity` | 8 | ビューでのラベル表示の関連付け |
| `EntityStatus` | `status` | 9 | エンティティの状態（表示/非表示、独立/従属など） |
| `unsigned int` | `sequence_number` | 10,20 | このエンティティのディレクトリ部行番号 |
| `int` | `line_weight_number` | 12 | 線の太さ（0〜） |
| `int` | `color_number` | 13 | 色番号（1=黒, 2=赤, 3=緑など） |
| `int` | `parameter_line_count` | 14 | パラメータデータの行数 |
| `int` | `form_number` | 15 | フォーム番号（同一タイプ内での変種を区別） |
| `std::string` | `entity_label` | 18 | エンティティのラベル（最大8文字） |
| `int` | `entity_subscript_number` | 19 | ラベルの添字番号 |

#### 2.3 数値とポインタの使い分け

　IGES仕様では、多くのDEフィールドで**正の値は属性値**、**負の値はポインタ**として解釈されます：

```cpp
// 例：line_font_pattern の場合
int line_font_pattern = 2;      // 正の値：破線を直接指定
int line_font_pattern = -50;    // 負の値：50行目のエンティティ（線種定義）を参照
```

#### 2.4 デフォルト値の管理

　構造体には、各パラメータがファイル内で空白（デフォルト）だったかを記録する機能があります：

```cpp
std::array<bool, 10> is_default_;  // デフォルト値フラグ
bool SetIsDefault(size_t index, bool value);  // デフォルト状態の設定
const std::array<bool, 10>& IsDefault() const;  // デフォルト状態の取得
```

これにより、明示的に指定された値と、空白でデフォルト値が適用された値を区別できます。

#### 2.5 実際の使用例

```cpp
// 赤い実線の円エンティティの例
RawEntityDE circle_de;
circle_de.entity_type = EntityType::kCircularArc;  // 円弧エンティティ
circle_de.parameter_data_pointer = 123;            // 123行目からパラメータ開始
circle_de.line_font_pattern = 1;                   // 実線
circle_de.color_number = 2;                        // 赤色
circle_de.level = 1;                               // レイヤー1
circle_de.sequence_number = 45;                    // ディレクトリ部45行目
```　

### 3. RawEntityPD構造体（エンティティ形状データ）

　`RawEntityPD`構造体は、IGESファイル内において定義された、各エンティティの具体的な形状を定義するパラメータデータを保持します。

```cpp
struct RawEntityPD {
    /// @brief エンティティの種類
    EntityType type = EntityType::kNull;

    /// @brief 対応するディレクトリエントリへのポインタ
    unsigned int de_pointer;

    /// @brief 形状を定義するパラメータ値（文字列形式）
    std::vector<std::string> data;

    /// @brief 各パラメータの型情報
    std::vector<IGESParameterType> data_types;

    /// @brief シーケンス番号
    unsigned int sequence_number;

    // ...その他のメンバ
};
```

## 基本的な使用方法

### IGESファイルの読み込み

　IGESファイルを中間データ構造に変換する基本的な例：

```cpp
#include "igesio/reader.h"

// IGESファイルを読み込み
auto data = igesio::ReadIgesIntermediate("sample.iges");

// ファイルの基本情報を表示
std::cout << "ファイル説明: " << data.start_section << std::endl;
std::cout << "エンティティ数: " << data.directory_entry_section.size() << std::endl;

// 各セクションの行数を確認
std::cout << "スタートセクション: " << data.terminate_section[0] << "行" << std::endl;
std::cout << "グローバルセクション: " << data.terminate_section[1] << "行" << std::endl;
std::cout << "DEセクション: " << data.terminate_section[2] << "行" << std::endl;
std::cout << "PDセクション: " << data.terminate_section[3] << "行" << std::endl;
```

### エンティティ情報の参照

　読み込んだデータからエンティティの情報を取得する例：

```cpp
// エンティティを順番に処理
for (size_t i = 0; i < data.directory_entry_section.size(); ++i) {
    const auto& de = data.directory_entry_section[i];  // 属性情報
    const auto& pd = data.parameter_data_section[i];   // 形状データ

    std::cout << "エンティティ " << i << ":" << std::endl;
    std::cout << "  種類: " << static_cast<int>(de.entity_type) << std::endl;
    std::cout << "  色: " << de.color_number << std::endl;
    std::cout << "  パラメータ数: " << pd.data.size() << std::endl;
    std::cout << "  ラベル: " << de.entity_label << std::endl;

    // DEとPDの対応関係を確認
    assert(de.entity_type == pd.type);
}
```

### IGESファイルの書き込み

　以下は、中間データ構造からIGESファイルを出力する例です。以下では`IntermediateIgesData`を使用してデータの書き換えを行い、新しいIGESファイルに保存します。

```cpp
#include "igesio/reader.h"
#include "igesio/writer.h"

// データを読み込み
auto data = igesio::ReadIgesIntermediate("input.iges");

// 必要に応じてデータを修正
// 例：すべてのエンティティを赤色に変更
for (auto& de : data.directory_entry_section) {
    de.color_number = 2;  // 赤色
}

// 修正したデータを新しいファイルに書き込み
igesio::WriteIgesIntermediate(data, "output.iges");
```

## 実践的な例：立方体データの処理

　以下は、実際のIGESファイル（角の丸い立方体）を使った処理例です。使用しているファイルは、`tests/test_data`フォルダの`single_rounded_cube.iges`です。このファイルは、1辺をフィレット加工した立方体の形状を表現しています。

```cpp
// テストファイルの読み込み
auto data = igesio::ReadIgesIntermediate("single_rounded_cube.iges");

// ファイル内容の確認
std::string expected_description =
    "This file represents the shape of a cube with one side filleted.";
assert(data.start_section == expected_description);

// エンティティ数の確認（この例では102個のエンティティ）
assert(data.directory_entry_section.size() == 102);
assert(data.parameter_data_section.size() == 102);

// データの整合性チェック
for (size_t i = 0; i < data.directory_entry_section.size(); ++i) {
    // DEとPDで同じエンティティタイプを持つことを確認
    assert(data.directory_entry_section[i].entity_type ==
           data.parameter_data_section[i].type);
}

// 各セクションの行数確認
assert(data.terminate_section[0] == 1);   // スタート: 1行
assert(data.terminate_section[1] == 4);   // グローバル: 4行
assert(data.terminate_section[2] == 204); // DE: 204行
assert(data.terminate_section[3] == 185); // PD: 185行

// 処理後、コピーファイルとして保存
igesio::WriteIgesIntermediate(data, "cube_copy.iges");
```
