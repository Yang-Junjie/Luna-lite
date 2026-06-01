#pragma once
#include <string>

namespace lunalite::asset {
enum class AssetType {
    None = 0,
    Mesh,
    Material,
    Model,
    Script,
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
        case AssetType::Model:
            return "Model";
        case AssetType::Script:
            return "Script";
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
    if (type == "Model") {
        return AssetType::Model;
    }
    if (type == "Script") {
        return AssetType::Script;
    }
    return AssetType::None;
}

} // namespace lunalite::asset
