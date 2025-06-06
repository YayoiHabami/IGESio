# IGESio C++ Library

For English documentation, see [README.md](README.md).

## 概要

IGESioは、IGES (Initial Graphics Exchange Specification) ファイルフォーマットを扱うためのモダンなC++ライブラリです。本ライブラリは、IGES 5.3 仕様に基づいてIGESファイルの読み込み、書き出し、および関連データの操作機能を包括的に提供します。

現在のバージョンは [`igesio::GetVersion()`](src/common/versions.cpp) で確認できます (例: 0.1.0)。

ライブラリの設計方針やIGES仕様の解釈については、[docs/policy_ja.md](docs/policy_ja.md) を参照してください。

<!-- omit in toc -->
## Table of Contents

- [概要](#概要)
- [主な機能](#主な機能)
- [使用例](#使用例)
  - [基本的な読み込み・書き出し](#基本的な読み込み書き出し)
- [必要システム](#必要システム)
  - [動作確認環境](#動作確認環境)
  - [環境セットアップ](#環境セットアップ)
    - [Windows環境](#windows環境)
    - [Ubuntu環境](#ubuntu環境)
  - [Third-Party Dependencies](#third-party-dependencies)
- [ビルド方法](#ビルド方法)
  - [プラットフォーム別の代替手段](#プラットフォーム別の代替手段)
- [テストの実行](#テストの実行)
  - [テストのビルド設定](#テストのビルド設定)
  - [テストの実行方法](#テストの実行方法)
- [ディレクトリ構造](#ディレクトリ構造)
- [ドキュメント](#ドキュメント)
- [著作権・ライセンス](#著作権ライセンス)

## 主な機能

IGESioライブラリは以下の主要機能を提供します：

- **IGESファイル読み込み**: [`igesio::ReadIges`](src/reader.cpp) によるIGESファイルの解析と読み込み
- **IGESファイル書き出し**: [`igesio::WriteIges`](src/writer.cpp) によるIGESファイルの生成と出力
- **エンティティデータ管理**: IGESエンティティの効率的な管理と操作
- **グローバルパラメータ制御**: [`igesio::models::GlobalParam`](include/igesio/models/global_param.h) によるグローバルセクションパラメータの管理

## 使用例

### 基本的な読み込み・書き出し

```cpp
#include <igesio/reader.h>

int main() {
    std::string file_name = "example.igs";
    // TODO: IGESDataクラスを定義後、ここの実装を更新

    return 0;
}
```

## 必要システム

IGESioライブラリをビルドするには以下の環境が必要です：

- **C++17 対応コンパイラ**: モダンなC++機能を使用するため
- **CMake 3.14以上**: ビルドシステムとして使用

### 動作確認環境

このライブラリは以下の環境でビルドとテストを確認しています：

| 環境 | OS | コンパイラ | CMake | Ninja |
|:-----------------:|----|-----------:|------:|------:|
|      Windows      | Windows 11 | GCC 11.2.0 | 3.27.0-rc2 | 1.10.2 |
| Linux<br>(Ubuntu) | Ubuntu 22.04 LTS <br> (WSL2環境を含む) | GCC 11.4.0 | 3.22.1 | 1.10.1 |

> **プラットフォーム対応**: このライブラリはWindowsとUbuntuで動作確認済みです。
>
> - **他のLinux環境**: 類似の環境（GCC、CMakeが利用可能なLinuxディストリビューション）では動作する可能性が高いですが、未検証です
> - **バージョン差異**: 記載されたバージョンより新しいものであれば動作する可能性が高いですが、古いバージョンでは互換性の問題が生じる場合があります
> - **macOS**: 動作するはずですが、環境によっては調整が必要な場合があります

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

以下の手順に従ってIGESioライブラリをビルドしてください：

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

### プラットフォーム別の代替手段

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

## テストの実行

### テストのビルド設定

テストは既定ではビルドされません。`IGESIO_BUILD_TESTING` オプションで制御します：

```bash
# テストを含めてビルド
cmake -DIGESIO_BUILD_TESTING=ON ..

# テストを除外してビルド (既定)
cmake ..
```

### テストの実行方法

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
- **[entity-parameters (en)](docs/entity_parameter_count_en.md)**: IGES 5.3における、各エンティティの分類やパラメータについての分析
- **[additional-notes (ja)](docs/additional_notes_ja.md)**: その他の補足事項
- **[todo](docs/todo.md)**: TODOリスト

## 著作権・ライセンス

このライブラリは、MITライセンスの下で提供されています。詳細は [LICENSE](LICENSE) ファイルを参照してください。

&copy; 2025 Yayoi Habami

各ソースファイルの詳細な著作権情報については、ファイルヘッダコメントを参照してください（例: [`include/igesio/common/iges_metadata.h`](include/igesio/common/iges_metadata.h)）。
