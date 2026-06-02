#include "../core/log.h"
#include "asset_importer.h"

#include <cctype>

#include <algorithm>
#include <fstream>
#include <system_error>

namespace lunalite::asset {
namespace {
std::string normalizeExtension(std::string extension)
{
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}
} // namespace

bool Importer::supports(const std::filesystem::path& assetPath) const
{
    const auto extension = normalizeExtension(assetPath.extension().string());
    for (const auto& supportedExtension : getSupportedExtensions()) {
        if (extension == normalizeExtension(supportedExtension)) {
            return true;
        }
    }

    return false;
}

bool Importer::shouldRefreshExistingMetadata(const AssetMetadata&, const std::filesystem::path&) const
{
    return false;
}

std::filesystem::path Importer::getProjectRoot()
{
    return project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
}

std::filesystem::path Importer::makeProjectRelative(const std::filesystem::path& path)
{
    const auto projectRoot = getProjectRoot();
    const auto absolutePath = path.is_absolute() ? path : projectRoot / path;

    std::error_code error;
    const auto relativePath = std::filesystem::relative(absolutePath, projectRoot, error);
    if (!error) {
        return relativePath.lexically_normal();
    }

    return absolutePath.lexically_normal();
}

std::filesystem::path Importer::getMetaFilePath(const AssetMetadata& metadata)
{
    auto metaPath = getProjectRoot() / metadata.FilePath;
    metaPath += ".lunameta";
    return metaPath;
}

bool Importer::serializeMetadata(const AssetMetadata& metadata)
{
    const auto metaPath = getMetaFilePath(metadata);

    std::error_code error;
    std::filesystem::create_directories(metaPath.parent_path(), error);
    if (error) {
        LUNA_CORE_ERROR(
            "Failed to create metadata directory '{}': {}", metaPath.parent_path().string(), error.message());
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

    std::ofstream out(metaPath);
    if (!out.is_open()) {
        LUNA_CORE_ERROR("Failed to open metadata file for writing: '{}'", metaPath.string());
        return false;
    }

    out << root;
    if (!out.good()) {
        LUNA_CORE_ERROR("Failed to write metadata file: '{}'", metaPath.string());
        return false;
    }

    return true;
}

AssetMetadata Importer::deserializeMetadata(const std::filesystem::path& metaPath)
{
    try {
        const YAML::Node root = YAML::LoadFile(metaPath.string());
        const YAML::Node asset = root["Asset"];
        if (!asset) {
            LUNA_CORE_ERROR("Failed to deserialize metadata '{}': missing Asset", metaPath.string());
            return {};
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
        LUNA_CORE_ERROR("Failed to deserialize metadata '{}': {}", metaPath.string(), exception.what());
    } catch (const std::filesystem::filesystem_error& exception) {
        LUNA_CORE_ERROR("Failed to deserialize metadata '{}': {}", metaPath.string(), exception.what());
    }

    return {};
}

AssetMetadata Importer::createMetadata(const std::filesystem::path& assetPath, AssetType type)
{
    AssetMetadata metadata;
    metadata.Handle = AssetHandle{};
    metadata.Type = type;
    metadata.Name = assetPath.stem().string();
    metadata.FilePath = makeProjectRelative(assetPath);
    metadata.MemoryOnly = false;
    return metadata;
}

bool Importer::hasSpecializedConfig(const YAML::Node& config)
{
    return config.IsDefined() && !config.IsNull();
}

} // namespace lunalite::asset
