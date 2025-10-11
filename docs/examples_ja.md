# Examples

　本項では、`examples`ディレクトリに含まれるサンプルコードの概要を説明します。各サンプルコードの詳細な説明は、各ソースコード内のコメントを参照してください。

## 目次

- [目次](#目次)
- [GUIアプリケーション](#guiアプリケーション)
  - [iges\_viewer.cpp](#iges_viewercpp)
    - [IGESファイルの読み込み](#igesファイルの読み込み)
    - [ビューアの操作](#ビューアの操作)
- [CUIアプリケーション](#cuiアプリケーション)
  - [iges\_data\_from\_scratch.cpp](#iges_data_from_scratchcpp)
  - [iges\_data\_io.cpp](#iges_data_iocpp)
  - [intermediate\_data\_io.cpp](#intermediate_data_iocpp)
  - [sample\_curves.cpp](#sample_curvescpp)

## GUIアプリケーション

### iges_viewer.cpp

　本ライブラリで提供するグラフィックスモジュールのサンプルコードです。IGESファイルを読み込み、GUI上で表示します。表示するエンティティのタイプを選択したり、マウス操作で回転・拡大縮小・平行移動が可能です。

<img src="./images/curves_viewer_window.png" alt="IGES Viewer Screenshot" width="600"/>

**図. IGES Viewerのスクリーンショット**

　プログラムを起動すると、デフォルトでは上の図のようなウィンドウが表示されます。初期状態では何も表示されず、左側の「Controls」パネルからIGESファイルをロードする必要があります。このControlsパネルは、移動や拡大・縮小が可能です。

#### IGESファイルの読み込み

　コントロールパネルの「IGES File」フィールドにファイルパスを入力し、「Load IGES File」ボタンをクリックします。ファイルパスは絶対パスで指定してください。上記の図では、例として`examples/data/sample_curves.igs`を指定しています。

　ファイルが正常に読み込まれると、曲線エンティティが自動的にビューアーに追加され、表示されます。サポートされていないエンティティや無効なデータはスキップされ、エラーメッセージがコンソールに出力されます。

> IGESファイルによっては、エンティティが表示されない場合があります。これは、当該エンティティが本ライブラリでサポートされていないか、表示可能なエンティティが従属状態<sup>※</sup>にあるためです。
>
> <sup>※</sup>従属状態とは、例えば「曲線がサーフェス上にある」など、他のエンティティに依存している状態を指します。この場合、依存先のエンティティも表示する必要があります。依存先のエンティティがサポートされていない場合、従属エンティティも表示できません。

#### ビューアの操作

- カメラ制御:
  - 左ドラッグ: 視点の回転。
  - 右ドラッグ: パン（平行移動）。
  - マウスホイール: ズームイン/アウト。
  - 「Reset Camera」ボタンでカメラを初期位置に戻せます。
- 投影モード: 以下の2種類から選択可能です。
  - パースペクティブ（遠近法）: 遠近感のある表示。
  - オーソグラフィック（平行投影）: 平行な線を保った表示 (CADで一般的)。
- スクリーンショット:
  - 「Capture Screenshot」ボタンをクリックすると、現在のビューをPNG画像として保存します。
  - ファイル名は「screenshot YYYY-MM-DD HH-MM-SS.png」の形式で、実行ディレクトリに保存されます。
- 背景色: カラーピッカーで背景色を変更可能です。
- エンティティ表示のコントロール:
  - 「Show All Entities」チェックボックスで全エンティティの表示/非表示を切り替えられます。
  - 各エンティティタイプごとに個別のチェックボックスがあり、タイプごとに表示/非表示を切り替えられます。

## CUIアプリケーション

### iges_data_from_scratch.cpp

　エンティティとIGESのデータをプログラム上で作成するサンプルコードです。基本的な曲線エンティティや構造エンティティなどを作成し、簡単にその操作を行った後、IGESファイルに書き出します。

　以下のような出力が得られます。

```
Composite Curve:
  Parameter ranges:
    Curve1 range: [0, 1.5708],
    Curve2 range: [4.71239, 9.42478],
    Curve3 range: [3.14159, 6.28319],
    CompositeCurve range: [0, 9.42478]
  The 2nd curve ID (from TryGet): 2
Arc Parameters
  Normal at t=1.5: ((0.0707372), (0.997495), (0))
  Tangent at t=1.5: ((-0.997495), (0.0707372), (0))
  Dot product: 0

TransformationMatrix parameters: [6.12303e-17, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0, 1.0]

Total entities added: 8
iges_data is ready: true
Writing IGES file to: "path\\to\\IGESio\\build\\debug_ex_win\\examples\\from_scratch.iges"
Write success: true
```

### iges_data_io.cpp

　IGESファイルからデータを読み込み、プログラム上で利用するサンプルコードです。ここでは、IGESファイルから読み込んだエンティティのタイプと数を表示します。

　以下のように、コマンドライン引数としてIGESファイルのパスを指定できます。パスを指定しない場合は、`examples/data/input.igs` がデフォルトで使用されます。`--help` または `-h` を指定すると、使用方法が表示されます。

```
> iges_data_io.exe "path\to\iges\file.igs"
```

　引数なしで実行すると、以下のような出力が得られます。

```
Reading IGES file from: path/to/IGESio/examples\data\input.igs

Table 1. Entity types and counts (102 entities):
Entity Type                    Type#  Supported  Count
--------------------------------------------------------
Color Definition               314    Yes        1
Surface of Revolution          120    No         1
Line                           110    Yes        28
Transformation Matrix          124    Yes        4
Rational B-Spline Curve        126    Yes        30
Circular Arc                   100    Yes        4
Rational B-Spline Surface      128    No         6
Composite Curve                102    Yes        14
Curve on a Parametric Surface  142    No         7
Trimmed Surface                144    No         7
```

### intermediate_data_io.cpp

　IGESファイルから、[中間データ構造](./intermediate_data_structure_ja.md)の入出力を行うサンプルコードです。通常の使用では、`igesio::ReadIges`や `igesio::WriteIges`を使用することで、中間データ構造が内部的に変換されるため、直接中間データ構造を操作する必要はありません。

　結果については、[iges_data_io.cpp](#iges_data_iocpp)とおおよそ同様です。

### sample_curves.cpp

　本ライブラリで実装済みの曲線エンティティを作成し、IGESファイルに書き出すサンプルコードです。[entities](./entities/entities_ja.md)における各曲線エンティティの作成方法の例として参照してください。また、当該ドキュメントの各図は、このサンプルコードで生成したIGESファイルを[IGES Viewer](#guiアプリケーション)で表示したものです。

　通常、コマンドライン出力はなく、`sample_curves.igs`というIGESファイルが生成されます。
