#include "../../core/log.h"
#include "../asset_cache.h"
#include "../environment_map_baker.h"
#include "../lunacube.h"
#include "texture_asset_importer.h"

#include <cctype>
#include <cstdint>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace lunalite::asset {
namespace {
constexpr uint32_t DefaultEnvironmentCubemapSize = 1'024;
constexpr uint32_t DefaultIrradianceCubeSize = 32;
constexpr uint32_t DefaultPrefilterCubeSize = 128;

struct EnvironmentImportSettings {
    uint32_t cubemap_size{DefaultEnvironmentCubemapSize};
    uint32_t irradiance_size{DefaultIrradianceCubeSize};
    uint32_t prefilter_size{DefaultPrefilterCubeSize};
};

struct EnvironmentArtifactPaths {
    std::filesystem::path environment_cube;
    std::filesystem::path irradiance_cube;
    std::filesystem::path prefilter_cube;
};

bool isHdrTexture(const std::filesystem::path& assetPath)
{
    auto extension = assetPath.extension().string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".hdr";
}

std::optional<std::string> calculateSourceHash(const std::filesystem::path& assetPath)
{
    std::ifstream in(assetPath, std::ios::binary);
    if (!in.is_open()) {
        return std::nullopt;
    }

    uint64_t hash = 14'695'981'039'346'656'037ull;
    std::array<char, 4'096> buffer{};
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto readCount = in.gcount();
        for (std::streamsize i = 0; i < readCount; ++i) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<size_t>(i)]);
            hash *= 1'099'511'628'211ull;
        }
    }

    if (in.bad()) {
        return std::nullopt;
    }

    return std::to_string(hash);
}

uint32_t readEnvironmentSize(const YAML::Node& config, const char* key, uint32_t defaultSize)
{
    const auto environment = config["Environment"];
    if (!environment || !environment[key]) {
        return defaultSize;
    }

    const auto size = environment[key].as<uint32_t>(defaultSize);
    return size == 0 ? defaultSize : size;
}

EnvironmentImportSettings readEnvironmentImportSettings(const YAML::Node& config)
{
    return EnvironmentImportSettings{
        .cubemap_size = readEnvironmentSize(config, "CubemapSize", DefaultEnvironmentCubemapSize),
        .irradiance_size = readEnvironmentSize(config, "IrradianceSize", DefaultIrradianceCubeSize),
        .prefilter_size = readEnvironmentSize(config, "PrefilterSize", DefaultPrefilterCubeSize),
    };
}

EnvironmentArtifactPaths getEnvironmentArtifactPaths(AssetHandle handle)
{
    return EnvironmentArtifactPaths{
        .environment_cube = cache::getImportedAssetArtifactPath(handle, "environment.lunacube"),
        .irradiance_cube = cache::getImportedAssetArtifactPath(handle, "irradiance.lunacube"),
        .prefilter_cube = cache::getImportedAssetArtifactPath(handle, "prefilter.lunacube"),
    };
}

bool environmentArtifactUpToDate(const YAML::Node& config,
                                 const std::string& sourceHash,
                                 const EnvironmentImportSettings& settings,
                                 const EnvironmentArtifactPaths& paths)
{
    const auto environment = config["Environment"];
    if (!environment) {
        return false;
    }

    const auto artifacts = environment["Artifacts"];
    const auto environmentCube = artifacts ? artifacts["EnvironmentCube"].as<std::string>("") : "";
    const auto irradianceCube = artifacts ? artifacts["IrradianceCube"].as<std::string>("") : "";
    const auto prefilterCube = artifacts ? artifacts["PrefilterCube"].as<std::string>("") : "";
    return environment["Type"].as<std::string>("") == "EquirectangularHDR" &&
           environment["SourceHash"].as<std::string>("") == sourceHash &&
           environment["CubemapSize"].as<uint32_t>(0) == settings.cubemap_size &&
           environment["IrradianceSize"].as<uint32_t>(0) == settings.irradiance_size &&
           environment["PrefilterSize"].as<uint32_t>(0) == settings.prefilter_size &&
           environmentCube == paths.environment_cube.generic_string() &&
           irradianceCube == paths.irradiance_cube.generic_string() &&
           prefilterCube == paths.prefilter_cube.generic_string() &&
           std::filesystem::exists(cache::resolveProjectPath(paths.environment_cube)) &&
           std::filesystem::exists(cache::resolveProjectPath(paths.irradiance_cube)) &&
           std::filesystem::exists(cache::resolveProjectPath(paths.prefilter_cube));
}

void writeEnvironmentConfig(YAML::Node& config,
                            const std::string& sourceHash,
                            const EnvironmentImportSettings& settings,
                            const EnvironmentArtifactPaths& paths)
{
    YAML::Node environment = config["Environment"];
    environment["Type"] = "EquirectangularHDR";
    environment["SourceHash"] = sourceHash;
    environment["CubemapSize"] = settings.cubemap_size;
    environment["IrradianceSize"] = settings.irradiance_size;
    environment["PrefilterSize"] = settings.prefilter_size;

    YAML::Node artifacts;
    artifacts["EnvironmentCube"] = paths.environment_cube.generic_string();
    artifacts["IrradianceCube"] = paths.irradiance_cube.generic_string();
    artifacts["PrefilterCube"] = paths.prefilter_cube.generic_string();
    environment["Artifacts"] = artifacts;
    config["Environment"] = environment;
}

std::optional<LunaCubeImage> readCachedEnvironmentCube(const YAML::Node& config,
                                                       const std::string& sourceHash,
                                                       uint32_t cubemapSize,
                                                       const std::filesystem::path& artifactPath)
{
    const auto environment = config["Environment"];
    const auto artifacts = environment ? environment["Artifacts"] : YAML::Node{};
    const auto environmentCube = artifacts ? artifacts["EnvironmentCube"].as<std::string>("") : "";
    if (!environment || environment["SourceHash"].as<std::string>("") != sourceHash ||
        environment["CubemapSize"].as<uint32_t>(0) != cubemapSize || environmentCube != artifactPath.generic_string()) {
        return std::nullopt;
    }

    auto cube = readLunaCube(cache::resolveProjectPath(artifactPath));
    if (!cube || cube->format != LunaCubeFormat::RGBA32F || cube->size != cubemapSize || cube->mip_count != 1 ||
        cube->pixels.empty()) {
        return std::nullopt;
    }

    return cube;
}

bool generateEnvironmentArtifact(const std::filesystem::path& assetPath, AssetMetadata& metadata)
{
    if (!isHdrTexture(assetPath)) {
        return true;
    }

    const auto sourceHash = calculateSourceHash(assetPath);
    if (!sourceHash) {
        LUNA_CORE_ERROR("Failed to hash HDR texture '{}'", assetPath.string());
        return false;
    }

    const auto settings = readEnvironmentImportSettings(metadata.SpecializedConfig);
    const auto artifactPaths = getEnvironmentArtifactPaths(metadata.Handle);
    if (environmentArtifactUpToDate(metadata.SpecializedConfig, *sourceHash, settings, artifactPaths)) {
        writeEnvironmentConfig(metadata.SpecializedConfig, *sourceHash, settings, artifactPaths);
        return true;
    }

    auto environmentCube = readCachedEnvironmentCube(
        metadata.SpecializedConfig, *sourceHash, settings.cubemap_size, artifactPaths.environment_cube);
    if (!environmentCube) {
        const auto hdrImage = EnvironmentMapBaker::loadHdrImage(assetPath);
        if (!hdrImage) {
            return false;
        }

        environmentCube = EnvironmentMapBaker::bakeEnvironmentCube(*hdrImage, settings.cubemap_size);
        if (!environmentCube) {
            LUNA_CORE_ERROR("Failed to bake HDR environment cubemap '{}'", assetPath.string());
            return false;
        }

        if (!writeLunaCube(cache::resolveProjectPath(artifactPaths.environment_cube), *environmentCube)) {
            LUNA_CORE_ERROR("Failed to write HDR environment cubemap '{}'",
                            cache::resolveProjectPath(artifactPaths.environment_cube).string());
            return false;
        }
    }

    const auto irradianceCube = EnvironmentMapBaker::bakeIrradianceCube(*environmentCube, settings.irradiance_size);
    if (!irradianceCube) {
        LUNA_CORE_ERROR("Failed to bake HDR irradiance cubemap '{}'", assetPath.string());
        return false;
    }
    if (!writeLunaCube(cache::resolveProjectPath(artifactPaths.irradiance_cube), *irradianceCube)) {
        LUNA_CORE_ERROR("Failed to write HDR irradiance cubemap '{}'",
                        cache::resolveProjectPath(artifactPaths.irradiance_cube).string());
        return false;
    }

    const auto prefilterCube = EnvironmentMapBaker::bakePrefilterCube(*environmentCube, settings.prefilter_size);
    if (!prefilterCube) {
        LUNA_CORE_ERROR("Failed to bake HDR prefilter cubemap '{}'", assetPath.string());
        return false;
    }
    if (!writeLunaCube(cache::resolveProjectPath(artifactPaths.prefilter_cube), *prefilterCube)) {
        LUNA_CORE_ERROR("Failed to write HDR prefilter cubemap '{}'",
                        cache::resolveProjectPath(artifactPaths.prefilter_cube).string());
        return false;
    }

    writeEnvironmentConfig(metadata.SpecializedConfig, *sourceHash, settings, artifactPaths);
    return true;
}

YAML::Node defaultTextureConfig(const std::filesystem::path& assetPath)
{
    const bool hdr = isHdrTexture(assetPath);

    YAML::Node config;
    config["GenerateMipmaps"] = true;
    config["ColorSpace"] = hdr ? "Linear" : "SRGB";

    YAML::Node sampler;
    sampler["MinFilter"] = "Linear";
    sampler["MagFilter"] = "Linear";
    sampler["MipFilter"] = "Linear";
    sampler["AddressU"] = "Repeat";
    sampler["AddressV"] = hdr ? "ClampToEdge" : "Repeat";
    sampler["AddressW"] = hdr ? "ClampToEdge" : "Repeat";
    config["Sampler"] = sampler;
    return config;
}
} // namespace

std::vector<AssetMetadata> TextureAssetImporter::import(const std::filesystem::path& assetPath)
{
    auto metadata = createMetadata(assetPath, AssetType::Texture);
    const auto metaPath = getMetaFilePath(metadata);

    if (std::filesystem::exists(metaPath)) {
        const auto oldMetadata = deserializeMetadata(metaPath);
        if (oldMetadata.Handle.isValid()) {
            metadata.Handle = oldMetadata.Handle;
        }
        metadata.MemoryOnly = oldMetadata.MemoryOnly;
        metadata.SpecializedConfig = oldMetadata.SpecializedConfig;
    }

    metadata.Type = AssetType::Texture;
    metadata.Name = assetPath.stem().string();
    metadata.FilePath = makeProjectRelative(assetPath);
    if (!hasSpecializedConfig(metadata.SpecializedConfig)) {
        metadata.SpecializedConfig = defaultTextureConfig(assetPath);
    }

    if (!generateEnvironmentArtifact(assetPath, metadata)) {
        return {};
    }

    if (!serializeMetadata(metadata)) {
        return {};
    }

    return {metadata};
}

std::vector<std::string> TextureAssetImporter::getSupportedExtensions() const
{
    return {".png", ".jpg", ".jpeg", ".hdr"};
}

bool TextureAssetImporter::shouldRefreshExistingMetadata(const AssetMetadata&,
                                                         const std::filesystem::path& assetPath) const
{
    return isHdrTexture(assetPath);
}

} // namespace lunalite::asset
