# Index

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Key Documents](#key-documents)
- [Module-Specific Documents](#module-specific-documents)
  - [common module](#common-module)
  - [entities module](#entities-module)
  - [graphics module](#graphics-module)
  - [models module](#models-module)
  - [utils module](#utils-module)
- [Other Documents](#other-documents)

## Key Documents

The following documents are particularly important. Please refer to these first.

- **[Build Instructions](build.md)**: How to build this library
- **[Examples](examples.md)**: Overview of the sample code in the `examples` directory
- **[Overview of Classes Used](./class_structure.md)**: Explanation of the relationships of the main classes used in this library

## Module-Specific Documents

The following are module-specific documents. For files not listed, please refer to the comments in the source code.

### common module

- **[Matrix](common/matrix.md)**: Fixed/dynamic size matrix classes

### entities module

- **[Overview of Entity Classes](entities/entities.md)**
    - Explanation of the relationships between all entity classes based on `IEntityIdentifier`.
    - Description of `EntityBase`, the base class for individual entity classes.
- **[Individual Entity Classes](entities/entities.md)**
    - Explanation of each interface.
    - Description of each entity class (code examples, diagrams, etc.).
- **[Intermediate Data Structure](intermediate_data_structure.md)**
    - Explanation of the intermediate data structure used internally when inputting/outputting IGES files in this library.
    - Intermediate data structure of the Directory Entry section and Parameter Data section of IGES files.
- **[DE Field](entities/de_field.md)**
    - Brief explanation of the Directory Entry section.
    - Explanation of the class structure for handling DE fields.

### graphics module

- Currently, there are no documents.

### models module

- **[Intermediate Data Structure](intermediate_data_structure.md)**: Intermediate data structure when inputting/outputting IGES files

### utils module

- Currently, there are no documents.

## Other Documents

- **[Policy (ja)](policy_ja.md)**: About the library's design policy and interpretation of IGES specifications
- **[Flow/Reader (ja)](flow/reader_ja.md)**: About the flow of the reading process
- **[Entity Analysis (en)](entity-analysis.md)**: Analysis of the classification and parameters of each entity in IGES 5.3
- **[Additional Notes (ja)](additional_notes_ja.md)**: Other supplementary notes
- **[TODO (ja)](todo.md)**: TODO list
