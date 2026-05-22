#include "asset_database.h"
#include "mesh_asset_loader.h"

#include <cstdint>

#include <memory>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace lunalite::asset {

AssetHandle MeshAssetLoader::loadObj(const std::filesystem::path& path, AssetDatabase& database)
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
        for (const auto& index : shape.mesh.indices) {
            mesh->vertices.push_back(MeshAssetLoader::readVertex(attrib, index));
            mesh->indices.push_back(static_cast<uint32_t>(mesh->vertices.size() - 1));
        }
    }

    if (mesh->vertices.empty()) {
        return AssetHandle{0};
    }

    return database.add(std::move(mesh));
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

    if (index.normal_index >= 0) {
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
