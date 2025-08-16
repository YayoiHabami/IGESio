# Directory Entry Section Implementation

This section provides an overview of the classes related to the Directory Entry (DE) section of IGES files, implemented in `igesio/entities/de.h` (and `igesio/entities/de/*`).

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Overview](#overview)
  - [Directory Entry Section](#directory-entry-section)
  - [Related Implementations](#related-implementations)
- [RawEntityDE](#rawentityde)
  - [List of Fields](#list-of-fields)
  - [Helper Functions and Design Considerations](#helper-functions-and-design-considerations)
    - [Default Value Management](#default-value-management)
    - [Factory Method](#factory-method)
    - [Internal Use](#internal-use)
- [DEFieldWrapper](#defieldwrapper)
  - [DEFieldWrapper and its Derived Classes](#defieldwrapper-and-its-derived-classes)
    - [Key Design Features](#key-design-features)
    - [Example: Color Field Manipulation](#example-color-field-manipulation)
  - [Key Members of the Class Interface](#key-members-of-the-class-interface)
    - [Constructor](#constructor)
    - [Getting Values](#getting-values)
    - [Pointer and ID Operations](#pointer-and-id-operations)
    - [Example: Pointer and ID Operations](#example-pointer-and-id-operations)

## Overview

### Directory Entry Section

The Directory Entry (DE) section of an IGES file stores information common to all entities and defines the metadata for each entity. The DE section consists of 20 fields, including the entity type and a pointer to the parameter data. These fields are used to identify and define the attributes of the entity.

> Parameter Data: In an IGES file, the parameters for an entity are divided into common parts stored in the DE section and entity-specific parameters stored in the Parameter Data (PD) section. For example, in a circular arc entity, parameters such as the coordinates of the center point and arc endpoints are stored in the PD section. A pointer indicating its location is stored in the second field.

Each field in the Directory Entry section can have a default value (blank or zero), an integer value, or a pointer to another entity. Fields 1, 2, 10, 11, 14, and 20 do not have default values except in compressed format files.

The following table details the 20 fields of the Directory Entry section:

| Number | Field Name | Description |
|---------------|-------------|------|
| 1 | Entity Type Number | Number identifying the entity type |
| 2 | Parameter Data | Pointer to the first line of the entity's parameter data record |
| 3 | Structure | Negative pointer to the Directory Entry of the defining entity that specifies the meaning of the entity, or zero (default) |
| 4 | Line Font Pattern | Line font pattern number, negative pointer to the Directory Entry of the Line Font Definition Entity (Type 304), or zero (default) |
| 5 | Level | Level number where the entity exists, negative pointer to the Directory Entry of the Definition Levels Property Entity (Type 406, Form 1), or zero (default) |
| 6 | View | Pointer to the Directory Entry of the View Entity (Type 410), pointer to the Views Visible Associativity Instance (Type 402, Form 3, 4, or 19), or zero (default) |
| 7 | Transformation Matrix | Pointer to the Directory Entry of the Transformation Matrix Entity (Type 124) used to define the entity, or zero (default) |
| 8 | Label Display Associativity | Pointer to the Directory Entry of the Label Display Associativity (Type 402, Form 5), or zero (default) |
| 9 | Status Number | 8-digit number concatenating four 2-digit values |
| 10 | Sequence Number | Sequence number of the Directory Entry (D#) |
| 11 | Entity Type Number | Same value as Field 1 |
| 12 | Line Weight Number | System display line weight. A stepped value in the range of 0 to the maximum value (parameter 16 of the Global Section) |
| 13 | Color Number | Color number, negative pointer to the Directory Entry of the Color Definition Entity (Type 314), or zero (default) |
| 14 | Parameter Line Count | Number of lines in the entity's parameter data record |
| 15 | Form Number | Form number of the entity for which there are multiple interpretations of parameter values, or zero (default) |
| 16 | Reserved | Reserved for future use |
| 17 | Reserved | Reserved for future use |
| 18 | Entity Label | Up to 8 alphanumeric characters (right-justified), or NULL (default) |
| 19 | Entity Subscript Number | 1 to 8-digit unsigned number associated with the entity label |
| 20 | Sequence Number | Sequence number of the Directory Entry (D#+1) |

### Related Implementations

This library implements the following two systems of DE section-related classes. In most cases, users rarely create instances of either system explicitly, and instead use those automatically created during file input/output or entity creation. The `DEFieldWrapper` derived classes are used to get or set information related to the DE section (e.g., color, line font, transformation matrix) from individual entity classes programmatically.

| Class Name | Description |
| --- | --- |
| `RawEntityDE` | Intermediate product used during input/output with IGES files. When an IGES file is read, data in this format is first generated as the `directory_entry_section` member of `IntermediateIgesData`. |
| `DEFieldWrapper` | Class for holding fields in the DE section that may have pointers. Can hold pointers to each entity object internally. These classes are held as member variables in each individual entity class that inherits from `EntityBase`. |

## RawEntityDE

`RawEntityDE` is a structure that extracts and holds information essential to the implementation of this library from the 20 fields of the Directory Entry (DE) section of an IGES file. This structure is used as intermediate data when reading or writing IGES files.

This structure contains the following main members. Each member directly represents the value of the corresponding DE field and is defined by primitive types such as `int` and `std::string`, or composite types such as `EntityStatus`. These members are set to the default values specified in the IGES file within the constructor.

For more details, see [Intermediate Data Structure](../intermediate_data_structure.md).

### List of Fields

The fields held by `RawEntityDE` are as follows. When reading from an IGES file, all fields are set, including "Parameter Data" (2nd field) and "Parameter Line Count" (14th field), which have no meaning in the program.

| Member Variable | Description |
| --- | --- |
| `entity_type` | **Entity type number** corresponding to DE fields 1 and 11. Represented by the `EntityType` enumeration. |
| `parameter_data_pointer` | **Pointer to parameter data** corresponding to DE field 2. Indicates the line number where the entity's parameter data begins. |
| `structure` | **Structure** corresponding to DE field 3. A negative value indicates a pointer to a structure definition entity, and 0 indicates the default value. |
| `line_font_pattern` | **Line font pattern** corresponding to DE field 4. A negative value indicates a pointer to a line font definition entity, a positive value is a pattern number defined in IGES, and 0 is the default value. |
| `level` | **Entity level** corresponding to DE field 5. A negative value indicates a pointer to a definition levels property entity, a positive value is a level number, and 0 is the default value. |
| `view` | **View** corresponding to DE field 6. Indicates a pointer to a View entity or AssociativityInstance, and 0 is the default value. |
| `transformation_matrix` | **Transformation matrix** corresponding to DE field 7. Indicates a pointer to a Transformation Matrix entity, and 0 is the default value. |
| `label_display_associativity` | **Label display associativity** corresponding to DE field 8. Indicates a pointer to an AssociativityInstance, and 0 is the default value. |
| `status` | **Status number** corresponding to DE field 9. Represented by the `EntityStatus` structure, which has four pieces of information: display status, dependency switch, usage flag, and hierarchy. |
| `sequence_number` | **Sequence number** corresponding to DE fields 10 and 20. Indicates the entity's line number (ID) within the DE section. |
| `line_weight_number` | **Line weight number** corresponding to DE field 12. An integer from 0 to the maximum value, specifying the line weight relatively. |
| `color_number` | **Color number** corresponding to DE field 13. A negative value indicates a pointer to a ColorDefinition entity, a positive value is a color number defined in IGES, and 0 is the default value. |
| `parameter_line_count` | **Parameter line count** corresponding to DE field 14. Indicates the number of lines occupied by the entity's parameter data in the PD section. |
| `form_number` | **Form number** corresponding to DE field 15. Indicates a specific type or variation of the entity. The default value is 0. |
| `entity_label` | **Entity label** corresponding to DE field 18. A string that serves as the entity's identifier. |
| `entity_subscript_number` | **Entity subscript number** corresponding to DE field 19. Functions as a numerical modifier for the entity label. |

### Helper Functions and Design Considerations

#### Default Value Management

`RawEntityDE` has an internal array `is_default_` to track whether each field was a default value (blank or zero) when read from the IGES file. This information is to comply with `A conforming editor shall not affect entities that the user did not edit` (IGES 5.3 Section 1.4.7.1). It can be obtained through the `IsDefault()` method.

#### Factory Method

The static member function `ByDefault(entity_type, form_number)` generates an instance of `RawEntityDE` initialized with default values based on the specified entity type and form number. This method is a convenient way to initialize the structure with minimal information when creating a new entity.

#### Internal Use

The `RawEntityDE` structure is mainly used in the IGES file reading and writing logic. At the time of reading, information is parsed from the text data of the IGES file into this structure, and then converted into an object of the corresponding `EntityBase` derived class (e.g. `CircularArc`). The reverse process is followed at the time of writing. This abstracts the complex parts of file I/O and separates them from entity-specific logic.

## DEFieldWrapper

### DEFieldWrapper and its Derived Classes

In this library, each field of the Directory Entry (DE) section is classified into the following three types, and the `DEFieldWrapper` inheritance class is defined as a class that expresses the fields of (2) among them.

1. **Fields represented by enumerations or primitive types**
    - Entity Type (1st/11th fields), Status Number (9th field), Entity Label (18th field), etc.
2. **Fields that can have pointers**
    - Structure (3rd field), Transformation Matrix (7th field), Color (13th field), etc.
3. **Fields that have no meaning in the `EntityBase` inheritance class**
    - Parameter Data (2nd field), Parameter Line Count (14th field), Reserved (16th/17th fields), etc.

In field classification (2), the value can be either a default value, a primitive type value, or a pointer. The `DEFieldWrapper` class is designed so that it can be used without being aware of which type of value is set for each field. Since the fields of classification (1) are directly represented by enumerations and primitive types, the `DEFieldWrapper` class is not used. In the `EntityBase` inheritance class, these fields of classification (1) and (2) are held as member variables, and values are acquired and set as necessary.

#### Key Design Features

**Value Type Management**
The state of the value held by the field is managed by the `DEFieldValueType` enumeration (`kDefault`, `kPointer`, `kPositive`). This allows you to determine whether the value is the default, a pointer, or a unique positive value.

**Type-Safe Pointers**
The template parameter `Args...` is used to specify at compile time the type of entity that the field can reference. This prevents setting pointers to entities of unintended types.

**Prevention of Circular References**
By holding references to entities with `std::weak_ptr`, memory leaks due to mutual references between entities are prevented.

**Consistency of ID and Pointer**
Ensures that the entity ID (`id_`) held internally and the ID of the entity pointed to by the actual pointer (`weak_ptrs_`) match. A runtime exception is thrown if you try to set a pointer with a mismatched ID.

> The "ID and pointer consistency" function is for handling sequential processing when reading IGES files. If the entity referenced by the DE field has not yet been instantiated, only the ID can be set first, and the pointer can be set later.

#### Example: Color Field Manipulation

The following is an example of manipulating the color field (13th DE Field) of the `CircularArc` entity. Default values, enumeration values, and pointer values are set, and values are acquired.

```cpp
#include <igesio/entities/de.h>
#include <igesio/entities/curves/circular_arc.h>

using Vector2d = iges::Vector2d;
namespace ent = iges::entities;

// Array to hold color values: IGES expresses RGB values in the range of 0-100
// Can be obtained as an array of double type regardless of the type of value stored
std::array<double, 3> color;

// Create a circle entity with a radius of 5.0 and centered at the origin
// Each DE field is initialized with the default value
auto center = Vector2d(0.0, 0.0);
auto circle = std::make_shared<ent::CircularArc>(center, 5.0);

// Get the default color
circle->GetColor().GetValueType();    // DEFieldValueType::kDefault
color = circle->GetColor().GetRGB();  // {0, 0, 0}

// Set to the specified color (enumeration value)
circle->OverwriteColor(ent::ColorNumber::kCyan);
circle->GetColor().GetValueType();    // DEFieldValueType::kPositive
color = circle->GetColor().GetRGB();  // {0, 100, 100}

// Create and set a color entity (pointer value)
auto color_def = std::make_shared<ent::ColorDefinition>(
         std::array<double, 3>{50.0, 100.0, 30.0}, "Light Green");
circle->OverwriteColor(color_def);
circle->GetColor().GetValueType();    // DEFieldValueType::kPointer
color = circle->GetColor().GetRGB();  // {50, 100, 30}
```

### Key Members of the Class Interface

The `DEFieldWrapper` class is defined as a class that takes one or more template parameters. `typename... Args` is a variable-length template argument that specifies the type of entity that this field may refer to as a pointer.

```cpp
template<typename... Args>
class DEFieldWrapper { ... };
```

The following is a list of `DEFieldWrapper` derived classes.

| Number | Class Name | Template Parameters |
|:--:| --- | ----------------------- |
| 3 | `DEStructure` | `IStructure` |
| 4 | `DELineFontPattern` | `ILineFontDefinition` |
| 5 | `DELevel` | `IDefinitionLevelsProperty` |
| 6 | `DEView` | `IView`, `IViewsVisibleAssociativity` |
| 7 | `DETransformationMatrix` | `ITransformation` |
| 8 | `DELabelDisplayAssociativity` | `ILabelDisplayAssociativity` |
| 13 | `DEColor` | `IColorDefinition` |

#### Constructor

| Constructor | Description |
| --- | --- |
| `DEFieldWrapper()` | Initializes the field with the default value (value is 0). |
| `explicit DEFieldWrapper(const uint64_t id)` | Initializes by specifying the ID of the entity to be referenced as a pointer. Used to hold only the ID when reading the DE section from an IGES file. |

In addition, copy constructors, move constructors, assignment operators, etc. are also defined.

#### Getting Values

| Function Name | Return Value | Description |
| --- | --- | --- |
| `GetValueType()` | `DEFieldValueType` | Returns the type of the current field value. |
| `GetValue()` | `int` | Gets an integer value to write to the IGES file. If the value is a pointer, the ID is **converted to a negative number** and returned. |
| `GetPointer<T>()` | `std::shared_ptr<const T>` | Gets a smart pointer to the referenced entity by specifying the type `T`. Returns `nullptr` if the pointer is invalid. |
| `GetID()` | `uint64_t` | Returns the ID of the entity that this field is referencing or attempting to reference. |

#### Pointer and ID Operations

| Function Name | Arguments | Description |
| --- | --- | --- |
| `HasValidPointer()` | None | Returns whether the current field has a valid pointer. Returns `true` if the pointer has been set, `false` otherwise. |
| `SetPointer(ptr)` | `ptr`<br>(`std::shared_ptr<const T>`) | Sets a pointer to the entity. The pointer is valid only if the ID of the argument `ptr` matches the ID held by this instance.<br>If they do not match, a `std::invalid_argument` exception is thrown. Use `OverwritePointer(ptr)` if you want to set a new pointer. |
| `OverwritePointer(ptr)` | `ptr`<br>(`std::shared_ptr<const T>`) | Completely overwrites the current ID and pointer with those of the argument `ptr`. If the pointer is invalid, the ID is also invalidated. |
| `OverwriteID(new_id)` | `new_id`<br>(`uint64_t`) | Invalidates the current pointer and sets a new ID. The pointer becomes `nullptr`. |
| `GetUnsetID()` | None | Returns the ID if the ID is set but the corresponding pointer entity has not yet been set. This is used in the process of associating (resolving) pointers after all entity objects have been generated after reading the file. Returns `std::nullopt` if the ID is not set. |

#### Example: Pointer and ID Operations

The following is a continuation of the code created in [Example: Color Field Manipulation](#example-color-field-manipulation).

```cpp
circle->GetColor().HasValidPointer();  // true - Light Green's ColorDefinition is set

// Create a new color definition entity
auto new_color_def = std::make_shared<ent::ColorDefinition>(
         std::array<double, 3>{100.0, 50.0, 0.0}, "Orange");

// Overwrite with a pointer to the new color definition entity
// Inside `OverwritePointer`, `DEColor::OverwriteID` and `DEColor::SetPointer` are called
circle.OverwriteColor(new_color_def);

// Get the new color
color = circle->GetColor().GetRGB();  // {100, 50, 0}
```

The `EntityBase` inheritance class only provides const references to each DE field. Therefore, if you want to set a new value as in the example above, use the `Overwrite` function corresponding to that field (`EntityBase::OverwriteColor` in the case of the color field).

> To reduce the amount of operation on the user side, it is designed so that non-const references to `DEFieldWrapper` cannot be obtained from the entity class. Functions such as `DEFieldWrapper::SetPointer` are used when sequentially reading from an IGES file (setting only the ID in a state where the referenced instance has not been generated). Therefore, in normal use, you will use a const reference to get the value of the field and an `Overwrite` function to set the value.
