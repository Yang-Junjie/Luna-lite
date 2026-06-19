#include "../metadata/asset_metadata_store.h"
#include "sprite_animator_controller_importer.h"

namespace lunalite::asset {

std::vector<AssetMetadata> SpriteAnimatorControllerImporter::import(const std::filesystem::path& assetPath,
                                                                    AssetMetadataStore& metadataStore)
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::SpriteAnimatorController);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> SpriteAnimatorControllerImporter::getSupportedExtensions() const
{
    return {".lunaanimator"};
}

} // namespace lunalite::asset
