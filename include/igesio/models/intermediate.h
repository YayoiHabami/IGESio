/**
 * @file models/intermediate.h
 * @brief IGESファイルのデータを保持する中間生成物
 * @author Yayoi Habami
 * @date 2025-04-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_MODELS_INTERMEDIATE_H_
#define IGESIO_MODELS_INTERMEDIATE_H_

#include <limits>
#include <string>
#include <vector>

#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/pd.h"
#include "igesio/models/global_param.h"



namespace igesio::models {

/// @brief IGESデータを表す中間生成物
/// @note 1つのIGESファイルは、1つのIntermediateIgesData構造体に格納される
struct IntermediateIgesData {
    /// @brief スタートセクション
    /// @note 各行ごとに"\n"で区切られた文字列
    std::string start_section;
    /// @brief グローバルセクション
    GlobalParam global_section;
    /// @brief ディレクトリエントリセクション
    std::vector<entities::RawEntityDE> directory_entry_section;
    /// @brief パラメータデータセクション
    std::vector<entities::RawEntityPD> parameter_data_section;
    /// @brief ターミネートセクション
    /// @note 前から順に、スタートセクション、グローバルセクション、
    ///       ディレクトリエントリセクション、パラメータデータセクションの行数を格納する.
    std::array<unsigned int, 4> terminate_section;
};

}  // namespace igesio::models

#endif  // IGESIO_MODELS_INTERMEDIATE_H_
