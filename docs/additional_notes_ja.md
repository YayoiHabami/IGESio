# Additional Notes

　本ページでは、個別にページを作成するには内容が薄いものの、何らかの形で記載しておきたい情報をまとめています。

<!-- omit in toc -->
## Table of Contents

- [Entities](#entities)
  - [Associativity (Type 302, Type 402)](#associativity-type-302-type-402)
    - [Associativity Definition vs Associativity Instance](#associativity-definition-vs-associativity-instance)
    - [Structure of Associativity](#structure-of-associativity)

## Entities

### Associativity (Type 302, Type 402)

　IGES（Initial Graphics Exchange Specification）ファイル形式においては、**Associativity（関連性）機能**が定義されています。以下に関係性をまとめます (Section 4.69, Section 4.80)。

#### Associativity Definition vs Associativity Instance

| | Associativity Definition | Associativity Instance |
|---|:-:|:-:|
| **Type** | 302 | 402 |
| **役割** | 関連性のスキーマを定義<br> (ユーザー定義の設計図) | 実際の関連性を表現<br> (インスタンス) |
| **Form Number** | 5001-9999 | 1-21 (事前定義済み)<br> 21-5000 (Reserved)<br> 5001-9999 (ユーザー定義) |
| **説明** | エンティティ間の関係の「文法」を規定<br> (意味論ではなく構文を定義) | Form Number 1-5000 は事前定義済み<br> (仕様書に記載、ファイル内に定義エンティティ不要)<br><br> Form Number 5001-9999 はユーザー定義<br>(Type 302で定義が必要) |

#### Structure of Associativity

　関連性は、複数の **クラス（Classes）** で構成され、各クラスは独立したリストとして扱われます。クラス間およびクラス内の要素間には関連性が存在します。また、各クラスは、以下の要素（属性）を持ちます。

**各クラスの属性**

1. **Back Pointer**: メンバーエンティティが関連性インスタンスへの逆参照を持つかどうか
2. **Order**: クラス内の要素の順序が重要かどうか
3. **Entry**: クラスのメンバー（複数のアイテムで構成可能）
4. **Item Type**: 各アイテムがポインタか値かを指定

**データ構造の例**

```
Associativity Definition (Type 302)
├─ Class 1: BP(1), OR(1), N(1), IT(1,1)...IT(1,N(1))
├─ Class 2: BP(2), OR(2), N(2), IT(2,1)...IT(2,N(2))
└─ Class K: BP(K), OR(K), N(K), IT(K,1)...IT(K,N(K))

Associativity Instance (Type 402)
├─ NE(1), NE(2)...NE(K) [各クラスのエントリ数]
├─ Class 1 entries: I(1,1,1)...I(1,NE(1),N(1))
├─ Class 2 entries: I(2,1,1)...I(2,NE(2),N(2))
└─ Class K entries: I(K,1,1)...I(K,NE(K),N(K))
```

**Usage Examples**

- **座標関係**: 各エントリがX、Y、Z値の3つのアイテムを持つ位置データのグループ化
- **階層構造**: 親子関係や依存関係の表現
- **有限要素解析**: ノード間の関係定義

この仕組みにより、IGESファイル内で複雑なエンティティ間の関係を柔軟に定義・表現することが可能になります。
