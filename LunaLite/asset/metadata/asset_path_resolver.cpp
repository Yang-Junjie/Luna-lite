#include "../../project/project_manager.h"
#include "asset_path_resolver.h"

#include <system_error>

namespace lunalite::asset {

std::filesystem::path AssetPathResolver::projectRoot() const
{
    return project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
}

std::filesystem::path AssetPathResolver::makeProjectRelative(const std::filesystem::path& path) const
{
    const auto root = projectRoot();
    const auto absolutePath = path.is_absolute() ? path : root / path;

    std::error_code error;
    const auto relativePath = std::filesystem::relative(absolutePath, root, error);
    if (!error) {
        return relativePath.lexically_normal();
    }

    return absolutePath.lexically_normal();
}

std::filesystem::path AssetPathResolver::metadataFilePath(const AssetMetadata& metadata) const
{
    auto metadataPath = projectRoot() / metadata.FilePath;
    metadataPath += ".lunameta";
    return metadataPath;
}

} // namespace lunalite::asset
