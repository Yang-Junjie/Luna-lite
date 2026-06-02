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
    std::error_code error;
    std::filesystem::create_directories(resolveProjectPath(getImportedAssetDirectory(handle)), error);
    return !error;
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
