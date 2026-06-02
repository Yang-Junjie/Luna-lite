#pragma once
#include "../asset_importer.h"

#include <tiny_obj_loader.h>

namespace lunalite::asset {
class MeshAssetImporter final : public Importer {
public:
    std::vector<AssetMetadata> import(const std::filesystem::path& assetPath) override;
    std::vector<std::string> getSupportedExtensions() const override;

private:
    static AssetMetadata createOrLoadCompanionMetadata(const std::filesystem::path& assetPath,
                                                       AssetType type,
                                                       AssetHandle suggestedHandle);
    static void writeDefaultMaterial(const std::filesystem::path& materialPath);
    static void writeMaterial(const std::filesystem::path& materialPath, const tinyobj::material_t& material);
    static void writeModel(const std::filesystem::path& modelPath,
                           AssetHandle meshHandle,
                           const std::vector<AssetHandle>& materialHandles);
    static std::vector<AssetMetadata> generateModelCompanions(const std::filesystem::path& assetPath,
                                                              const AssetMetadata& meshMetadata);
};
} // namespace lunalite::asset
