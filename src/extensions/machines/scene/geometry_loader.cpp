/**
 * @file extensions/machines/scene/geometry_loader.cpp
 * @brief 機械定義・プロジェクト定義の形状 (`GeometrySpec`) の読込
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/scene/geometry_loader.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "igesio/entities/meshes/mesh_entity.h"
#include "igesio/models/global_param.h"
#include "igesio/models/iges_data.h"
#include "igesio/numerics/meshes/algorithms/conversion.h"
#include "igesio/numerics/meshes/algorithms/normals.h"
#include "igesio/reader.h"
#include "igesio/extensions/obj/obj_io.h"
#include "igesio/extensions/machines/machine/primitives.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 診断を追加する
/// @param[out] warnings 追加先 (`nullptr`なら何もしない)
/// @param severity 重大度
/// @param context 発生個所
/// @param message メッセージ
/// @param spec 形状 (行番号に用いる)
void Report(std::vector<Diagnostic>* warnings, const Severity severity,
            const std::string_view context, const std::string& message,
            const GeometrySpec& spec) {
    if (warnings == nullptr) return;
    warnings->push_back(
            Diagnostic{severity, std::string(context), message, spec.line});
}

/// @brief 読み込んだメッシュをmmに換算し、法線が無ければ折り目を保って再計算する
/// @param[in,out] mesh メッシュ
/// @param unit_scale ファイルの長さ単位をmmに換算する係数
/// @param options 読込の設定 (折り目のしきい値)
/// @note 一様な拡大縮小なので法線は換算しない. 平均法線だけでは機械部品の角が
///       丸く陰影が付くため、折り目を保った頂点法線にする
void FinishMesh(numerics::TriangleMeshd& mesh, const double unit_scale,
                const GeometryLoadOptions& options) {
    if (unit_scale != 1.0) mesh.positions *= unit_scale;
    if (!mesh.HasNormals()) {
        numerics::RecomputeNormalsWithCrease(mesh, options.crease_angle_cos);
    }
}

/// @brief IGESファイルを読み込み、ルートアセンブリを取得する
/// @param path ファイルのパス
/// @param spec 形状 (診断に用いる)
/// @param context 診断の発生個所
/// @param[out] warnings 警告の追加先 (`nullptr`なら追加しない)
/// @return ルートアセンブリ. 単位がmmでない場合は警告を追加して`nullptr`
/// @throw igesio::FileOpenError ファイルが開けない場合 (`ReadIges`から伝播)
/// @throw igesio::DataFormatError ファイルの内容が不正な場合 (`ReadIges`から伝播)
std::shared_ptr<models::Assembly> LoadIges(const std::filesystem::path& path,
                                           const GeometrySpec& spec,
                                           const std::string_view context,
                                           std::vector<Diagnostic>* warnings) {
    const models::IgesData data = igesio::ReadIges(path.string());
    if (data.global_section.units_flag != models::UnitFlag::kMillimeter) {
        Report(warnings, Severity::kWarning, context,
               "IGES units are not millimeters; skipped: " + spec.raw_path, spec);
        return nullptr;
    }
    return data.RootPtr();
}

}  // namespace



std::optional<numerics::TriangleMeshd> LoadGeometryMesh(
        const GeometrySpec& spec, const GeometryLoadOptions& options,
        const std::string_view context, std::vector<Diagnostic>* warnings) {
    if (const auto* primitive = std::get_if<PrimitiveSpec>(&spec.source);
        primitive != nullptr) {
        // プリミティブは内部単位で作られ、面法線を持つ
        return MakePrimitiveMesh(*primitive);
    }
    const std::filesystem::path& path = std::get<std::filesystem::path>(spec.source);
    switch (ClassifyGeometryFile(path)) {
        case GeometryFileFormat::kStl: {
            numerics::TriangleMeshd mesh =
                    numerics::CastScalar<double>(ReadStl(path.string(), options.stl));
            FinishMesh(mesh, spec.file_unit_scale, options);
            return mesh;
        }
        case GeometryFileFormat::kObj: {
            numerics::TriangleMeshd mesh = ReadObj(path.string());
            FinishMesh(mesh, spec.file_unit_scale, options);
            return mesh;
        }
        case GeometryFileFormat::kIges:
            Report(warnings, Severity::kInfo, context,
                   "IGES is not a mesh format; use LoadGeometry: " + spec.raw_path,
                   spec);
            return std::nullopt;
        case GeometryFileFormat::kStep:
        case GeometryFileFormat::kUnknown:
            break;
    }
    Report(warnings, Severity::kInfo, context,
           "unsupported geometry format; skipped: " + spec.raw_path, spec);
    return std::nullopt;
}

std::shared_ptr<models::Assembly> LoadGeometry(
        const GeometrySpec& spec, const GeometryLoadOptions& options,
        const std::string_view context, std::vector<Diagnostic>* warnings) {
    auto assembly = models::MakeAssembly();
    const auto* path = std::get_if<std::filesystem::path>(&spec.source);
    if (path != nullptr && ClassifyGeometryFile(*path) == GeometryFileFormat::kIges) {
        const auto root = LoadIges(*path, spec, context, warnings);
        if (root == nullptr) return nullptr;
        assembly->AddChildAssembly(root);
    } else {
        std::optional<numerics::TriangleMeshd> mesh =
                LoadGeometryMesh(spec, options, context, warnings);
        if (!mesh.has_value()) return nullptr;
        assembly->AddEntity(std::make_shared<entities::MeshEntity>(std::move(*mesh)));
    }
    if (spec.color.has_value()) assembly->SetColorOverride(spec.color);
    if (spec.opacity < 1.0f) assembly->SetOpacityOverride(spec.opacity);
    return assembly;
}

}  // namespace igesio::extensions::machines
