#pragma once
#include "lunacube.h"

#include <cstdint>

#include <filesystem>
#include <optional>
#include <vector>

namespace lunalite::asset {
struct HdrImage {
    uint32_t width{0};
    uint32_t height{0};
    std::vector<float> pixels;
};

class EnvironmentMapBaker final {
public:
    static std::optional<HdrImage> loadHdrImage(const std::filesystem::path& asset_path);
    static std::optional<LunaCubeImage> bakeEnvironmentCube(const HdrImage& hdr_image, uint32_t cubemap_size);
    static std::optional<LunaCubeImage> bakeIrradianceCube(const LunaCubeImage& environment_cube, uint32_t size);
    static std::optional<LunaCubeImage> bakePrefilterCube(const LunaCubeImage& environment_cube, uint32_t size);
};
} // namespace lunalite::asset
