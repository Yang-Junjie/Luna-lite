#include "../LunaLite/asset/asset_cache.h"
#include "../LunaLite/asset/lunacube.h"
#include "../LunaLite/project/project_manager.h"

#include <cstdint>
#include <cstring>

#include <filesystem>
#include <iostream>
#include <vector>

namespace {
std::vector<uint8_t> makeTestPixels()
{
    constexpr uint32_t size = 2;
    constexpr uint32_t componentCount = 4;
    constexpr uint32_t faceCount = lunalite::asset::LunaCubeFaceCount;
    std::vector<float> values(size * size * faceCount * componentCount);

    for (uint32_t face = 0; face < faceCount; ++face) {
        for (uint32_t pixel = 0; pixel < size * size; ++pixel) {
            for (uint32_t channel = 0; channel < componentCount; ++channel) {
                const auto index = ((face * size * size) + pixel) * componentCount + channel;
                values[index] = static_cast<float>(face * 100 + pixel * 10 + channel);
            }
        }
    }

    std::vector<uint8_t> pixels(values.size() * sizeof(float));
    std::memcpy(pixels.data(), values.data(), pixels.size());
    return pixels;
}
} // namespace

int main()
{
    using namespace lunalite;

    const auto projectRoot = std::filesystem::current_path() / "build" / "lunacube_test_project";
    std::error_code error;
    std::filesystem::remove_all(projectRoot, error);

    project::ProjectInfo info;
    info.name = "LunaCubeTestProject";
    info.assets_path = "Assets";
    if (!project::ProjectManager::instance().createProject(projectRoot, info)) {
        std::cerr << "Failed to create test project.\n";
        return 1;
    }

    const auto handle = asset::AssetHandle{42};
    const auto artifactPath = asset::cache::getImportedAssetArtifactPath(handle, "environment.lunacube");
    if (artifactPath !=
        std::filesystem::path("Cache") / "ImportedAssets" / handle.toString() / "environment.lunacube") {
        std::cerr << "Unexpected project-relative cache artifact path: " << artifactPath.generic_string() << "\n";
        return 1;
    }

    const auto absoluteArtifactPath = asset::cache::resolveProjectPath(artifactPath);
    if (!std::filesystem::exists(absoluteArtifactPath.parent_path())) {
        std::cerr << "Imported asset cache directory was not created.\n";
        return 1;
    }

    asset::LunaCubeImage image;
    image.format = asset::LunaCubeFormat::RGBA32F;
    image.size = 2;
    image.mip_count = 1;
    image.pixels = makeTestPixels();

    const auto expectedPayloadSize = asset::calculateLunaCubePayloadSize(image.format, image.size, image.mip_count);
    if (!expectedPayloadSize || *expectedPayloadSize != image.pixels.size()) {
        std::cerr << "Unexpected test cubemap payload size.\n";
        return 1;
    }

    if (!asset::writeLunaCube(absoluteArtifactPath, image)) {
        std::cerr << "Failed to write .lunacube fixture.\n";
        return 1;
    }

    const auto loaded = asset::readLunaCube(absoluteArtifactPath);
    if (!loaded) {
        std::cerr << "Failed to read .lunacube fixture.\n";
        return 1;
    }

    if (loaded->format != image.format || loaded->size != image.size || loaded->mip_count != image.mip_count ||
        loaded->pixels != image.pixels) {
        std::cerr << "Loaded .lunacube data does not match the written image.\n";
        return 1;
    }

    std::cout << ".lunacube reader/writer preserved a 2x2 RGBA32F cubemap.\n";
    return 0;
}
