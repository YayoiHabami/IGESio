/**
 * @file models/iges_data.cpp
 * @brief 一つのIGESファイルに対応するデータ構造を管理するクラス
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/models/iges_data.h"

namespace {

namespace i_models = igesio::models;

}  // namespace



i_models::GlobalParam i_models::GetDefaultGlobalParam() {
    GlobalParam param = {};
    param.units_flag = UnitFlag::kMillimeter;
    param.max_line_weight = kDefaultMaxLineWeight;
    param.min_resolution = 0.001;
    return param;
}
