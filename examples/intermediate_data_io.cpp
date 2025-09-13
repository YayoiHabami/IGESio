/**
 * @file examples/intermediate_data_io.cpp
 * @author Yayoi Habami
 * @date 2025-08-14
 * @copyright 2025 Yayoi Habami
 * @brief This example demonstrates reading an IGES file into an intermediate
 *        data structure, accessing and processing the data, creating entity
 *        instances from the data, and writing the intermediate data structure
 *        back to a new IGES file.
 *
 * The program performs the following steps:
 * 1. Reads the IGES file into an intermediate data structure using
 *    `igesio::ReadIgesIntermediate`.
 * 2. Counts the total number of entities found in the IGES file.
 * 3. Lists the entity types and their counts.
 * 4. Attempts to create instances of entities from the intermediate
 *    data using `igesio::entities::EntityFactory::CreateEntity`.
 *    It then prints whether each entity type is supported or unsupported.
 * 5. Writes the intermediate data structure to a new IGES file named
 *    "output.igs" using `igesio::WriteIgesIntermediate`.
 *
 * @example Usage: `intermediate_data_io.exe [path_to_iges_file]`
 *
 * Example:
 * ```bash
 * intermediate_data_io.exe
 * ```
 * Reads the default IGES file "examples/data/input.igs".
 *
 * ```bash
 * intermediate_data_io.exe path/to/my_iges_file.igs
 * ```
 * Reads the IGES file specified by the provided path.
 */
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

#include <igesio/reader.h>
#include <igesio/writer.h>

#include <igesio/entities/factory.h>



/// @brief Parse command line arguments
/// @param argc Argument count
/// @param argv Argument vector
/// @return True if arguments are valid, false if help is requested
bool ParseArgs(int argc, char* argv[]) {
    // If the first argument is "--help" or "-h", display usage information
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h") {
            std::cout << "Usage: " << argv[0] << " [path_to_iges_file]" << std::endl;
            std::cout << "If no path is provided, "
                         "defaults to 'examples/data/input.igs'." << std::endl;
            return false;
        }
    }
    return true;
}

/// @brief Read IGES file into intermediate data structure
/// @param path Path to the IGES file. If nullptr, defaults to "examples/data/input.igs".
/// @return Intermediate IGES data structure
igesio::models::IntermediateIgesData
ReadIgesIntermediate(const char* path = nullptr) {
    // Get the path to the IGES file from the current source file
    // The IGES file "input.igs" located in the "examples/data" directory.
    // If a path is provided via command line argument, use it instead.
    std::filesystem::path iges_path;
    if (path) {
        // Use the provided path
        iges_path = std::filesystem::path(path);
        if (!iges_path.is_absolute()) {
            auto current_dir = std::filesystem::current_path();
            iges_path = current_dir / iges_path;
        }
    } else {
        // Use the default path
        auto source_dir = std::filesystem::absolute(__FILE__).parent_path();
        iges_path = source_dir / "data" / "input.igs";
    }

    std::cout << "Reading IGES file from: " << iges_path.string() << std::endl;

    // Read IGES file into intermediate data structure
    return igesio::ReadIgesIntermediate(iges_path.string());
}



int main(int argc, char* argv[]) {
    namespace i_ent = igesio::entities;

    if (!ParseArgs(argc, argv)) return 0;

    try {
        // Read IGES file into intermediate data structure
        auto data = ReadIgesIntermediate((argc > 1) ? argv[1] : nullptr);

        // Count entities
        std::cout << data.directory_entry_section.size() << " entities found." << std::endl;

        // List entity types and their counts
        std::cout << "Entity types and counts:" << std::endl;
        std::unordered_map<i_ent::EntityType, size_t> entity_count;
        for (const auto& de : data.directory_entry_section) {
            entity_count[de.entity_type]++;
        }
        for (const auto& [type, count] : entity_count) {
            std::cout << "  " << i_ent::ToString(type) << " Entity (type#"
                      << static_cast<int>(type) << "): " << count << std::endl;
        }


        // Try to create instances of entities from the intermediate data
        std::cout << "\nCreating entity instances:" << std::endl;
        for (int i = 0; i < data.parameter_data_section.size(); ++i) {
            const auto& de = data.directory_entry_section[i];
            const auto& pd = data.parameter_data_section[i];
            auto entity = i_ent::EntityFactory::CreateEntity(de, pd, {});
            std::cout << "  " << i_ent::ToString(de.entity_type)
                      << " entity (ID: " << entity->GetID() << ") - "
                      << (entity->IsSupported() ? "Supported" : "Unsupported") << std::endl;
        }


        // Write to a new file
        // The output file will be created in the same directory as the executable.
        auto output_dir = std::filesystem::absolute(argv[0]).parent_path();
        auto output_path = output_dir / "output.igs";
        igesio::WriteIgesIntermediate(data, output_path.string());
        std::cout << "\nData written to: " << output_path.string() << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
