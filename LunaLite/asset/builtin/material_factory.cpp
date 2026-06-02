#include "builtin_assets.h"
#include "material_factory.h"

namespace lunalite::asset {

std::shared_ptr<renderer::interface::Material> MaterialFactory::createDefault()
{
    auto material = std::make_shared<renderer::interface::Material>();
    material->handle = builtin::defaultMaterialHandle();
    material->parameters = std::make_shared<renderer::interface::MaterialParameters>();
    return material;
}

std::shared_ptr<renderer::interface::Material> MaterialFactory::createError()
{
    auto material = std::make_shared<renderer::interface::Material>();
    material->handle = builtin::errorMaterialHandle();
    material->parameters = std::make_shared<renderer::interface::MaterialParameters>();
    material->parameters->shading_model = renderer::interface::ShadingModel::Unlit;
    material->parameters->albedo = {1.0f, 0.0f, 1.0f, 1.0f};
    return material;
}

} // namespace lunalite::asset
