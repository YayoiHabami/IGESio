# IGESio C++ Library

[![CI](https://github.com/YayoiHabami/IGESio/workflows/CI/badge.svg)](https://github.com/YayoiHabami/IGESio/actions)
[![codecov](https://codecov.io/github/YayoiHabami/IGESio/graph/badge.svg)](https://codecov.io/github/YayoiHabami/IGESio)
[![License](https://img.shields.io/github/license/YayoiHabami/IGESio)](LICENSE)

For English documentation, see [README.md](README.md).

## 概要

IGESioは、IGES (Initial Graphics Exchange Specification) ファイルフォーマットを扱うためのモダンなC++ライブラリです。本ライブラリは、IGES 5.3 仕様に基づいてIGESファイルの読み込み、書き出し、および関連データの操作機能を包括的に提供します。

現在のバージョンは [`igesio::GetVersion()`](src/common/versions.cpp) で確認できます (例: 0.3.1)。

ライブラリの設計方針やIGES仕様の解釈については、[docs/policy_ja.md](docs/policy_ja.md) を参照してください。

<!-- omit in toc -->
## Table of Contents

- [概要](#概要)
- [主な機能](#主な機能)
- [使用例](#使用例)
  - [基本的な読み込み・書き出し](#基本的な読み込み書き出し)
    - [中間データ構造を使用した読み込み・書き出し](#中間データ構造を使用した読み込み書き出し)
    - [なぜ中間データ構造を使用するのか？](#なぜ中間データ構造を使用するのか)
    - [重要な注意事項](#重要な注意事項)
- [必要システム](#必要システム)
  - [動作確認環境](#動作確認環境)
  - [環境セットアップ](#環境セットアップ)
    - [Windows環境](#windows環境)
    - [Ubuntu環境](#ubuntu環境)
  - [Third-Party Dependencies](#third-party-dependencies)
- [ビルド方法](#ビルド方法)
  - [CMakeプロジェクトへの統合](#cmakeプロジェクトへの統合)
    - [CMakeによるリンク時のコンポーネント名](#cmakeによるリンク時のコンポーネント名)
  - [スタンドアロンでのビルド](#スタンドアロンでのビルド)
    - [プラットフォーム別の代替手段](#プラットフォーム別の代替手段)
    - [テストの実行](#テストの実行)
- [ディレクトリ構造](#ディレクトリ構造)
- [ドキュメント](#ドキュメント)
  - [ファイル別ドキュメント](#ファイル別ドキュメント)
- [著作権・ライセンス](#著作権ライセンス)

## 主な機能

IGESioライブラリは以下の主要機能を提供します：

- **IGESファイル読み込み**: [`igesio::ReadIges`](src/reader.cpp) によるIGESファイルの解析と読み込み
- **IGESファイル書き出し**: [`igesio::WriteIges`](src/writer.cpp) によるIGESファイルの生成と出力
- **エンティティデータ管理**: IGESエンティティの効率的な管理と操作
- **グローバルパラメータ制御**: [`igesio::models::GlobalParam`](include/igesio/models/global_param.h) によるグローバルセクションパラメータの管理

## 使用例

### 基本的な読み込み・書き出し

IGESioライブラリでは、IGESファイルの読み込みに2段階の変換プロセスを採用しています：

1. **IGESファイル → 中間データ構造** （[`IntermediateIgesData`](docs/intermediate_data_structure_ja.md#1-intermediateigesdata構造体)）
2. **中間データ構造 → データクラス** （`IGESData`クラス - 開発中）

#### 中間データ構造を使用した読み込み・書き出し

　現在利用可能な方法として、中間データ構造（[`IntermediateIgesData`](docs/intermediate_data_structure_ja.md#1-intermediateigesdata構造体)）を使用してIGESファイルの読み込みと書き出しができます。詳細については、[中間データ構造のドキュメント](docs/intermediate_data_structure_ja.md)を参照してください。

```cpp
#include <igesio/reader.h>
#include <igesio/writer.h>

int main() {
    try {
        // IGESファイルを中間データ構造に読み込み
        auto data = igesio::ReadIgesIntermediate("input.igs");

        // 必要に応じてデータを修正
        // ...

        // 修正したデータを新しいファイルに書き込み
        igesio::WriteIgesIntermediate(data, "output.igs");
    } catch (const std::exception& e) {
        std::cerr << "エラー: " << e.what() << std::endl;
        return 1;
    }

  return 0;
}
```

#### なぜ中間データ構造を使用するのか？

　2段階アプローチを採用する理由：

- **IGESフォーマットの複雑性**: IGESファイル内の生データと実用的なデータモデル間の変換を段階的に処理
- **エラー処理の分離**: ファイル解析エラーとデータ構造変換エラーを明確に区別
- **開発段階での柔軟性**: 最終的な`IGESData`クラスの設計変更に対する影響を最小化

#### 重要な注意事項

> **警告**: 中間データ構造（`IntermediateIgesData`）は内部実装の詳細であり、将来のバージョンで変更される可能性があります。
>
> 本格的な用途では、完成予定の`IGESData`クラスの使用を強く推奨します：
>
> ```cpp
> // 将来のAPI（開発中）
> auto iges_data = igesio::ReadIges("example.igs");  // IGESDataクラスを返す
> igesio::WriteIges(iges_data, "output.igs");
> ```
>
> 中間データ構造は、開発・デバッグ目的や、`IGESData`クラス完成までの一時的な利用にとどめてください。

## 必要システム

IGESioライブラリをビルドするには以下の環境が必要です：

- **C++17 対応コンパイラ**: モダンなC++機能を使用するため
- **CMake 3.14以上**: ビルドシステムとして使用

### 動作確認環境

このライブラリは以下の環境でビルドとテストを確認しています：

| OS | Compilers |
|----|----|
| ![Ubuntu](https://img.shields.io/badge/Ubuntu-latest-orange?logo=ubuntu) | ![GCC](https://img.shields.io/badge/GCC-✓-green) ![Clang](https://img.shields.io/badge/Clang-✓-green) |
| ![Windows](https://img.shields.io/badge/Windows-latest-blue?logo=windows) | ![GCC](https://img.shields.io/badge/GCC-✓-green) ![Clang](https://img.shields.io/badge/Clang-✓-green) ![MSVC](https://img.shields.io/badge/MSVC-✓-green) |
| ![macOS](https://img.shields.io/badge/macOS-latest-lightgrey?logo=apple) | ![GCC](https://img.shields.io/badge/GCC-✓-green) ![Clang](https://img.shields.io/badge/Clang-✓-green) |

> **クロスプラットフォーム対応**: このライブラリはWindows、Ubuntu、macOSで動作確認済みです。
>
> - **他のLinux環境**: 類似の環境（GCC、CMakeが利用可能なLinuxディストリビューション）では動作する可能性が高いですが、未検証です
> - **コンパイラ**: GCC、Clang、MSVC（Windows）での動作を確認済みです

### 環境セットアップ

#### Windows環境

1. **MinGW-W64**をインストール
2. **CMake**をインストール（[公式サイト](https://cmake.org/)から）
3. **Ninja**をインストール（オプション、`build.bat`使用時のみ必要）

#### Ubuntu環境

```bash
# 必要なパッケージのインストール
sudo apt update
sudo apt install build-essential cmake ninja-build

# バージョン確認
gcc --version
cmake --version
ninja --version
```

### Third-Party Dependencies

このライブラリには、互換性のあるライセンスを持つオプションの依存関係が含まれています：

| ライブラリ | ライセンス | 用途 | オプション |
|-----------|-----------|------|-----------|
| [Eigen3](https://eigen.tuxfamily.org/) | MPL-2.0 | 線形代数演算 | Yes (`-DIGESIO_ENABLE_EIGEN=OFF` で無効化) |
| [Google Test](https://github.com/google/googletest) | BSD-3-Clause | 単体テスト | Yes (`IGESIO_BUILD_TESTING` 有効時のみ) |

**ライセンス互換性**: すべての依存関係はMITと互換性のあるライセンスを使用しています。完全なライセンステキストについては [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES) を参照してください。

**注意**:
- Eigenはヘッダーオンリーライブラリで、明示的に有効化された場合のみインクルードされます
- Google Testは開発時のみ使用され、ライブラリと一緒に配布されません
- このライブラリはサードパーティの依存関係なしでビルドできます

詳細については、[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) を参照してください。

## ビルド方法

### CMakeプロジェクトへの統合

IGESioライブラリは、CMakeの`FetchContent`機能を使用して他のプロジェクトに簡単に統合できます。以下は、`main.cpp`ファイルを実行可能ファイルとしてビルドする`my_project`において、IGESioライブラリを統合するためのCMake設定の例です：

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContentを有効化
include(FetchContent)

# IGESioライブラリを取得
FetchContent_Declare(
    igesio
    GIT_REPOSITORY https://github.com/YayoiHabami/IGESio
    GIT_TAG main  # 特定のタグやコミットハッシュを指定することも可能
)

# IGESioを利用可能にする
FetchContent_MakeAvailable(igesio)

# 実行可能ファイルを作成
add_executable(my_app main.cpp)

# IGESioライブラリをリンク
target_link_libraries(my_app PRIVATE IGESio::IGESio)
```

**Note**: FetchContentを使用する場合、IGESioのテストはデフォルトでビルドされません。これにより、ユーザープロジェクトのビルド時間が短縮されます。

#### CMakeによるリンク時のコンポーネント名

IGESioライブラリは、以下のコンポーネント名でリンクできます。用途に応じて適切なコンポーネントを選択してください：

| コンポーネント名 | 説明 | 使用場面 |
|------------------|------|----------|
| `IGESio::IGESio` | 全てのIGESio機能を含むメインライブラリ | 一般的な用途（推奨） |
| `IGESio::common` | 共通モジュール（メタデータ、エラー処理等） | 基本機能のみ必要な場合 |
| `IGESio::utils` | データ型変換などのユーティリティ | ユーティリティ機能のみ必要な場合 |
| `IGESio::entities` | IGESエンティティ関連の機能 | エンティティ処理のみ必要な場合 |
| `IGESio::models` | 中間データ構造やIGESデータ全体の管理 | データモデル機能のみ必要な場合 |

**注意**: 通常は `IGESio::IGESio` をリンクすることを推奨します。個別コンポーネントの使用は、特定の機能のみが必要な場合や、依存関係を最小化したい場合にのみ検討してください。

### スタンドアロンでのビルド

開発やテスト目的でIGESioライブラリを個別にビルドする場合は、以下の手順に従ってください：

**1. リポジトリのクローン**:
```bash
git clone https://github.com/YayoiHabami/IGESio.git
cd IGESio
```

**2. ビルドディレクトリの作成**:
```bash
mkdir build
cd build
```

**3. CMakeの実行とビルド**:
```bash
cmake ..
cmake --build .
```

#### プラットフォーム別の代替手段

**Windows環境**: `build.bat` スクリプトを使用することも可能です。ただし、CMakeに加えてNinjaのインストールが必要です。事前にインストールし、パスを通しておいてください。ビルド時には、`debug`または`release`オプションを指定できます。また、第二引数に`doc`を指定すると、ドキュメントも生成されます。この際、DoxygenおよびGraphvizが必要です。

```bat
.\build.bat debug
```

**Linux環境**: `build.sh` スクリプトを使用することも可能です。CMakeとNinjaが必要です。初回実行時は実行権限を付与してください。その他のオプションはWindowsと同様です。

```bash
# 実行権限を付与
chmod +x build.sh

# スクリプト実行
./build.sh debug
```

#### テストの実行

スタンドアロンでビルドする場合でも、テストは既定ではビルドされません。`IGESIO_BUILD_TESTING` オプションで制御します：

```bash
# テストを含めてビルド
cmake -DIGESIO_BUILD_TESTING=ON ..

# テストを除外してビルド (既定)
cmake ..
```

ビルド完了後、以下の方法でテストを実行できます。ビルド成果物のディレクトリ (windowsの場合は`build\debug_win`、Linuxの場合は`build/debug_linux`) に移動してから実行してください。

**CTESTを使用する場合**:
```bash
ctest
```

**個別テスト実行ファイルを使用する場合**:
各テスト実行ファイルを直接実行します（例: `test_common`, `test_utils`, `test_entities`, `test_models`, `test_igesio`）。

テストの詳細な定義については、各 `tests` サブディレクトリ内の `CMakeLists.txt` を参照してください（例: [tests/CMakeLists.txt](tests/CMakeLists.txt), [tests/common/CMakeLists.txt](tests/common/CMakeLists.txt)）。

## ディレクトリ構造

```
IGESio/
├── build.bat, build.sh     # WindowsおよびLinux用のビルドスクリプト
├── CMakeLists.txt          # メインのCMakeビルドスクリプト
├── include/                # 公開ヘッダファイル
│   └── igesio/
├── src/                    # ソースファイル
│   ├── common/             # 共通モジュール (メタデータ、エラー処理等)
│   ├── entities/           # エンティティ関連モジュール
│   ├── models/             # データモデル関連モジュール
│   ├── utils/              # ユーティリティモジュール
│   ├── reader.cpp          # IGESファイル読み込み実装
│   └── writer.cpp          # IGESファイル書き出し実装
├── tests/                  # テストコード
│   └── test_data/          # テスト用データ (IGESファイル等)
├── docs/                   # ドキュメント
└── build/                  # ビルド成果物 (生成される)
```

## ドキュメント

プロジェクトに関する詳細なドキュメントは `docs` ディレクトリに整理されています：

- **[policy (ja)](docs/policy_ja.md)**: ライブラリの設計方針やIGES仕様の解釈について
- **[class-reference (ja)](docs/class_reference_ja.md)**: 本ライブラリで使用されているクラスのリファレンス
- **[flow/reader (ja)](docs/flow/reader_ja.md)**: 読み込み処理のフローについて
- **[entity-analysis (en)](docs/entity_analysis.md)**: IGES 5.3における、各エンティティの分類やパラメータについての分析
- **[additional-notes (ja)](docs/additional_notes_ja.md)**: その他の補足事項
- **[todo](docs/todo.md)**: TODOリスト

### ファイル別ドキュメント

以下は、ソースファイルごとのドキュメントです。記載されていないファイルについては、ソースコード内のコメントを参照してください。

**commonモジュール**
- **[Matrix](docs/common/matrix_ja.md)**: 固定/動的サイズの行列クラス

**entitiesモジュール**
- **[Entity class architecture](docs/entities/entity_base_class_architecture_ja.md)**
  - エンティティ関連クラスの継承構造の説明
  - 個別のエンティティクラスが継承する`EntityBase`クラスの説明
- **[Interfaces and derived classes](docs/entities/entities_ja.md)**
  -  `IEntityIdentifier`クラスとインターフェースクラスの説明
  -  個別のエンティティクラスの説明
- **[RawEntityDE and RawEntityPD](docs/intermediate_data_structure_ja.md)**: IGESファイルのDirectory EntryセクションとParameter Dataセクションの中間データ構造

**modelsモジュール**
- **[Intermediate](docs/intermediate_data_structure_ja.md)**: IGESファイル入出力時の中間データ構造

**utilsモジュール**

## 著作権・ライセンス

このライブラリは、MITライセンスの下で提供されています。詳細は [LICENSE](LICENSE) ファイルを参照してください。

&copy; 2025 Yayoi Habami

各ソースファイルの詳細な著作権情報については、ファイルヘッダコメントを参照してください（例: [`include/igesio/common/iges_metadata.h`](include/igesio/common/iges_metadata.h)）。
