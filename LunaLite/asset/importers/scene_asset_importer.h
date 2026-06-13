#pragma once
#include "../asset_importer.h"

namespace lunalite::asset {
class SceneAssetImporter final : public Importer {
public:
    std::vector<AssetMetadata> import(const std::filesystem::path& assetPath,
                                      AssetMetadataStore& metadataStore) override;
    std::vector<std::string> getSupportedExtensions() const override;
};
} // namespace lunalite::asset
