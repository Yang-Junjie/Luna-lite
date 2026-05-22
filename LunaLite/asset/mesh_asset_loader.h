#pragma once
#include "../renderer/interface/mesh.h"
#include "asset.h"

#include <filesystem>
#include <tiny_obj_loader.h>

namespace lunalite::asset {
class AssetDatabase;

class MeshAssetLoader {
public:
    static AssetHandle loadObj(const std::filesystem::path& path, AssetDatabase& database);

private:
    static renderer::interface::Vertex readVertex(const tinyobj::attrib_t& attrib, const tinyobj::index_t& index);
};
} // namespace lunalite::asset
