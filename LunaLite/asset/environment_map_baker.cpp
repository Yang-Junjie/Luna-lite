#include "../core/log.h"
#include "environment_map_baker.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <stb_image.h>

namespace lunalite::asset {
namespace {
constexpr uint32_t IrradianceThetaSamples = 16;
constexpr uint32_t IrradiancePhiSamples = 32;
constexpr uint32_t PrefilterSampleCount = 128;
constexpr float Pi = 3.14159265358979323846f;

struct Float4 {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};
};

Float4 readPixel(const HdrImage& image, uint32_t x, uint32_t y)
{
    const auto index = (static_cast<size_t>(y) * image.width + x) * 4;
    return {image.pixels[index], image.pixels[index + 1], image.pixels[index + 2], image.pixels[index + 3]};
}

Float4 lerp(const Float4& lhs, const Float4& rhs, float t)
{
    return {
        lhs.r + (rhs.r - lhs.r) * t,
        lhs.g + (rhs.g - lhs.g) * t,
        lhs.b + (rhs.b - lhs.b) * t,
        lhs.a + (rhs.a - lhs.a) * t,
    };
}

Float4 sampleEquirectangular(const HdrImage& image, float u, float v)
{
    u = u - std::floor(u);
    v = std::clamp(v, 0.0f, 1.0f);

    const auto sourceX = u * static_cast<float>(image.width) - 0.5f;
    const auto sourceY = v * static_cast<float>(image.height) - 0.5f;
    const auto x0 = static_cast<int>(std::floor(sourceX));
    const auto y0 = static_cast<int>(std::floor(sourceY));
    const auto tx = sourceX - static_cast<float>(x0);
    const auto ty = sourceY - static_cast<float>(y0);

    const auto wrapX = [&](int x) {
        const auto width = static_cast<int>(image.width);
        return static_cast<uint32_t>((x % width + width) % width);
    };
    const auto clampY = [&](int y) {
        return static_cast<uint32_t>(std::clamp(y, 0, static_cast<int>(image.height) - 1));
    };

    const auto c00 = readPixel(image, wrapX(x0), clampY(y0));
    const auto c10 = readPixel(image, wrapX(x0 + 1), clampY(y0));
    const auto c01 = readPixel(image, wrapX(x0), clampY(y0 + 1));
    const auto c11 = readPixel(image, wrapX(x0 + 1), clampY(y0 + 1));
    return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

std::array<float, 3> normalize(std::array<float, 3> value)
{
    const auto length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
    if (length <= 0.0f) {
        return {0.0f, 0.0f, 1.0f};
    }

    return {value[0] / length, value[1] / length, value[2] / length};
}

std::array<float, 3> cubeDirection(LunaCubeFace face, float u, float v)
{
    switch (face) {
        case LunaCubeFace::PositiveX:
            return normalize({1.0f, -v, -u});
        case LunaCubeFace::NegativeX:
            return normalize({-1.0f, -v, u});
        case LunaCubeFace::PositiveY:
            return normalize({u, 1.0f, v});
        case LunaCubeFace::NegativeY:
            return normalize({u, -1.0f, -v});
        case LunaCubeFace::PositiveZ:
            return normalize({u, -v, 1.0f});
        case LunaCubeFace::NegativeZ:
            return normalize({-u, -v, -1.0f});
    }

    return normalize({0.0f, 0.0f, 1.0f});
}

uint32_t fullMipLevelCount(uint32_t size)
{
    uint32_t levels = 1;
    while (size > 1) {
        size >>= 1;
        ++levels;
    }

    return levels;
}

glm::vec3 cubemapDirection(uint32_t face, uint32_t x, uint32_t y, uint32_t size)
{
    const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
    const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;

    switch (face) {
        case 0:
            return glm::normalize(glm::vec3{1.0f, -v, -u});
        case 1:
            return glm::normalize(glm::vec3{-1.0f, -v, u});
        case 2:
            return glm::normalize(glm::vec3{u, 1.0f, v});
        case 3:
            return glm::normalize(glm::vec3{u, -1.0f, -v});
        case 4:
            return glm::normalize(glm::vec3{u, -v, 1.0f});
        case 5:
        default:
            return glm::normalize(glm::vec3{-u, -v, -1.0f});
    }
}

glm::vec3 readCubePixel(const LunaCubeImage& cube, uint32_t face, uint32_t x, uint32_t y)
{
    const auto* faceData = getLunaCubeFaceData(cube, 0, face);
    if (faceData == nullptr) {
        return {0.0f, 0.0f, 0.0f};
    }

    const auto index = (static_cast<size_t>(y) * cube.size + x) * 4 * sizeof(float);
    std::array<float, 4> pixel{};
    std::memcpy(pixel.data(), faceData + index, sizeof(pixel));
    return {pixel[0], pixel[1], pixel[2]};
}

glm::vec3 sampleCubeColor(const LunaCubeImage& cube, const glm::vec3& direction)
{
    const auto normalized = glm::normalize(direction);
    const auto absDirection = glm::abs(normalized);

    uint32_t face = 0;
    float u = 0.0f;
    float v = 0.0f;
    if (absDirection.x >= absDirection.y && absDirection.x >= absDirection.z) {
        if (normalized.x >= 0.0f) {
            face = static_cast<uint32_t>(LunaCubeFace::PositiveX);
            u = -normalized.z / absDirection.x;
            v = -normalized.y / absDirection.x;
        } else {
            face = static_cast<uint32_t>(LunaCubeFace::NegativeX);
            u = normalized.z / absDirection.x;
            v = -normalized.y / absDirection.x;
        }
    } else if (absDirection.y >= absDirection.z) {
        if (normalized.y >= 0.0f) {
            face = static_cast<uint32_t>(LunaCubeFace::PositiveY);
            u = normalized.x / absDirection.y;
            v = normalized.z / absDirection.y;
        } else {
            face = static_cast<uint32_t>(LunaCubeFace::NegativeY);
            u = normalized.x / absDirection.y;
            v = -normalized.z / absDirection.y;
        }
    } else {
        if (normalized.z >= 0.0f) {
            face = static_cast<uint32_t>(LunaCubeFace::PositiveZ);
            u = normalized.x / absDirection.z;
            v = -normalized.y / absDirection.z;
        } else {
            face = static_cast<uint32_t>(LunaCubeFace::NegativeZ);
            u = -normalized.x / absDirection.z;
            v = -normalized.y / absDirection.z;
        }
    }

    const auto texX = (u * 0.5f + 0.5f) * static_cast<float>(cube.size) - 0.5f;
    const auto texY = (v * 0.5f + 0.5f) * static_cast<float>(cube.size) - 0.5f;
    const auto x0 = static_cast<int>(std::floor(texX));
    const auto y0 = static_cast<int>(std::floor(texY));
    const auto tx = texX - static_cast<float>(x0);
    const auto ty = texY - static_cast<float>(y0);
    const auto clampCoord = [&](int value) {
        return static_cast<uint32_t>(std::clamp(value, 0, static_cast<int>(cube.size) - 1));
    };

    const auto c00 = readCubePixel(cube, face, clampCoord(x0), clampCoord(y0));
    const auto c10 = readCubePixel(cube, face, clampCoord(x0 + 1), clampCoord(y0));
    const auto c01 = readCubePixel(cube, face, clampCoord(x0), clampCoord(y0 + 1));
    const auto c11 = readCubePixel(cube, face, clampCoord(x0 + 1), clampCoord(y0 + 1));
    return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
}

void basisFromNormal(const glm::vec3& normal, glm::vec3& right, glm::vec3& up)
{
    up = std::abs(normal.y) < 0.999f ? glm::vec3{0.0f, 1.0f, 0.0f} : glm::vec3{1.0f, 0.0f, 0.0f};
    right = glm::normalize(glm::cross(up, normal));
    up = glm::normalize(glm::cross(normal, right));
}

float radicalInverseVdC(uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55'55'55'55u) << 1u) | ((bits & 0xAA'AA'AA'AAu) >> 1u);
    bits = ((bits & 0x33'33'33'33u) << 2u) | ((bits & 0xCC'CC'CC'CCu) >> 2u);
    bits = ((bits & 0x0F'0F'0F'0Fu) << 4u) | ((bits & 0xF0'F0'F0'F0u) >> 4u);
    bits = ((bits & 0x00'FF'00'FFu) << 8u) | ((bits & 0xFF'00'FF'00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

glm::vec2 hammersley(uint32_t index, uint32_t count)
{
    return {static_cast<float>(index) / static_cast<float>(count), radicalInverseVdC(index)};
}

glm::vec3 importanceSampleGGX(const glm::vec2& xi, float roughness, const glm::vec3& normal)
{
    const float alpha = roughness * roughness;
    const float phi = 2.0f * Pi * xi.x;
    const float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (alpha * alpha - 1.0f) * xi.y));
    const float sinTheta = std::sqrt(std::max(1.0f - cosTheta * cosTheta, 0.0f));

    const glm::vec3 tangentHalf{
        std::cos(phi) * sinTheta,
        std::sin(phi) * sinTheta,
        cosTheta,
    };

    glm::vec3 right;
    glm::vec3 up;
    basisFromNormal(normal, right, up);
    return glm::normalize(right * tangentHalf.x + up * tangentHalf.y + normal * tangentHalf.z);
}

std::vector<float> makeIrradianceFace(const LunaCubeImage& cube, uint32_t face, uint32_t size)
{
    std::vector<float> facePixels(static_cast<size_t>(size) * size * 4);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const auto normal = cubemapDirection(face, x, y, size);
            glm::vec3 right;
            glm::vec3 up;
            basisFromNormal(normal, right, up);

            glm::vec3 irradiance{0.0f};
            for (uint32_t phiIndex = 0; phiIndex < IrradiancePhiSamples; ++phiIndex) {
                const float phi =
                    (static_cast<float>(phiIndex) + 0.5f) * 2.0f * Pi / static_cast<float>(IrradiancePhiSamples);
                for (uint32_t thetaIndex = 0; thetaIndex < IrradianceThetaSamples; ++thetaIndex) {
                    const float theta = (static_cast<float>(thetaIndex) + 0.5f) * 0.5f * Pi /
                                        static_cast<float>(IrradianceThetaSamples);
                    const float sinTheta = std::sin(theta);
                    const float cosTheta = std::cos(theta);
                    const glm::vec3 sampleDirection = glm::normalize(
                        right * (std::cos(phi) * sinTheta) + up * (std::sin(phi) * sinTheta) + normal * cosTheta);
                    irradiance += sampleCubeColor(cube, sampleDirection) * cosTheta * sinTheta;
                }
            }

            irradiance *= Pi / static_cast<float>(IrradiancePhiSamples * IrradianceThetaSamples);
            auto* destination = &facePixels[(static_cast<size_t>(y) * size + x) * 4];
            destination[0] = irradiance.r;
            destination[1] = irradiance.g;
            destination[2] = irradiance.b;
            destination[3] = 1.0f;
        }
    }
    return facePixels;
}

std::vector<float> makePrefilterFace(const LunaCubeImage& cube, uint32_t face, uint32_t size, float roughness)
{
    std::vector<float> facePixels(static_cast<size_t>(size) * size * 4);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const auto normal = cubemapDirection(face, x, y, size);
            const auto view = normal;
            glm::vec3 prefilteredColor{0.0f};
            float totalWeight = 0.0f;

            for (uint32_t sampleIndex = 0; sampleIndex < PrefilterSampleCount; ++sampleIndex) {
                const auto xi = hammersley(sampleIndex, PrefilterSampleCount);
                const auto halfVector = importanceSampleGGX(xi, roughness, normal);
                const auto light = glm::normalize(2.0f * glm::dot(view, halfVector) * halfVector - view);
                const float noL = std::max(glm::dot(normal, light), 0.0f);
                if (noL > 0.0f) {
                    prefilteredColor += sampleCubeColor(cube, light) * noL;
                    totalWeight += noL;
                }
            }

            if (totalWeight > 0.0f) {
                prefilteredColor /= totalWeight;
            }

            auto* destination = &facePixels[(static_cast<size_t>(y) * size + x) * 4];
            destination[0] = prefilteredColor.r;
            destination[1] = prefilteredColor.g;
            destination[2] = prefilteredColor.b;
            destination[3] = 1.0f;
        }
    }
    return facePixels;
}
} // namespace

std::optional<HdrImage> EnvironmentMapBaker::loadHdrImage(const std::filesystem::path& asset_path)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    float* rawPixels = stbi_loadf(asset_path.string().c_str(), &width, &height, &channels, 4);
    if (rawPixels == nullptr) {
        LUNA_CORE_ERROR("Failed to load HDR texture '{}': {}", asset_path.string(), stbi_failure_reason());
        return std::nullopt;
    }

    if (width <= 0 || height <= 0) {
        stbi_image_free(rawPixels);
        LUNA_CORE_ERROR(
            "Failed to load HDR texture '{}': invalid dimensions {}x{}", asset_path.string(), width, height);
        return std::nullopt;
    }

    HdrImage image;
    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);
    image.pixels.assign(rawPixels, rawPixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    stbi_image_free(rawPixels);
    return image;
}

std::optional<LunaCubeImage> EnvironmentMapBaker::bakeEnvironmentCube(const HdrImage& hdr_image, uint32_t cubemap_size)
{
    auto cube = createLunaCubeImage(LunaCubeFormat::RGBA32F, cubemap_size, 1);
    if (!cube) {
        return std::nullopt;
    }

    std::vector<float> output(cube->pixels.size() / sizeof(float));
    for (uint32_t faceIndex = 0; faceIndex < LunaCubeFaceCount; ++faceIndex) {
        const auto face = static_cast<LunaCubeFace>(faceIndex);
        for (uint32_t y = 0; y < cubemap_size; ++y) {
            for (uint32_t x = 0; x < cubemap_size; ++x) {
                const auto u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(cubemap_size)) - 1.0f;
                const auto v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(cubemap_size)) - 1.0f;
                const auto direction = cubeDirection(face, u, v);
                const auto longitude = std::atan2(direction[2], direction[0]);
                const auto latitude = std::asin(std::clamp(direction[1], -1.0f, 1.0f));
                const auto sampleU = longitude / (2.0f * Pi) + 0.5f;
                const auto sampleV = 0.5f - latitude / Pi;
                const auto sample = sampleEquirectangular(hdr_image, sampleU, sampleV);

                const auto index = ((static_cast<size_t>(faceIndex) * cubemap_size * cubemap_size) +
                                    (static_cast<size_t>(y) * cubemap_size + x)) *
                                   4;
                output[index] = sample.r;
                output[index + 1] = sample.g;
                output[index + 2] = sample.b;
                output[index + 3] = sample.a;
            }
        }
    }

    std::memcpy(cube->pixels.data(), output.data(), cube->pixels.size());
    return cube;
}

std::optional<LunaCubeImage> EnvironmentMapBaker::bakeIrradianceCube(const LunaCubeImage& environment_cube,
                                                                     uint32_t size)
{
    auto irradianceCube = createLunaCubeImage(LunaCubeFormat::RGBA32F, size, 1);
    if (!irradianceCube) {
        return std::nullopt;
    }

    for (uint32_t face = 0; face < LunaCubeFaceCount; ++face) {
        auto* faceData = getLunaCubeFaceData(*irradianceCube, 0, face);
        if (faceData == nullptr) {
            return std::nullopt;
        }

        const auto facePixels = makeIrradianceFace(environment_cube, face, size);
        std::memcpy(faceData, facePixels.data(), facePixels.size() * sizeof(float));
    }

    return irradianceCube;
}

std::optional<LunaCubeImage> EnvironmentMapBaker::bakePrefilterCube(const LunaCubeImage& environment_cube,
                                                                    uint32_t size)
{
    const auto mipCount = fullMipLevelCount(size);
    auto prefilterCube = createLunaCubeImage(LunaCubeFormat::RGBA32F, size, mipCount);
    if (!prefilterCube) {
        return std::nullopt;
    }

    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        const uint32_t mipSize = getLunaCubeMipSize(*prefilterCube, mip);
        const float roughness = mipCount > 1 ? static_cast<float>(mip) / static_cast<float>(mipCount - 1) : 0.0f;
        for (uint32_t face = 0; face < LunaCubeFaceCount; ++face) {
            auto* faceData = getLunaCubeFaceData(*prefilterCube, mip, face);
            if (faceData == nullptr) {
                return std::nullopt;
            }

            const auto facePixels = makePrefilterFace(environment_cube, face, mipSize, roughness);
            std::memcpy(faceData, facePixels.data(), facePixels.size() * sizeof(float));
        }
    }

    return prefilterCube;
}
} // namespace lunalite::asset
