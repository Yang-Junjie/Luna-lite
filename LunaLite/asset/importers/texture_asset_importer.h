#pragma once
#include "../asset_importer.h"

namespace lunalite::asset {
class TextureAssetImporter final : public Importer {
public:
    std::vector<AssetMetadata> import(const std::filesystem::path& assetPath,
                                      AssetMetadataStore& metadataStore) override;
    std::vector<std::string> getSupportedExtensions() const override;
    bool shouldRefreshExistingMetadata(const AssetMetadata& metadata,
                                       const std::filesystem::path& assetPath) const override;
};
} // namespace lunalite::asset
