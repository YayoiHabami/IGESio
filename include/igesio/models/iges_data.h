/**
 * @file models/iges_data.h
 * @brief 一つのIGESファイルに対応するデータ構造を管理するクラス
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_MODELS_IGES_DATA_H_
#define IGESIO_MODELS_IGES_DATA_H_

#include <memory>
#include <string>

#include "igesio/common/id_generator.h"
#include "igesio/models/assembly.h"
#include "igesio/models/global_param.h"



namespace igesio::models {

/// @brief 本ライブラリにおけるデフォルトのグローバルパラメータ
/// @return デフォルトのグローバルパラメータ
/// @note 単位フラグ (14) はミリメートル、最大線の太さの幅 (17) は1.0、
///       最小ユーザー意図解像度 (19) は0.001に設定される
GlobalParam GetDefaultGlobalParam();

/// @brief IGESデータクラス
/// @note 一つのIGESファイルに対応するデータ構造を表す.
///       「エンティティの集合」という責務はルートAssemblyが担い、本クラスは
///       入出力に関わる説明文・グローバルパラメータと、ルートAssemblyへの
///       合成(継承ではない)を保持するラッパーとする.
class IgesData {
 private:
    /// @brief IGESデータのID
    ObjectID id_ = IDGenerator::Generate(ObjectType::kIgesData);

    /// @brief エンティティの集合を保持するルートAssembly
    /// @note Assemblyのツリー機構(`weak_from_this`等)のためshared_ptrで保持する
    std::shared_ptr<Assembly> root_ = std::make_shared<Assembly>();

 public:
    /// @brief このIGESデータの説明
    /// @note IGESファイルのスタートセクションに対応する
    std::string description;

    /// @brief グローバルセクションのパラメータ
    GlobalParam global_section = GetDefaultGlobalParam();

    /// @brief IgesDataのIDを取得する
    /// @return IgesDataのID
    const ObjectID& GetID() const { return id_; }

    /// @brief ルートAssemblyを取得する
    /// @return ルートAssemblyへの参照
    Assembly& Root() { return *root_; }
    /// @brief ルートAssemblyを取得する (変更不可)
    /// @return ルートAssemblyへの参照
    const Assembly& Root() const { return *root_; }
};

}  // namespace igesio::models

#endif  // IGESIO_MODELS_IGES_DATA_H_
