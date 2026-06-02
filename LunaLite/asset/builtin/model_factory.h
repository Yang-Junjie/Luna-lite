#pragma once
#include "../../renderer/interface/model.h"

#include <memory>

namespace lunalite::asset {
class ModelFactory {
public:
    static std::shared_ptr<renderer::interface::Model> createCube();
    static std::shared_ptr<renderer::interface::Model> createPlane();
};
} // namespace lunalite::asset
