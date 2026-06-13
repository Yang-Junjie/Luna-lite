#pragma once
#include <string>

namespace lunalite::asset {
enum class AssetType {
    None = 0,
    Mesh,
    Material,
    Prefab,
    Script,
    Texture,
    Sprite,
    Scene,
};

inline std::string assetTypeToString(AssetType type)
{
    switch (type) {
        case AssetType::None:
            return "None";
        case AssetType::Mesh:
            return "Mesh";
        case AssetType::Material:
            return "Material";
        case AssetType::Prefab:
            return "Prefab";
        case AssetType::Script:
            return "Script";
        case AssetType::Texture:
            return "Texture";
        case AssetType::Sprite:
            return "Sprite";
        case AssetType::Scene:
            return "Scene";
        default:
            return "Unknown";
    }
}

inline AssetType stringToAssetType(const std::string& type)
{
    if (type == "Mesh") {
        return AssetType::Mesh;
    }
    if (type == "Material") {
        return AssetType::Material;
    }
    if (type == "Prefab") {
        return AssetType::Prefab;
    }
    if (type == "Script") {
        return AssetType::Script;
    }
    if (type == "Texture") {
        return AssetType::Texture;
    }
    if (type == "Sprite") {
        return AssetType::Sprite;
    }
    if (type == "Scene") {
        return AssetType::Scene;
    }
    return AssetType::None;
}

} // namespace lunalite::asset
