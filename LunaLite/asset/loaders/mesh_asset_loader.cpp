#include "../../core/log.h"
#include "../../project/project_manager.h"
#include "mesh_asset_loader.h"

#include <cstdint>

#include <algorithm>
#include <cctype>
#include <glm/geometric.hpp>
#include <memory>
#include <numeric>
#include <string>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace lunalite::asset {
namespace {
struct SubMeshBuilder {
    std::string name;
    uint32_t material_slot{0};
    std::vector<renderer::interface::Vertex> vertices;
    std::vector<uint32_t> indices;
};

SubMeshBuilder& getOrCreateSubMesh(std::vector<SubMeshBuilder>& submeshes,
                                   uint32_t materialSlot,
                                   const std::string& meshName,
                                   const std::vector<tinyobj::material_t>& materials)
{
    while (submeshes.size() <= materialSlot) {
        const auto slot = static_cast<uint32_t>(submeshes.size());
        std::string name = meshName;
        if (slot < materials.size() && !materials[slot].name.empty()) {
            name += "_" + materials[slot].name;
        } else {
            name += "_SubMesh" + std::to_string(slot);
        }

        submeshes.push_back(SubMeshBuilder{
            .name = std::move(name),
            .material_slot = slot,
        });
    }

    return submeshes[materialSlot];
}

std::string normalizeExtension(std::string extension)
{
    std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

fastgltf::Expected<fastgltf::Asset> loadGltfAsset(const std::filesystem::path& path)
{
    static constexpr auto extensions = fastgltf::Extensions::KHR_materials_emissive_strength |
                                       fastgltf::Extensions::KHR_materials_unlit;
    static constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                    fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!gltfFile) {
        return gltfFile.error();
    }

    fastgltf::Parser parser(extensions);
    return parser.loadGltf(gltfFile.get(), path.parent_path(), options);
}

glm::vec3 toVec3(fastgltf::math::fvec3 value)
{
    return {value.x(), value.y(), value.z()};
}

glm::vec2 toVec2(fastgltf::math::fvec2 value)
{
    return {value.x(), value.y()};
}

void normalizeGeneratedNormals(std::vector<renderer::interface::Vertex>& vertices)
{
    for (auto& vertex : vertices) {
        const auto length = glm::length(vertex.normal);
        vertex.normal = length > 0.0f ? vertex.normal / length : glm::vec3{0.0f, 1.0f, 0.0f};
    }
}
} // namespace

std::shared_ptr<renderer::interface::Mesh> MeshAssetLoader::load(const AssetMetadata& metadata)
{
    const auto extension = normalizeExtension(metadata.FilePath.extension().string());
    if (extension == ".gltf" || extension == ".glb") {
        return loadGltf(metadata);
    }

    return loadObj(metadata);
}

std::shared_ptr<renderer::interface::Mesh> MeshAssetLoader::loadObj(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());

    const auto path = projectRoot / metadata.FilePath;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string error;

    const auto materialBasePath = (path.parent_path() / "").string();
    const bool loaded = tinyobj::LoadObj(
        &attrib, &shapes, &materials, &warn, &error, path.string().c_str(), materialBasePath.c_str(), true);

    if (!loaded) {
        LUNA_CORE_ERROR("Failed to load OBJ file '{}': {}", path.string(), error);
        return nullptr;
    }
    if (!warn.empty()) {
        LUNA_CORE_WARN("OBJ load warning for '{}': {}", path.string(), warn);
    }

    const auto meshName = metadata.Name.empty() ? path.stem().string() : metadata.Name;
    std::vector<SubMeshBuilder> submeshBuilders;

    for (const auto& shape : shapes) {
        size_t faceIndex = 0;
        for (size_t i = 0; i + 2 < shape.mesh.indices.size(); i += 3) {
            const auto& index0 = shape.mesh.indices[i + 0];
            const auto& index1 = shape.mesh.indices[i + 1];
            const auto& index2 = shape.mesh.indices[i + 2];

            auto vertex0 = MeshAssetLoader::readVertex(attrib, index0);
            auto vertex1 = MeshAssetLoader::readVertex(attrib, index1);
            auto vertex2 = MeshAssetLoader::readVertex(attrib, index2);

            const bool triangleHasNormals = MeshAssetLoader::hasNormal(attrib, index0) &&
                                            MeshAssetLoader::hasNormal(attrib, index1) &&
                                            MeshAssetLoader::hasNormal(attrib, index2);
            if (!triangleHasNormals) {
                const auto surfaceNormal = MeshAssetLoader::calculateSurfaceNormal(vertex0, vertex1, vertex2);
                vertex0.normal = surfaceNormal;
                vertex1.normal = surfaceNormal;
                vertex2.normal = surfaceNormal;
            }

            const int materialId = faceIndex < shape.mesh.material_ids.size() ? shape.mesh.material_ids[faceIndex] : -1;
            const auto materialSlot = materialId >= 0 ? static_cast<uint32_t>(materialId) : 0u;
            auto& submesh = getOrCreateSubMesh(submeshBuilders, materialSlot, meshName, materials);

            submesh.vertices.push_back(vertex0);
            submesh.indices.push_back(static_cast<uint32_t>(submesh.vertices.size() - 1));

            submesh.vertices.push_back(vertex1);
            submesh.indices.push_back(static_cast<uint32_t>(submesh.vertices.size() - 1));

            submesh.vertices.push_back(vertex2);
            submesh.indices.push_back(static_cast<uint32_t>(submesh.vertices.size() - 1));

            ++faceIndex;
        }
    }

    if (submeshBuilders.empty()) {
        LUNA_CORE_ERROR("No valid vertices found in OBJ file '{}'", path.string());
        return nullptr;
    }

    std::vector<renderer::interface::SubMesh> submeshes;
    for (auto& builder : submeshBuilders) {
        if (builder.vertices.empty()) {
            continue;
        }

        renderer::interface::SubMesh submesh;
        submesh.name = std::move(builder.name);
        submesh.material_slot = builder.material_slot;
        submesh.setVertices(std::move(builder.vertices));
        submesh.setIndices(std::move(builder.indices));
        submeshes.push_back(std::move(submesh));
    }

    if (submeshes.empty()) {
        LUNA_CORE_ERROR("No valid submeshes found in OBJ file '{}'", path.string());
        return nullptr;
    }

    auto mesh = std::make_shared<renderer::interface::Mesh>();
    mesh->handle = metadata.Handle;
    mesh->setSubMeshes(std::move(submeshes));

    return mesh;
}

std::shared_ptr<renderer::interface::Mesh> MeshAssetLoader::loadGltf(const AssetMetadata& metadata)
{
    const auto projectRoot =
        project::ProjectManager::instance().getProjectRootPath().value_or(std::filesystem::current_path());
    const auto path = projectRoot / metadata.FilePath;

    auto loadedAsset = loadGltfAsset(path);
    if (!loadedAsset) {
        LUNA_CORE_ERROR("Failed to load glTF file '{}': {}", path.string(), fastgltf::getErrorMessage(loadedAsset.error()));
        return nullptr;
    }

    auto asset = std::move(loadedAsset.get());
    const auto meshName = metadata.Name.empty() ? path.stem().string() : metadata.Name;
    std::vector<renderer::interface::SubMesh> submeshes;

    for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex) {
        const auto& gltfMesh = asset.meshes[meshIndex];
        const auto gltfMeshName = gltfMesh.name.empty() ? meshName + "_Mesh" + std::to_string(meshIndex)
                                                        : std::string{gltfMesh.name};

        for (size_t primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); ++primitiveIndex) {
            const auto& primitive = gltfMesh.primitives[primitiveIndex];
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                LUNA_CORE_WARN("Skipped non-triangle glTF primitive '{}' in '{}'", primitiveIndex, path.string());
                continue;
            }

            const auto positionAttribute = primitive.findAttribute("POSITION");
            if (positionAttribute == primitive.attributes.cend()) {
                LUNA_CORE_WARN("Skipped glTF primitive without POSITION in '{}'", path.string());
                continue;
            }

            const auto& positionAccessor = asset.accessors[positionAttribute->accessorIndex];
            std::vector<renderer::interface::Vertex> vertices(positionAccessor.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                asset, positionAccessor, [&](fastgltf::math::fvec3 position, std::size_t index) {
                    vertices[index].position = toVec3(position);
                    vertices[index].normal = glm::vec3{0.0f};
                });

            bool hasNormals = false;
            if (const auto normalAttribute = primitive.findAttribute("NORMAL");
                normalAttribute != primitive.attributes.cend()) {
                const auto& normalAccessor = asset.accessors[normalAttribute->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset, normalAccessor, [&](fastgltf::math::fvec3 normal, std::size_t index) {
                        if (index < vertices.size()) {
                            vertices[index].normal = toVec3(normal);
                        }
                    });
                hasNormals = true;
            }

            if (const auto texcoordAttribute = primitive.findAttribute("TEXCOORD_0");
                texcoordAttribute != primitive.attributes.cend()) {
                const auto& texcoordAccessor = asset.accessors[texcoordAttribute->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                    asset, texcoordAccessor, [&](fastgltf::math::fvec2 uv, std::size_t index) {
                        if (index < vertices.size()) {
                            vertices[index].uv = toVec2(uv);
                        }
                    });
            }

            std::vector<uint32_t> indices;
            if (primitive.indicesAccessor) {
                const auto& indexAccessor = asset.accessors[*primitive.indicesAccessor];
                indices.resize(indexAccessor.count);

                if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedByte ||
                    indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort) {
                    std::vector<uint16_t> shortIndices(indexAccessor.count);
                    fastgltf::copyFromAccessor<uint16_t>(asset, indexAccessor, shortIndices.data());
                    std::ranges::transform(shortIndices, indices.begin(), [](uint16_t index) {
                        return static_cast<uint32_t>(index);
                    });
                } else {
                    fastgltf::copyFromAccessor<uint32_t>(asset, indexAccessor, indices.data());
                }
            } else {
                indices.resize(vertices.size());
                std::iota(indices.begin(), indices.end(), 0u);
            }

            if (!hasNormals) {
                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    const auto i0 = indices[i + 0];
                    const auto i1 = indices[i + 1];
                    const auto i2 = indices[i + 2];
                    if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
                        continue;
                    }

                    const auto normal = calculateSurfaceNormal(vertices[i0], vertices[i1], vertices[i2]);
                    vertices[i0].normal += normal;
                    vertices[i1].normal += normal;
                    vertices[i2].normal += normal;
                }
                normalizeGeneratedNormals(vertices);
            }

            renderer::interface::SubMesh submesh;
            submesh.name = gltfMeshName + "_Primitive" + std::to_string(primitiveIndex);
            submesh.material_slot = primitive.materialIndex ? static_cast<uint32_t>(*primitive.materialIndex) : 0u;
            submesh.setVertices(std::move(vertices));
            submesh.setIndices(std::move(indices));
            submeshes.push_back(std::move(submesh));
        }
    }

    if (submeshes.empty()) {
        LUNA_CORE_ERROR("No valid submeshes found in glTF file '{}'", path.string());
        return nullptr;
    }

    auto mesh = std::make_shared<renderer::interface::Mesh>();
    mesh->handle = metadata.Handle;
    mesh->setSubMeshes(std::move(submeshes));
    return mesh;
}

glm::vec3 MeshAssetLoader::calculateSurfaceNormal(const renderer::interface::Vertex& v0,
                                                  const renderer::interface::Vertex& v1,
                                                  const renderer::interface::Vertex& v2)
{
    const auto edge0 = v1.position - v0.position;
    const auto edge1 = v2.position - v0.position;
    const auto normal = glm::cross(edge0, edge1);

    const auto length = glm::length(normal);
    if (length <= 0.0f) {
        return glm::vec3{0.0f, 1.0f, 0.0f};
    }

    return normal / length;
}

bool MeshAssetLoader::hasNormal(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index)
{
    return index.normal_index >= 0 && static_cast<size_t>(index.normal_index) * 3 + 2 < attrib.normals.size();
}

renderer::interface::Vertex MeshAssetLoader::readVertex(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index)
{
    renderer::interface::Vertex vertex;

    if (index.vertex_index >= 0) {
        const auto base = static_cast<size_t>(index.vertex_index) * 3;
        if (base + 2 >= attrib.vertices.size()) {
            LUNA_CORE_ERROR("OBJ vertex index out of range: {}", index.vertex_index);
            return vertex;
        }

        vertex.position = {
            attrib.vertices[base + 0],
            attrib.vertices[base + 1],
            attrib.vertices[base + 2],
        };

        if (!attrib.colors.empty()) {
            if (base + 2 < attrib.colors.size()) {
                vertex.color = {
                    attrib.colors[base + 0],
                    attrib.colors[base + 1],
                    attrib.colors[base + 2],
                };
            } else {
                LUNA_CORE_WARN("OBJ vertex color index out of range: {}", index.vertex_index);
            }
        }
    }

    if (MeshAssetLoader::hasNormal(attrib, index)) {
        const auto base = static_cast<size_t>(index.normal_index) * 3;
        vertex.normal = {
            attrib.normals[base + 0],
            attrib.normals[base + 1],
            attrib.normals[base + 2],
        };
    }

    if (index.texcoord_index >= 0) {
        const auto base = static_cast<size_t>(index.texcoord_index) * 2;
        if (base + 1 >= attrib.texcoords.size()) {
            LUNA_CORE_WARN("OBJ texcoord index out of range: {}", index.texcoord_index);
            return vertex;
        }

        vertex.uv = {
            attrib.texcoords[base + 0],
            attrib.texcoords[base + 1],
        };
    }

    return vertex;
}
} // namespace lunalite::asset
