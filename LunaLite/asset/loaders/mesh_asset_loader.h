#pragma once
#include "../../renderer/interface/mesh.h"
#include "../asset_metadata.h"

#include <filesystem>
#include <memory>
#include <tiny_obj_loader.h>

namespace lunalite::asset {
class MeshAssetLoader {
public:
    static std::shared_ptr<renderer::interface::Mesh> load(const AssetMetadata& metadata);
    static std::shared_ptr<renderer::interface::Mesh> loadObj(const AssetMetadata& metadata);
    static std::shared_ptr<renderer::interface::Mesh> loadGltf(const AssetMetadata& metadata);

private:
    static glm::vec3 calculateSurfaceNormal(const renderer::interface::Vertex& v0,
                                            const renderer::interface::Vertex& v1,
                                            const renderer::interface::Vertex& v2);
    static bool hasNormal(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index);
    static renderer::interface::Vertex readVertex(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index);
};
} // namespace lunalite::asset
