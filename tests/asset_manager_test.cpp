#include "../LunaLite/asset/asset_cache.h"
#include "../LunaLite/asset/asset_manager.h"
#include "../LunaLite/asset/builtin/builtin_assets.h"
#include "../LunaLite/asset/lunacube.h"
#include "../LunaLite/project/project_manager.h"
#include "../LunaLite/renderer/interface/material.h"
#include "../LunaLite/renderer/interface/mesh.h"
#include "../LunaLite/renderer/interface/model.h"
#include "../LunaLite/renderer/interface/texture.h"
#include "../third_party/stb/stb_image_write.h"

#include <cmath>
#include <cstdint>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
std::filesystem::path findTestMesh()
{
    const std::filesystem::path candidates[] = {
        "sample_project/Assets/Meshs/cube.obj",
        "../sample_project/Assets/Meshs/cube.obj",
        "../../sample_project/Assets/Meshs/cube.obj",
        "sample_project/Assets/cube.obj",
        "../sample_project/Assets/cube.obj",
        "../../sample_project/Assets/cube.obj",
        "assets/cube.obj",
        "../assets/cube.obj",
        "../../assets/cube.obj",
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return candidates[0];
}

std::filesystem::path findTestTexture()
{
    const std::filesystem::path candidates[] = {
        "sample_project/Assets/Models/texture.png",
        "../sample_project/Assets/Models/texture.png",
        "../../sample_project/Assets/Models/texture.png",
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return candidates[0];
}

template <typename T, std::size_t Size> void writeArray(std::ofstream& out, const std::array<T, Size>& values)
{
    out.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(T)));
}

bool nearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= 0.001f;
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

bool writeGltfTriangle(const std::filesystem::path& gltfPath,
                       const std::filesystem::path& texturePath,
                       const std::filesystem::path& occlusionTexturePath)
{
    const auto bufferPath = gltfPath.parent_path() / "triangle.bin";
    std::ofstream buffer(bufferPath, std::ios::binary);
    if (!buffer.is_open()) {
        return false;
    }

    constexpr std::array<float, 9> positions{
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
    };
    constexpr std::array<float, 6> texcoords{
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
    };
    constexpr std::array<uint16_t, 3> indices{0, 1, 2};

    writeArray(buffer, positions);
    writeArray(buffer, texcoords);
    writeArray(buffer, indices);
    if (!buffer.good()) {
        return false;
    }

    std::ofstream gltf(gltfPath);
    if (!gltf.is_open()) {
        return false;
    }

    gltf << R"({
  "asset": { "version": "2.0" },
  "extensionsUsed": ["KHR_materials_emissive_strength", "KHR_materials_unlit"],
  "buffers": [
    { "uri": "triangle.bin", "byteLength": 66 }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 24, "target": 34962 },
    { "buffer": 0, "byteOffset": 60, "byteLength": 6, "target": 34963 }
  ],
  "accessors": [
    {
      "bufferView": 0,
      "componentType": 5126,
      "count": 3,
      "type": "VEC3",
      "min": [0.0, 0.0, 0.0],
      "max": [1.0, 1.0, 0.0]
    },
    { "bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2" },
    { "bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ],
  "samplers": [
    { "minFilter": 9985, "magFilter": 9728, "wrapS": 33071, "wrapT": 33648 }
  ],
  "images": [
    { "uri": ")"
         << texturePath.filename().generic_string() << R"(" },
    { "uri": ")"
         << occlusionTexturePath.filename().generic_string() << R"(" }
  ],
  "textures": [
    { "sampler": 0, "source": 0 },
    { "sampler": 0, "source": 1 }
  ],
  "materials": [
    {
      "name": "GltfMaterial",
      "pbrMetallicRoughness": {
        "baseColorFactor": [0.25, 0.5, 0.75, 0.9],
        "metallicFactor": 0.3,
        "roughnessFactor": 0.7,
        "baseColorTexture": { "index": 0 }
      },
      "normalTexture": { "index": 1, "scale": 0.6 },
      "occlusionTexture": { "index": 1, "strength": 0.35 },
      "emissiveFactor": [0.1, 0.2, 0.3],
      "emissiveTexture": { "index": 0 },
      "extensions": {
        "KHR_materials_emissive_strength": { "emissiveStrength": 2.5 },
        "KHR_materials_unlit": {}
      }
    }
  ],
  "meshes": [
    {
      "name": "GltfMesh",
      "primitives": [
        { "attributes": { "POSITION": 0, "TEXCOORD_0": 1 }, "indices": 2, "material": 0 }
      ]
    }
  ],
  "nodes": [
    { "mesh": 0 }
  ],
  "scenes": [
    { "nodes": [0] }
  ],
  "scene": 0
})";

    return gltf.good();
}
} // namespace

int main()
{
    using namespace lunalite;

    const auto sourceMesh = findTestMesh();
    if (!std::filesystem::exists(sourceMesh)) {
        std::cerr << "Failed to find test mesh.\n";
        return 1;
    }
    const auto sourceTexture = findTestTexture();
    if (!std::filesystem::exists(sourceTexture)) {
        std::cerr << "Failed to find test texture.\n";
        return 1;
    }

    const auto projectRoot = std::filesystem::current_path() / "build" / "asset_manager_test_project";
    std::error_code error;
    std::filesystem::remove_all(projectRoot, error);

    project::ProjectInfo info;
    info.name = "AssetManagerTestProject";
    info.assets_path = "GameAssets";
    if (!project::ProjectManager::instance().createProject(projectRoot, info)) {
        std::cerr << "Failed to create test project.\n";
        return 1;
    }

    const auto targetMesh = projectRoot / info.assets_path / "cube.obj";
    std::filesystem::copy_file(sourceMesh, targetMesh, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::cerr << "Failed to copy test mesh: " << error.message() << "\n";
        return 1;
    }

    const auto textureHandle = asset::AssetHandle{123'456'789ull};
    const auto targetTexture = projectRoot / info.assets_path / "texture.png";
    std::filesystem::copy_file(sourceTexture, targetTexture, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::cerr << "Failed to copy test texture: " << error.message() << "\n";
        return 1;
    }
    {
        std::ofstream textureMetadata(targetTexture.string() + ".lunameta");
        textureMetadata << "Asset:\n";
        textureMetadata << "  Handle: " << static_cast<uint64_t>(textureHandle) << "\n";
        textureMetadata << "  Type: Texture\n";
        textureMetadata << "  Name: texture\n";
        textureMetadata << "  FilePath: GameAssets/texture.png\n";
        textureMetadata << "  MemoryOnly: false\n";
        textureMetadata << "  Config:\n";
        textureMetadata << "    GenerateMipmaps: false\n";
        textureMetadata << "    ColorSpace: Linear\n";
        textureMetadata << "    Sampler:\n";
        textureMetadata << "      MinFilter: Nearest\n";
        textureMetadata << "      MagFilter: Linear\n";
        textureMetadata << "      MipFilter: None\n";
        textureMetadata << "      AddressU: ClampToEdge\n";
        textureMetadata << "      AddressV: MirroredRepeat\n";
        textureMetadata << "      AddressW: Repeat\n";
    }

    const auto targetHdrTexture = projectRoot / info.assets_path / "environment.hdr";
    const auto expectedHdrTextureHandle = asset::AssetHandle{987'654'321ull};
    const float hdrPixels[] = {
        1.0f,
        0.5f,
        0.25f,
        0.25f,
        0.5f,
        1.0f,
    };
    if (stbi_write_hdr(targetHdrTexture.string().c_str(), 2, 1, 3, hdrPixels) == 0) {
        std::cerr << "Failed to write HDR test texture.\n";
        return 1;
    }
    {
        std::ofstream hdrMetadata(targetHdrTexture.string() + ".lunameta");
        hdrMetadata << "Asset:\n";
        hdrMetadata << "  Handle: " << static_cast<uint64_t>(expectedHdrTextureHandle) << "\n";
        hdrMetadata << "  Type: Texture\n";
        hdrMetadata << "  Name: environment\n";
        hdrMetadata << "  FilePath: GameAssets/environment.hdr\n";
        hdrMetadata << "  MemoryOnly: false\n";
        hdrMetadata << "  Config:\n";
        hdrMetadata << "    GenerateMipmaps: true\n";
        hdrMetadata << "    ColorSpace: Linear\n";
        hdrMetadata << "    Sampler:\n";
        hdrMetadata << "      MinFilter: Linear\n";
        hdrMetadata << "      MagFilter: Linear\n";
        hdrMetadata << "      MipFilter: Linear\n";
        hdrMetadata << "      AddressU: Repeat\n";
        hdrMetadata << "      AddressV: ClampToEdge\n";
        hdrMetadata << "      AddressW: ClampToEdge\n";
        hdrMetadata << "    Environment:\n";
        hdrMetadata << "      CubemapSize: 4\n";
        hdrMetadata << "      IrradianceSize: 4\n";
        hdrMetadata << "      PrefilterSize: 4\n";
    }

    const auto texturedMaterialPath = projectRoot / info.assets_path / "Textured.lunamat";
    {
        std::ofstream material(texturedMaterialPath);
        material << "Material:\n";
        material << "  ShadingModel: Lit\n";
        material << "  Albedo: [1.0, 1.0, 1.0, 1.0]\n";
        material << "  Metallic: 0.0\n";
        material << "  Roughness: 0.5\n";
        material << "  Emission: [1.0, 1.0, 1.0]\n";
        material << "  EmissionStrength: 2.0\n";
        material << "  NormalScale: 0.75\n";
        material << "  OcclusionStrength: 0.5\n";
        material << "  Textures:\n";
        material << "    Occlusion: " << static_cast<uint64_t>(textureHandle) << "\n";
        material << "    Emission: " << static_cast<uint64_t>(textureHandle) << "\n";
    }

    const auto gltfTexturePath = projectRoot / info.assets_path / "gltf_texture.png.asset";
    std::filesystem::copy_file(
        sourceTexture, gltfTexturePath, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::cerr << "Failed to copy glTF test texture: " << error.message() << "\n";
        return 1;
    }

    const auto gltfOcclusionTexturePath = projectRoot / info.assets_path / "gltf_occlusion.png.asset";
    std::filesystem::copy_file(
        sourceTexture, gltfOcclusionTexturePath, std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::cerr << "Failed to copy glTF occlusion test texture: " << error.message() << "\n";
        return 1;
    }

    const auto targetGltf = projectRoot / info.assets_path / "triangle.gltf";
    if (!writeGltfTriangle(targetGltf, gltfTexturePath, gltfOcclusionTexturePath)) {
        std::cerr << "Failed to write glTF test fixture.\n";
        return 1;
    }

    const auto ignoredAssetDir = projectRoot / "Assets";
    std::filesystem::create_directories(ignoredAssetDir, error);
    if (error) {
        std::cerr << "Failed to create ignored asset directory: " << error.message() << "\n";
        return 1;
    }
    std::filesystem::copy_file(
        sourceMesh, ignoredAssetDir / "ignored.obj", std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        std::cerr << "Failed to copy ignored test mesh: " << error.message() << "\n";
        return 1;
    }

    if (!asset::AssetManager::get().loadProjectAssets()) {
        std::cerr << "Failed to load project assets.\n";
        return 1;
    }

    const auto ignoredHandle = asset::AssetManager::get().getHandleByRelativePath("Assets/ignored.obj");
    if (ignoredHandle.isValid()) {
        std::cerr << "Imported an asset outside the configured assets path.\n";
        return 1;
    }

    const auto handle = asset::AssetManager::get().getHandleByRelativePath("GameAssets/cube.obj");
    if (!handle.isValid()) {
        std::cerr << "Failed to resolve imported mesh handle.\n";
        return 1;
    }

    const auto fileNameHandle = asset::AssetManager::get().getHandleByFileName("cube.obj");
    if (!fileNameHandle.isValid() || fileNameHandle != handle) {
        std::cerr << "Failed to resolve imported mesh by file name.\n";
        return 1;
    }

    const auto* mesh = asset::AssetManager::get().getAsset<renderer::interface::Mesh>(handle);
    if (mesh == nullptr || mesh->getSubMeshes().empty()) {
        std::cerr << "Failed to load mesh through AssetManager.\n";
        return 1;
    }
    const auto& submesh = mesh->getSubMeshes().front();
    if (submesh.getVertices().empty() || submesh.getIndices().empty()) {
        std::cerr << "Failed to load submesh geometry through AssetManager.\n";
        return 1;
    }

    const auto materialHandle = asset::AssetManager::get().getHandleByRelativePath("GameAssets/cube_Default.lunamat");
    const auto* material = asset::AssetManager::get().getAsset<renderer::interface::Material>(materialHandle);
    if (material == nullptr || material->parameters->albedo.r < 0.7f) {
        std::cerr << "Failed to load generated material through AssetManager.\n";
        return 1;
    }

    const auto* texture = asset::AssetManager::get().getAsset<renderer::interface::Texture>(textureHandle);
    if (texture == nullptr || texture->getWidth() == 0 || texture->getHeight() == 0 || texture->getPixels().empty()) {
        std::cerr << "Failed to load texture through AssetManager.\n";
        return 1;
    }
    const auto& textureSettings = texture->getImportSettings();
    if (textureSettings.generate_mipmaps ||
        textureSettings.color_space != renderer::interface::TextureColorSpace::Linear ||
        textureSettings.sampler.min_filter != renderer::interface::TextureFilter::Nearest ||
        textureSettings.sampler.mag_filter != renderer::interface::TextureFilter::Linear ||
        textureSettings.sampler.mip_filter != renderer::interface::TextureMipFilter::None ||
        textureSettings.sampler.address_u != renderer::interface::TextureAddressMode::ClampToEdge ||
        textureSettings.sampler.address_v != renderer::interface::TextureAddressMode::MirroredRepeat ||
        textureSettings.sampler.address_w != renderer::interface::TextureAddressMode::Repeat) {
        std::cerr << "Failed to load texture config through AssetManager.\n";
        return 1;
    }

    const auto hdrTextureHandle = asset::AssetManager::get().getHandleByRelativePath("GameAssets/environment.hdr");
    if (hdrTextureHandle != expectedHdrTextureHandle) {
        std::cerr << "Failed to preserve HDR texture metadata handle.\n";
        return 1;
    }
    const auto* hdrTexture = asset::AssetManager::get().getAsset<renderer::interface::Texture>(hdrTextureHandle);
    if (hdrTexture == nullptr || hdrTexture->getWidth() != 2 || hdrTexture->getHeight() != 1 ||
        hdrTexture->getFormat() != renderer::interface::TextureFormat::RGBA32F ||
        hdrTexture->getPixels().size() != 2 * 1 * 4 * sizeof(float) ||
        hdrTexture->getImportSettings().color_space != renderer::interface::TextureColorSpace::Linear) {
        std::cerr << "Failed to import and load HDR texture through AssetManager.\n";
        return 1;
    }
    const auto* hdrMetadata = asset::AssetManager::get().getMetadata(hdrTextureHandle);
    const auto environment = hdrMetadata != nullptr ? hdrMetadata->SpecializedConfig["Environment"] : YAML::Node{};
    const auto artifacts = environment ? environment["Artifacts"] : YAML::Node{};
    const auto environmentArtifact =
        environment && environment["Artifacts"] ? environment["Artifacts"]["EnvironmentCube"].as<std::string>("") : "";
    const auto irradianceArtifact = artifacts ? artifacts["IrradianceCube"].as<std::string>("") : "";
    const auto prefilterArtifact = artifacts ? artifacts["PrefilterCube"].as<std::string>("") : "";
    if (!environment || environment["Type"].as<std::string>("") != "EquirectangularHDR" ||
        environment["SourceHash"].as<std::string>("").empty() || environment["CubemapSize"].as<uint32_t>(0) != 4 ||
        environment["IrradianceSize"].as<uint32_t>(0) != 4 || environment["PrefilterSize"].as<uint32_t>(0) != 4 ||
        environmentArtifact.empty() || irradianceArtifact.empty() || prefilterArtifact.empty()) {
        std::cerr << "Failed to write HDR environment artifact metadata.\n";
        return 1;
    }
    const auto environmentCube = asset::readLunaCube(asset::cache::resolveProjectPath(environmentArtifact));
    const auto irradianceCube = asset::readLunaCube(asset::cache::resolveProjectPath(irradianceArtifact));
    const auto prefilterCube = asset::readLunaCube(asset::cache::resolveProjectPath(prefilterArtifact));
    const auto expectedEnvironmentCubePayloadSize =
        asset::calculateLunaCubePayloadSize(asset::LunaCubeFormat::RGBA32F, 4, 1);
    const auto expectedIrradianceCubePayloadSize =
        asset::calculateLunaCubePayloadSize(asset::LunaCubeFormat::RGBA32F, 4, 1);
    const auto expectedPrefilterMipCount = fullMipLevelCount(4);
    const auto expectedPrefilterCubePayloadSize =
        asset::calculateLunaCubePayloadSize(asset::LunaCubeFormat::RGBA32F, 4, expectedPrefilterMipCount);
    if (!environmentCube || !expectedEnvironmentCubePayloadSize ||
        environmentCube->format != asset::LunaCubeFormat::RGBA32F || environmentCube->size != 4 ||
        environmentCube->mip_count != 1 || environmentCube->pixels.size() != *expectedEnvironmentCubePayloadSize) {
        std::cerr << "Failed to write HDR environment .lunacube artifact.\n";
        return 1;
    }
    if (!irradianceCube || !expectedIrradianceCubePayloadSize ||
        irradianceCube->format != asset::LunaCubeFormat::RGBA32F || irradianceCube->size != 4 ||
        irradianceCube->mip_count != 1 || irradianceCube->pixels.size() != *expectedIrradianceCubePayloadSize) {
        std::cerr << "Failed to write HDR irradiance .lunacube artifact.\n";
        return 1;
    }
    if (!prefilterCube || !expectedPrefilterCubePayloadSize ||
        prefilterCube->format != asset::LunaCubeFormat::RGBA32F || prefilterCube->size != 4 ||
        prefilterCube->mip_count != expectedPrefilterMipCount ||
        prefilterCube->pixels.size() != *expectedPrefilterCubePayloadSize) {
        std::cerr << "Failed to write HDR prefilter .lunacube artifact.\n";
        return 1;
    }

    const auto texturedMaterialHandle =
        asset::AssetManager::get().getHandleByRelativePath("GameAssets/Textured.lunamat");
    const auto* texturedMaterial =
        asset::AssetManager::get().getAsset<renderer::interface::Material>(texturedMaterialHandle);
    if (texturedMaterial == nullptr || texturedMaterial->parameters->occlusion_texture != textureHandle ||
        texturedMaterial->parameters->emission_texture != textureHandle ||
        !nearlyEqual(texturedMaterial->parameters->normal_scale, 0.75f) ||
        !nearlyEqual(texturedMaterial->parameters->occlusion_strength, 0.5f)) {
        std::cerr << "Failed to load material texture scalar parameters through AssetManager.\n";
        return 1;
    }

    const auto gltfHandle = asset::AssetManager::get().getHandleByRelativePath("GameAssets/triangle.gltf");
    if (!gltfHandle.isValid()) {
        std::cerr << "Failed to resolve imported glTF mesh handle.\n";
        return 1;
    }

    const auto* gltfMesh = asset::AssetManager::get().getAsset<renderer::interface::Mesh>(gltfHandle);
    if (gltfMesh == nullptr || gltfMesh->getSubMeshes().size() != 1) {
        std::cerr << "Failed to load glTF mesh through AssetManager.\n";
        return 1;
    }
    const auto& gltfSubmesh = gltfMesh->getSubMeshes().front();
    if (gltfSubmesh.material_slot != 0 || gltfSubmesh.getVertices().size() != 3 ||
        gltfSubmesh.getIndices().size() != 3 || gltfSubmesh.getVertices().front().normal.z < 0.9f) {
        std::cerr << "Failed to load glTF primitive geometry through AssetManager.\n";
        return 1;
    }

    const auto gltfMaterialHandle =
        asset::AssetManager::get().getHandleByRelativePath("GameAssets/triangle_GltfMaterial.lunamat");
    const auto* gltfMaterial = asset::AssetManager::get().getAsset<renderer::interface::Material>(gltfMaterialHandle);
    if (gltfMaterial == nullptr || gltfMaterial->parameters == nullptr ||
        gltfMaterial->parameters->shading_model != renderer::interface::ShadingModel::Unlit ||
        !nearlyEqual(gltfMaterial->parameters->metallic, 0.3f) ||
        !nearlyEqual(gltfMaterial->parameters->roughness, 0.7f) ||
        !nearlyEqual(gltfMaterial->parameters->emission_strength, 2.5f) ||
        !nearlyEqual(gltfMaterial->parameters->normal_scale, 0.6f) ||
        !nearlyEqual(gltfMaterial->parameters->occlusion_strength, 0.35f)) {
        std::cerr << "Failed to load glTF material factors through AssetManager.\n";
        return 1;
    }

    const auto gltfTextureHandle =
        asset::AssetManager::get().getHandleByRelativePath("GameAssets/gltf_texture.png.asset");
    if (!gltfTextureHandle.isValid() || gltfMaterial->parameters->albedo_texture != gltfTextureHandle ||
        gltfMaterial->parameters->emission_texture != gltfTextureHandle) {
        std::cerr << "Failed to load glTF material texture handles through AssetManager.\n";
        return 1;
    }

    const auto gltfOcclusionTextureHandle =
        asset::AssetManager::get().getHandleByRelativePath("GameAssets/gltf_occlusion.png.asset");
    if (!gltfOcclusionTextureHandle.isValid() ||
        gltfMaterial->parameters->normal_texture != gltfOcclusionTextureHandle ||
        gltfMaterial->parameters->occlusion_texture != gltfOcclusionTextureHandle) {
        std::cerr << "Failed to load glTF material normal/occlusion texture handles through AssetManager.\n";
        return 1;
    }

    const auto* gltfTexture = asset::AssetManager::get().getAsset<renderer::interface::Texture>(gltfTextureHandle);
    if (gltfTexture == nullptr || gltfTexture->getPixels().empty()) {
        std::cerr << "Failed to load glTF companion texture through AssetManager.\n";
        return 1;
    }
    const auto& gltfTextureSettings = gltfTexture->getImportSettings();
    if (!gltfTextureSettings.generate_mipmaps ||
        gltfTextureSettings.color_space != renderer::interface::TextureColorSpace::SRGB ||
        gltfTextureSettings.sampler.min_filter != renderer::interface::TextureFilter::Linear ||
        gltfTextureSettings.sampler.mag_filter != renderer::interface::TextureFilter::Nearest ||
        gltfTextureSettings.sampler.mip_filter != renderer::interface::TextureMipFilter::Nearest ||
        gltfTextureSettings.sampler.address_u != renderer::interface::TextureAddressMode::ClampToEdge ||
        gltfTextureSettings.sampler.address_v != renderer::interface::TextureAddressMode::MirroredRepeat ||
        gltfTextureSettings.sampler.address_w != renderer::interface::TextureAddressMode::Repeat) {
        std::cerr << "Failed to load glTF texture sampler config through AssetManager.\n";
        return 1;
    }

    const auto* gltfOcclusionTexture =
        asset::AssetManager::get().getAsset<renderer::interface::Texture>(gltfOcclusionTextureHandle);
    if (gltfOcclusionTexture == nullptr || gltfOcclusionTexture->getPixels().empty() ||
        gltfOcclusionTexture->getImportSettings().color_space != renderer::interface::TextureColorSpace::Linear) {
        std::cerr << "Failed to load glTF occlusion texture config through AssetManager.\n";
        return 1;
    }

    const auto gltfModelHandle = asset::AssetManager::get().getHandleByRelativePath("GameAssets/triangle.lunamodel");
    const auto* gltfModel = asset::AssetManager::get().getAsset<renderer::interface::Model>(gltfModelHandle);
    if (gltfModel == nullptr || gltfModel->getMeshes().empty() || gltfModel->getMeshes().front().mesh != gltfHandle ||
        gltfModel->getMeshes().front().materials.empty() ||
        gltfModel->getMeshes().front().materials.front() != gltfMaterialHandle) {
        std::cerr << "Failed to load generated glTF model through AssetManager.\n";
        return 1;
    }

    const auto* defaultMaterial =
        asset::AssetManager::get().getAsset<renderer::interface::Material>(asset::builtin::defaultMaterialHandle());
    const auto* cubeMesh =
        asset::AssetManager::get().getAsset<renderer::interface::Mesh>(asset::builtin::cubeMeshHandle());
    const auto* cubeModel =
        asset::AssetManager::get().getAsset<renderer::interface::Model>(asset::builtin::cubeModelHandle());
    if (defaultMaterial == nullptr || defaultMaterial->parameters == nullptr || cubeMesh == nullptr ||
        cubeMesh->getSubMeshes().empty() || cubeModel == nullptr || cubeModel->getMeshes().empty()) {
        std::cerr << "Failed to load built-in mesh, material, and model assets.\n";
        return 1;
    }

    const auto modelHandle = asset::AssetManager::get().getHandleByRelativePath("GameAssets/cube.lunamodel");
    const auto* model = asset::AssetManager::get().getAsset<renderer::interface::Model>(modelHandle);
    if (model == nullptr || model->getMeshes().empty() || model->getMeshes().front().materials.empty()) {
        std::cerr << "Failed to load model through AssetManager.\n";
        return 1;
    }

    std::cout << "AssetManager imported and loaded mesh, material, model, and texture assets.\n";
    return 0;
}
