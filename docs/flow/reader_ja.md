# reader.h/cppのフローの説明

## 目次

- [目次](#目次)

## `IgesReader`クラス

### `IgesReader.GetLine`メソッド

　読み込むセクションの種類 (`section`) と、そのセクションの一つ前のセクションの種類 (`prev_sections`) を指定して、次の行を取得するメソッドです。`section`では、表1に示す`SectionType`のいずれかを指定します。

**表1**. `SectionType`の一覧

| `SectionType` | 一つ前のセクション |
| :--: | --- |
| `kFlag` | `std::nullopt` |
| `kStart` | `std::nullopt`, `kFlag` |
| `kGlobal` | `kStart` |
| `kDirectory` | `kGlobal` |
| `kParameter` | `kDirectory` |
| `kTerminate` | `kParameter`, `kData` |
| `kData` | `kGlobal` |

　`prev_sections`では、`section`の一つ前のセクションの種類を指定します。上記の`SectionType`に加え、`std::nullopt`を指定することも可能です。`std::nullopt`を指定した場合、`section`の一つ前のセクションは存在しないものとみなされます。

　以下に入力値`prev_sections`による分岐を示します。

1. `prev_sections.empty()`の場合
   - 表1に従い、一つ前のセクションを取得します (`::GetFormerSections(section)`)。
   - これを`prev_sections`とし、`GetLine(section, prev_sections)`を呼び出し、その結果を返します。
2. `prev_sections.size() > 1`の場合
   - `GetLine(section, {prev_sections[i]}`を`i=0`から順に呼び出し、最初に成功したものを返します。
   - 失敗した場合は、`std::nullopt`を返します。
3. `prev_sections.size() == 1`の場合
   - 以下の通りです。

**指定された一つ前のセクションの数が1つの場合**

　`prev_sections.size() == 1`とし、以降、これを`prev_section` (`= prev_sections[0]`) とします。

　具体例を示します。読み込んだIGESファイルが、表2に示すような行で構成されているとします。`section`が`kGlobal`の場合、すなわち、`kGlobal`の行を読み込む場合、`prev_section`の値に応じて以下のように分岐します。

**表2**. IGESファイルの行数とセクションの例

|Line Number|1|2|3|4|5|6|7|8|9|10|
|---|---|---|---|---|---|---|---|---|---|---|
|SectionType|S|S|G|G|D|D|P|P|P|T|

- `prev_section`が`kStart`の場合
  - 次の行が`kGlobal` (すなわち一つ前が2行目) の場合、`kGlobal`の行 (3行目) を読み込み、返します。
  - それ以外の場合、`std::nullopt`を返します。
- `prev_section`が`kGlobal`の場合
  - 次の行が`kGlobal` (すなわち一つ前が3行目) の場合、`kGlobal`の行 (4行目) を読み込み、返します。
  - それ以外の場合、`std::nullopt`を返します。
- `prev_section`がそれ以外の場合、`std::nullopt`を返します。
