/**
 * @file examples/iges_data_io.cpp
 * @author Yayoi Habami
 * @date 2025-08-14
 * @copyright 2025 Yayoi Habami
 * @brief This example demonstrates reading an IGES file into an IgesData
 *        class.
 */
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>

#include <igesio/reader.h>
#include <igesio/writer.h>

#include <igesio/entities/factory.h>

namespace i_ent = igesio::entities;

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
igesio::models::IgesData
ReadIgesDef(const char* path = nullptr) {
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

    return igesio::ReadIges(iges_path.string());
}



void ShowEntityCounts(const igesio::models::IgesData& data) {
    // Count entity types (and whether they are supported)
    std::unordered_map<i_ent::EntityType, int> type_counts;
    std::unordered_map<i_ent::EntityType, bool> is_supported;
    for (const auto& [id, entity] : data.GetEntities()) {
        type_counts[entity->GetType()]++;
        is_supported[entity->GetType()] = entity->IsSupported();
    }

    // Find maximum name length for formatting
    int max_name = 0;
    for (const auto& [type, count] : type_counts) {
        int name_length = static_cast<int>(i_ent::ToString(type).length());
        if (name_length > max_name) max_name = name_length;
    }

    // Display results
    std::cout << std::left << std::setw(max_name + 2) << "Entity Type"
                << std::setw(7) << "Type#" << std::setw(11) << "Supported"
                << std::setw(7) << "Count" << std::endl;
    std::cout << std::string(max_name + 2 + 7 + 11 + 7, '-') << std::endl;
    for (const auto& [type, count] : type_counts) {
        std::cout << std::left << std::setw(max_name + 2)
                    << i_ent::ToString(type)
                    << std::setw(7) << static_cast<int>(type)
                    << std::setw(11) << (is_supported[type] ? "Yes" : "No")
                    << std::setw(7) << count << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (!ParseArgs(argc, argv)) return 0;

    try {
        // Read IGES file into IgesData class
        auto data = ReadIgesDef((argc > 1) ? argv[1] : nullptr);

        // List entity types and their counts
        std::cout << std::endl << "Table 1. Entity types and counts ("
                  << data.GetEntityCount() << " entities):" << std::endl;
        ShowEntityCounts(data);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
