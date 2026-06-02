#pragma once
#include "../../renderer/interface/material.h"

#include <memory>

namespace lunalite::asset {
class MaterialFactory {
public:
    static std::shared_ptr<renderer::interface::Material> createDefault();
    static std::shared_ptr<renderer::interface::Material> createError();
};
} // namespace lunalite::asset
