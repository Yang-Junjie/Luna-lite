#pragma once
#include "../../renderer/interface/mesh.h"

#include <memory>

namespace lunalite::asset {
class MeshFactory {
public:
    static std::shared_ptr<renderer::interface::Mesh> createCube();
    static std::shared_ptr<renderer::interface::Mesh> createPlane();
};
} // namespace lunalite::asset
