#pragma once
#include "asset.h"

#include <filesystem>
#include <string_view>

namespace lunalite::asset::cache {
std::filesystem::path getProjectCacheRootPath();
std::filesystem::path getImportedAssetDirectory(AssetHandle handle);
bool ensureImportedAssetDirectory(AssetHandle handle);
std::filesystem::path getImportedAssetArtifactPath(AssetHandle handle, std::string_view artifact_name);
std::filesystem::path resolveProjectPath(const std::filesystem::path& project_relative_path);
} // namespace lunalite::asset::cache
