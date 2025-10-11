# Index (ドキュメント)

## 目次

- [目次](#目次)
- [特に重要なドキュメント](#特に重要なドキュメント)
- [機能別ドキュメント](#機能別ドキュメント)
  - [commonモジュール](#commonモジュール)
  - [entitiesモジュール](#entitiesモジュール)
  - [graphicsモジュール](#graphicsモジュール)
  - [modelsモジュール](#modelsモジュール)
  - [utilsモジュール](#utilsモジュール)
- [その他のドキュメント](#その他のドキュメント)

## 特に重要なドキュメント

　以下は、特に重要なドキュメントです。まずはこちらを参照してください。

- **[ビルド方法](build_ja.md)**: 本ライブラリのビルド方法
- **[Examples (ja)](examples_ja.md)**: `examples`ディレクトリに含まれるサンプルコードの概要
- **[使用するクラスの概観](./class_structure_ja.md)**: 本ライブラリで使用する主要なクラスの関係の説明
- **[実装状況の一覧](./implementation_progress.md)**: エンティティクラス等の実装状況の一覧

## 機能別ドキュメント

　以下は、機能別のドキュメントです。記載されていないファイルについては、ソースコード内のコメントを参照してください。

### commonモジュール

- **[Matrix](common/matrix_ja.md)**: 固定/動的サイズの行列クラス

### entitiesモジュール

- **[エンティティクラスの概観](entities/entities_ja.md)**
  - `IEntityIdentifier`を基底とする、全エンティティクラスの関係の説明
  - 個別のエンティティクラスの基底クラスである、`EntityBase`の説明
- **[個別エンティティクラス](entities/entities_ja.md)**
  - 各インターフェースの説明
  - 各エンティティクラスの説明（コード例、図など）
- **[中間データ構造](intermediate_data_structure_ja.md)**
  - 本ライブラリにおいて、IGESファイルの入出力の際に内部で使用される中間データ構造の説明
  - IGESファイルのDirectory EntryセクションとParameter Dataセクションの中間データ構造
- **[DEフィールド](entities/de_field_ja.md)**
  - Directory Entryセクションに関する簡単な説明
  - DEフィールドを扱うクラス構造の説明

### graphicsモジュール

- 現在、ドキュメントはありません。

### modelsモジュール

- **[中間データ構造](intermediate_data_structure_ja.md)**: IGESファイル入出力時の中間データ構造

### utilsモジュール

- 現在、ドキュメントはありません。

## その他のドキュメント

- **[policy (ja)](policy_ja.md)**: ライブラリの設計方針やIGES仕様の解釈について
- **[flow/reader (ja)](flow/reader_ja.md)**: 読み込み処理のフローについて
- **[entity-analysis (en)](entity_analysis.md)**: IGES 5.3における、各エンティティの分類やパラメータについての分析
- **[additional-notes (ja)](additional_notes_ja.md)**: その他の補足事項
- **[todo](todo.md)**: TODOリスト
