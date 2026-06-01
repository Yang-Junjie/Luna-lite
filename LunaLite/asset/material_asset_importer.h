#pragma once
#include "asset_importer.h"

namespace lunalite::asset {
class MaterialAssetImporter final : public Importer {
public:
    AssetMetadata import(const std::filesystem::path& assetPath) override;
    std::vector<std::string> getSupportedExtensions() const override;
};
} // namespace lunalite::asset
