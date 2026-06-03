#include "asset_importer.h"

#include <cctype>

#include <algorithm>

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

} // namespace lunalite::asset
