# ビルド方法

本ライブラリは、CMakeの`FetchContent`機能を使用して他のプロジェクトに簡単に統合できます。また、開発やテスト目的で個別にビルドすることも可能です。以下では、これらの方法について説明します。

## 目次

- [目次](#目次)
- [CMakeプロジェクトへの統合](#cmakeプロジェクトへの統合)
  - [CMakeによるリンク時のコンポーネント名](#cmakeによるリンク時のコンポーネント名)
  - [CMakeのオプション](#cmakeのオプション)
- [スタンドアロンでのビルド](#スタンドアロンでのビルド)
  - [プラットフォーム別の代替手段](#プラットフォーム別の代替手段)
  - [テストの実行](#テストの実行)

## CMakeプロジェクトへの統合

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
    GIT_REPOSITORY https://github.com/YayoiHabami/IGESio.git
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

### CMakeによるリンク時のコンポーネント名

IGESioライブラリは、以下のコンポーネント名でリンクできます。用途に応じて適切なコンポーネントを選択してください：

| コンポーネント名 | 説明 |
|------------------|------|
| `IGESio::IGESio` | 全てのIGESio機能を含むメインライブラリ <br> 一般的な用途（推奨） |
| `IGESio::common` | 共通モジュール（メタデータ、エラー処理等） |
| `IGESio::utils` | データ型変換などのユーティリティ |
| `IGESio::entities` | IGESエンティティ関連の機能 |
| `IGESio::models` | 中間データ構造やIGESデータ全体の管理 |
| `IGESio::graphics` | 描画機能 (OpenGL)<br>GUIは含まない |

**注意**: 通常は `IGESio::IGESio` をリンクすることを推奨します。個別コンポーネントの使用は、特定の機能のみが必要な場合や、依存関係を最小化したい場合にのみ検討してください。

### CMakeのオプション

IGESioライブラリのビルドをカスタマイズするためのCMakeオプションは以下の通りです。

| オプション | 説明 | デフォルト |
|------------|------|------------|
| `IGESIO_BUILD_DOCS` | ドキュメントをビルドする | OFF |
| `IGESIO_BUILD_EXAMPLE` | examplesをビルドする | OFF |
| `IGESIO_BUILD_GUI` | GUI（GLFWおよびImGuiを使用）を例としてビルドする<br>※ このオプションがONのとき、`IGESIO_ENABLE_GRAPHICS`も有効化される<br>※ Python環境とjinja2のインストールが必要 | OFF |
| `IGESIO_BUILD_TESTING` | テストをビルドする | OFF |
| `IGESIO_ENABLE_COVERAGE` | カバレッジレポートを有効にする | OFF |
| `IGESIO_ENABLE_EIGEN` | Eigenサポートを有効にする | OFF |
| `IGESIO_ENABLE_GRAPHICS` | OpenGL（glad）サポートを有効にする | OFF |

これらのオプションは、IGESioを`FetchContent_MakeAvailable`で有効化する前に設定する必要があります。また、CMakeのコマンドライン引数としても指定可能です（例: `cmake -DIGESIO_BUILD_TESTING=ON ..`）。

```cmake
# ...

FetchContent_Declare(
    igesio
    GIT_REPOSITORY https://github.com/YayoiHabami/IGESio.git
)

# EigenおよびOpenGLサポートを有効化
set(IGESIO_ENABLE_EIGEN ON)
set(IGESIO_ENABLE_GRAPHICS ON)

# IGESioを利用可能にする
FetchContent_MakeAvailable(igesio)

# ...
```

## スタンドアロンでのビルド

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

スクリプトを使用する場合のオプションは以下の通りです：

**(1)**: `build.bat [debug|release] <doc> <ex>`

- `debug` または `release` を指定すると、ソースコードがビルドされます。
- `doc` を指定すると、ドキュメントもビルドされます（DoxygenおよびGraphvizが必要です）。
- `ex` を指定すると、examplesもビルドされます。

**(2)**: `build.bat doc`

- `doc` のみ指定すると、ソースコードをビルドせずにドキュメントのみビルドされます。

### テストの実行

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
