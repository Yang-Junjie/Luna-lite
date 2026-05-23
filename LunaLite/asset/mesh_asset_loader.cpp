#include "asset_database.h"
#include "mesh_asset_loader.h"

#include <cstdint>

#include <glm/geometric.hpp>
#include <memory>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace lunalite::asset {

AssetHandle MeshAssetLoader::loadObj(const std::filesystem::path& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string error;

    const bool loaded =
        tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, path.string().c_str(), nullptr, true);

    if (!loaded) {
        return AssetHandle{0};
    }

    auto mesh = std::make_shared<renderer::interface::Mesh>();
    for (const auto& shape : shapes) {
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

            mesh->vertices.push_back(vertex0);
            mesh->indices.push_back(static_cast<uint32_t>(mesh->vertices.size() - 1));

            mesh->vertices.push_back(vertex1);
            mesh->indices.push_back(static_cast<uint32_t>(mesh->vertices.size() - 1));

            mesh->vertices.push_back(vertex2);
            mesh->indices.push_back(static_cast<uint32_t>(mesh->vertices.size() - 1));
        }
    }

    if (mesh->vertices.empty()) {
        return AssetHandle{0};
    }

    return AssetDatabase::get().add(std::move(mesh));
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
        vertex.position = {
            attrib.vertices[base + 0],
            attrib.vertices[base + 1],
            attrib.vertices[base + 2],
        };

        if (!attrib.colors.empty()) {
            vertex.color = {
                attrib.colors[base + 0],
                attrib.colors[base + 1],
                attrib.colors[base + 2],
            };
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
        vertex.uv = {
            attrib.texcoords[base + 0],
            attrib.texcoords[base + 1],
        };
    }

    return vertex;
}
} // namespace lunalite::asset
