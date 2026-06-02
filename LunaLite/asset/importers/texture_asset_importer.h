#pragma once
#include "../asset_importer.h"

namespace lunalite::asset {
class TextureAssetImporter final : public Importer {
public:
    std::vector<AssetMetadata> import(const std::filesystem::path& assetPath) override;
    std::vector<std::string> getSupportedExtensions() const override;
};
} // namespace lunalite::asset
