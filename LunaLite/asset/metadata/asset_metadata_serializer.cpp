#include "../../core/log.h"
#include "asset_metadata_serializer.h"

#include <fstream>
#include <system_error>

namespace lunalite::asset {
namespace {
bool hasSpecializedConfig(const YAML::Node& config)
{
    return config.IsDefined() && !config.IsNull();
}
} // namespace

bool AssetMetadataSerializer::write(const AssetMetadata& metadata) const
{
    const auto metadataPath = m_paths.metadataFilePath(metadata);

    std::error_code error;
    std::filesystem::create_directories(metadataPath.parent_path(), error);
    if (error) {
        LUNA_CORE_ERROR(
            "Failed to create metadata directory '{}': {}", metadataPath.parent_path().string(), error.message());
        return false;
    }

    YAML::Node asset;
    asset["Handle"] = static_cast<uint64_t>(metadata.Handle);
    asset["Type"] = assetTypeToString(metadata.Type);
    asset["Name"] = metadata.Name;
    asset["FilePath"] = metadata.FilePath.generic_string();
    asset["MemoryOnly"] = metadata.MemoryOnly;
    if (hasSpecializedConfig(metadata.SpecializedConfig)) {
        asset["Config"] = metadata.SpecializedConfig;
    }

    YAML::Node root;
    root["Asset"] = asset;

    std::ofstream out(metadataPath);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open metadata file for writing: '{}'", metadataPath.string());
        return false;
    }

    out << root;
    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write metadata file: '{}'", metadataPath.string());
        return false;
    }

    return true;
}

std::optional<AssetMetadata> AssetMetadataSerializer::read(const std::filesystem::path& metadataPath) const
{
    try {
        const YAML::Node root = YAML::LoadFile(metadataPath.string());
        const YAML::Node asset = root["Asset"];
        if (!asset) {
            LUNA_CORE_ERROR("Failed to deserialize metadata '{}': missing Asset", metadataPath.string());
            return std::nullopt;
        }

        AssetMetadata metadata;
        if (asset["Handle"]) {
            metadata.Handle = AssetHandle{asset["Handle"].as<uint64_t>()};
        }
        if (asset["Type"]) {
            metadata.Type = stringToAssetType(asset["Type"].as<std::string>());
        }
        if (asset["Name"]) {
            metadata.Name = asset["Name"].as<std::string>();
        }
        if (asset["FilePath"]) {
            metadata.FilePath = asset["FilePath"].as<std::string>();
        }
        if (asset["MemoryOnly"]) {
            metadata.MemoryOnly = asset["MemoryOnly"].as<bool>();
        }
        if (asset["Config"]) {
            metadata.SpecializedConfig = asset["Config"];
        }

        return metadata;
    } catch (const YAML::Exception& exception) {
        LUNA_CORE_ERROR("Failed to deserialize metadata '{}': {}", metadataPath.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to deserialize metadata '{}': {}", metadataPath.string(), exception.what());
    }

    return std::nullopt;
}

} // namespace lunalite::asset
