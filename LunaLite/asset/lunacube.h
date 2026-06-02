#pragma once

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <optional>
#include <vector>

namespace lunalite::asset {
enum class LunaCubeFormat : uint32_t {
    RGBA32F = 1,
};

enum class LunaCubeFace : uint32_t {
    PositiveX = 0,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ,
};

constexpr uint32_t LunaCubeVersion = 1;
constexpr uint32_t LunaCubeFaceCount = 6;

struct LunaCubeImage {
    LunaCubeFormat format{LunaCubeFormat::RGBA32F};
    uint32_t size{0};
    uint32_t mip_count{0};
    // Payload order is mip-major, then LunaCubeFace order. Each face is tightly packed RGBA data.
    std::vector<uint8_t> pixels;
};

uint32_t getLunaCubeBytesPerPixel(LunaCubeFormat format);
uint32_t getLunaCubeMipSize(const LunaCubeImage& image, uint32_t mip);
std::optional<LunaCubeImage> createLunaCubeImage(LunaCubeFormat format, uint32_t size, uint32_t mip_count);
std::optional<size_t> calculateLunaCubePayloadSize(LunaCubeFormat format, uint32_t size, uint32_t mip_count);
std::optional<size_t> calculateLunaCubeFaceOffsetBytes(const LunaCubeImage& image, uint32_t mip, uint32_t face);
const uint8_t* getLunaCubeFaceData(const LunaCubeImage& image, uint32_t mip, uint32_t face);
uint8_t* getLunaCubeFaceData(LunaCubeImage& image, uint32_t mip, uint32_t face);
bool writeLunaCube(const std::filesystem::path& path, const LunaCubeImage& image);
std::optional<LunaCubeImage> readLunaCube(const std::filesystem::path& path);
} // namespace lunalite::asset
