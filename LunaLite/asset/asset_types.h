#pragma once
#include <string>

namespace lunalite::asset {
enum class AssetType {
    None = 0,
    Texture,
    Mesh,
};

inline std::string assetTypeToString(AssetType type)
{
    switch (type) {
        case AssetType::None:
            return "None";
        case AssetType::Texture:
            return "Texture";
        case AssetType::Mesh:
            return "Mesh";
        default:
            return "Unknown";
    }
}

inline AssetType stringToAssetType(const std::string& type)
{
    if (type == "Texture") {
        return AssetType::Texture;
    } else if (type == "Mesh") {
        return AssetType::Mesh;
    }
    return AssetType::None;
}

} // namespace lunalite::asset
