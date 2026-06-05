#include "../core/log.h"
#include "../project/project_manager.h"
#include "asset_cache.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace lunalite::asset::cache {
namespace {
std::filesystem::path getProjectRoot()
{
    return project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
}
} // namespace

std::filesystem::path getProjectCacheRootPath()
{
    return getProjectRoot() / "Cache";
}

std::filesystem::path getImportedAssetDirectory(AssetHandle handle)
{
    return std::filesystem::path("Cache") / "ImportedAssets" / handle.toString();
}

bool ensureImportedAssetDirectory(AssetHandle handle)
{
    LUNA_ASSERT(handle.isValid(), "Imported asset cache directories require a valid asset handle.");
    if (!handle.isValid()) {
        LUNA_CORE_ERROR("Failed to create imported asset cache directory: invalid asset handle");
        return false;
    }

    const auto directory = resolveProjectPath(getImportedAssetDirectory(handle));
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        LUNA_CORE_ERROR(
            "Failed to create imported asset cache directory '{}': {}", directory.string(), error.message());
        return false;
    }

    return true;
}

std::filesystem::path getImportedAssetArtifactPath(AssetHandle handle, std::string_view artifact_name)
{
    ensureImportedAssetDirectory(handle);
    return getImportedAssetDirectory(handle) / std::filesystem::path(std::string(artifact_name));
}

std::filesystem::path resolveProjectPath(const std::filesystem::path& project_relative_path)
{
    return getProjectRoot() / project_relative_path;
}
} // namespace lunalite::asset::cache
