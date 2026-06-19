#include "../metadata/asset_metadata_store.h"
#include "sprite_animation_clip_importer.h"

namespace lunalite::asset {

std::vector<AssetMetadata> SpriteAnimationClipImporter::import(const std::filesystem::path& assetPath,
                                                               AssetMetadataStore& metadataStore)
{
    const auto metadata = metadataStore.createOrLoadMetadata(assetPath, AssetType::SpriteAnimationClip);
    if (!metadataStore.writeMetadataFile(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> SpriteAnimationClipImporter::getSupportedExtensions() const
{
    return {".lunaanim"};
}

} // namespace lunalite::asset
