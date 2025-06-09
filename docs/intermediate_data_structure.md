# Intermediate Data Structure of IGESio

## Introduction

IGESio is a C++ library for reading and writing IGES (Initial Graphics Exchange Specification) files. IGES files are a widely used standard format for exchanging 3D CAD data.

This library adopts a two-stage conversion process when reading IGES files:

1. **IGES File → Intermediate Data Structure** (explained on this page)
2. **Intermediate Data Structure → Data Classes** (`IGESData` class and `IEntity` derived classes)

The intermediate data structures (`RawEntityDE`, `RawEntityPD`, etc.) explained on this page are temporarily used after reading IGES files and before generating the final `IGESData` class. The reasons for adopting this two-stage approach are detailed in the "[Why is an Intermediate Data Structure Necessary?](#why-is-an-intermediate-data-structure-necessary)" section below.

## Table of Contents

- [Introduction](#introduction)
- [Table of Contents](#table-of-contents)
- [IGES File Structure](#iges-file-structure)
  - [Five Sections of IGES Files](#five-sections-of-iges-files)
  - [Entity Concept](#entity-concept)
- [Overview of Intermediate Data Structure](#overview-of-intermediate-data-structure)
  - [Why is an Intermediate Data Structure Necessary?](#why-is-an-intermediate-data-structure-necessary)
    - [Problem: Limitations of Line Number References](#problem-limitations-of-line-number-references)
    - [Solution: Two-Stage Conversion Process](#solution-two-stage-conversion-process)
- [Main Data Structures](#main-data-structures)
  - [1. IntermediateIgesData Structure](#1-intermediateigesdata-structure)
  - [2. RawEntityDE Structure (Entity Attributes)](#2-rawentityde-structure-entity-attributes)
    - [2.1 Role of the Structure](#21-role-of-the-structure)
    - [2.2 Main Member Variables](#22-main-member-variables)
    - [2.3 Distinction Between Values and Pointers](#23-distinction-between-values-and-pointers)
    - [2.4 Default Value Management](#24-default-value-management)
    - [2.5 Practical Usage Example](#25-practical-usage-example)
  - [3. RawEntityPD Structure (Entity Geometry Data)](#3-rawentitypd-structure-entity-geometry-data)
- [Basic Usage](#basic-usage)
  - [Reading IGES Files](#reading-iges-files)
  - [Accessing Entity Information](#accessing-entity-information)
  - [Writing IGES Files](#writing-iges-files)
- [Practical Example: Processing Cube Data](#practical-example-processing-cube-data)

## IGES File Structure

To understand IGES files, let's first explain the basic structure of IGES files.

### Five Sections of IGES Files

IGES files consist of the following five sections:

1. **Start Section**
     - Contains file descriptions and comments
     - Human-readable text format
     - Includes information about file creator and purpose

2. **Global Section**
     - Defines file-wide settings and parameters
     - Information about units, precision, creation date/time, etc.
     - Common settings necessary for file interpretation

3. **Directory Entry Section**
     - Index of each entity (geometric element) in the file
     - Entity type, display attributes, references to parameter data
     - Consists of two lines of data per entity

4. **Parameter Data Section**
     - Specific geometric data for each entity
     - Numerical data such as coordinate values, dimensions, curve control points
     - Described in comma-separated format

5. **Terminate Section**
     - Records the number of lines in each section
     - Used for file integrity checking

### Entity Concept

In IGES files, all geometric elements such as points, lines, circles, and surfaces are represented as "entities". Each entity has the following two types of information:

- **Directory Entry (DE)**: Entity attribute information (color, line type, layer, etc.)
- **Parameter Data (PD)**: Specific numerical data defining the entity's geometry

## Overview of Intermediate Data Structure

When IGESio library reads IGES files, it first represents each section as the following C++ structures as "intermediate data structure":

```
IGES File
├── IntermediateIgesData (entire file)
│   ├── start_section (Start Section)
│   ├── global_section (Global Section)
│   ├── directory_entry_section (Directory Entry Section)
│   │   └── Array of RawEntityDE
│   ├── parameter_data_section (Parameter Data Section)
│   │   └── Array of RawEntityPD
│   └── terminate_section (Terminate Section)
```

### Why is an Intermediate Data Structure Necessary?

The reason this library adopts an intermediate data structure lies in the **reference method between entities within IGES files**.

In IGES files, when each entity references other entities, it uses "line numbers of the referenced records". For example, a Composite Curve entity references multiple curve entities to form a single curve. If the line numbers of each component's DE records are 19, 21, and 23, the Composite Curve's PD record will contain line numbers like `19,21,23`.

#### Problem: Limitations of Line Number References

- **Read-only cases**: No problem as line numbers can be used as IDs
- **Cases with entity addition/deletion**: References break down as line numbers change

#### Solution: Two-Stage Conversion Process

1. **Stage 1**: IGES File → Intermediate Data Structure
        - Preserves line number references as-is

2. **Stage 2**: Intermediate Data Structure → Entity Classes (`IEntity` derived)
        - Assigns unique IDs within the program
        - Converts line number references to internal ID references

This approach allows proper management of inter-entity dependencies even when entities are added, deleted, or modified within the program. Similarly, when writing to IGES files, IGES files are generated through the intermediate data structure.

## Main Data Structures

### 1. IntermediateIgesData Structure

This is the main structure representing the entire file. It holds all five sections of the IGES file.

```cpp
struct IntermediateIgesData {
        /// @brief Start Section - file description
        std::string start_section;

        /// @brief Global Section - file-wide settings
        GlobalParam global_section;

        /// @brief Directory Entry Section - entity attribute list
        std::vector<entities::RawEntityDE> directory_entry_section;

        /// @brief Parameter Data Section - entity geometry data
        std::vector<entities::RawEntityPD> parameter_data_section;

        /// @brief Terminate Section - line counts for each section
        std::array<unsigned int, 4> terminate_section;
};
```

### 2. RawEntityDE Structure (Entity Attributes)

The RawEntityDE structure holds information from the **Directory Entry Section** in IGES files. In IGES files, attribute information for each entity (geometric elements such as points, lines, circles) is recorded in a fixed format of two lines of 80 characters each.

#### 2.1 Role of the Structure

The main purposes of this structure are:
- **Index function within the file**: Manages position and reference relationships of each entity
- **Attribute information storage**: Defines visual attributes such as display method, color, line type
- **Coordination with parameter data**: Provides references to actual geometric data

#### 2.2 Main Member Variables

The following are the main member variables of the `RawEntityDE` structure. Each member corresponds to a field in the DE section of the IGES file. The "Number" column indicates the position in the DE section (corresponding to n in `Field Number n` in the specification).

| Type | Member Name | Number | Description |
|---|---|:-:|---|
| `EntityType` | `entity_type` | 1,11 | Entity type (point=116, line=110, circle=100, etc.) |
| `unsigned int` | `parameter_data_pointer` | 2 | Pointer to parameter data |
| `int` | `structure` | 3 | Structure definition (used for macros and assemblies) |
| `int` | `line_font_pattern` | 4 | Line type (1=solid, 2=dashed, etc.) |
| `int` | `level` | 5 | Level number (class for property application) |
| `int` | `view` | 6 | View information (which view to display in) |
| `int` | `transformation_matrix` | 7 | Transformation matrix (rotation/translation) |
| `int` | `label_display_associativity` | 8 | Label display associativity in views |
| `EntityStatus` | `status` | 9 | Entity status (visible/invisible, independent/dependent, etc.) |
| `unsigned int` | `sequence_number` | 10,20 | Directory Entry line number for this entity |
| `int` | `line_weight_number` | 12 | Line weight (0~) |
| `int` | `color_number` | 13 | Color number (1=black, 2=red, 3=green, etc.) |
| `int` | `parameter_line_count` | 14 | Number of parameter data lines |
| `int` | `form_number` | 15 | Form number (distinguishes variants within the same type) |
| `std::string` | `entity_label` | 18 | Entity label (maximum 8 characters) |
| `int` | `entity_subscript_number` | 19 | Label subscript number |

#### 2.3 Distinction Between Values and Pointers

In the IGES specification, many DE fields interpret **positive values as attribute values** and **negative values as pointers**:

```cpp
// Example: line_font_pattern case
int line_font_pattern = 2;      // Positive value: directly specifies dashed line
int line_font_pattern = -50;    // Negative value: references entity at line 50 (line type definition)
```

#### 2.4 Default Value Management

The structure has functionality to record whether each parameter was blank (default) in the file:

```cpp
std::array<bool, 10> is_default_;  // Default value flags
bool SetIsDefault(size_t index, bool value);  // Set default state
const std::array<bool, 10>& IsDefault() const;  // Get default state
```

This allows distinguishing between explicitly specified values and values where defaults were applied due to blanks.

#### 2.5 Practical Usage Example

```cpp
// Example of a red solid line circle entity
RawEntityDE circle_de;
circle_de.entity_type = EntityType::kCircularArc;  // Circular arc entity
circle_de.parameter_data_pointer = 123;            // Parameters start at line 123
circle_de.line_font_pattern = 1;                   // Solid line
circle_de.color_number = 2;                        // Red color
circle_de.level = 1;                               // Layer 1
circle_de.sequence_number = 45;                    // Directory Entry line 45
```

### 3. RawEntityPD Structure (Entity Geometry Data)

The `RawEntityPD` structure holds parameter data that defines the specific geometry of each entity defined in IGES files.

```cpp
struct RawEntityPD {
        /// @brief Entity type
        EntityType type = EntityType::kNull;

        /// @brief Pointer to corresponding directory entry
        unsigned int de_pointer;

        /// @brief Parameter values defining geometry (string format)
        std::vector<std::string> data;

        /// @brief Type information for each parameter
        std::vector<IGESParameterType> data_types;

        /// @brief Sequence number
        unsigned int sequence_number;

        // ...other members
};
```

## Basic Usage

### Reading IGES Files

Basic example of converting IGES files to intermediate data structure:

```cpp
#include "igesio/reader.h"

// Read IGES file
auto data = igesio::ReadIgesIntermediate("sample.iges");

// Display basic file information
std::cout << "File description: " << data.start_section << std::endl;
std::cout << "Number of entities: " << data.directory_entry_section.size() << std::endl;

// Check line counts for each section
std::cout << "Start Section: " << data.terminate_section[0] << " lines" << std::endl;
std::cout << "Global Section: " << data.terminate_section[1] << " lines" << std::endl;
std::cout << "DE Section: " << data.terminate_section[2] << " lines" << std::endl;
std::cout << "PD Section: " << data.terminate_section[3] << " lines" << std::endl;
```

### Accessing Entity Information

Example of getting entity information from read data:

```cpp
// Process entities in order
for (size_t i = 0; i < data.directory_entry_section.size(); ++i) {
        const auto& de = data.directory_entry_section[i];  // Attribute information
        const auto& pd = data.parameter_data_section[i];   // Geometry data

        std::cout << "Entity " << i << ":" << std::endl;
        std::cout << "  Type: " << static_cast<int>(de.entity_type) << std::endl;
        std::cout << "  Color: " << de.color_number << std::endl;
        std::cout << "  Parameter count: " << pd.data.size() << std::endl;
        std::cout << "  Label: " << de.entity_label << std::endl;

        // Verify DE and PD correspondence
        assert(de.entity_type == pd.type);
}
```

### Writing IGES Files

The following is an example of outputting IGES files from intermediate data structure. Here, data modification is performed using `IntermediateIgesData` and saved to a new IGES file.

```cpp
#include "igesio/reader.h"
#include "igesio/writer.h"

// Read data
auto data = igesio::ReadIgesIntermediate("input.iges");

// Modify data as needed
// Example: Change all entities to red color
for (auto& de : data.directory_entry_section) {
        de.color_number = 2;  // Red color
}

// Write modified data to new file
igesio::WriteIgesIntermediate(data, "output.iges");
```

## Practical Example: Processing Cube Data

The following is a processing example using an actual IGES file (rounded corner cube). The file used is `single_rounded_cube.iges` in the `tests/test_data` folder. This file represents the geometry of a cube with one edge filleted.

```cpp
// Read test file
auto data = igesio::ReadIgesIntermediate("single_rounded_cube.iges");

// Verify file contents
std::string expected_description =
        "This file represents the shape of a cube with one side filleted.";
assert(data.start_section == expected_description);

// Verify entity count (102 entities in this example)
assert(data.directory_entry_section.size() == 102);
assert(data.parameter_data_section.size() == 102);

// Data integrity check
for (size_t i = 0; i < data.directory_entry_section.size(); ++i) {
        // Verify DE and PD have the same entity type
        assert(data.directory_entry_section[i].entity_type ==
                     data.parameter_data_section[i].type);
}

// Verify line counts for each section
assert(data.terminate_section[0] == 1);   // Start: 1 line
assert(data.terminate_section[1] == 4);   // Global: 4 lines
assert(data.terminate_section[2] == 204); // DE: 204 lines
assert(data.terminate_section[3] == 185); // PD: 185 lines

// After processing, save as copy file
igesio::WriteIgesIntermediate(data, "cube_copy.iges");
```

