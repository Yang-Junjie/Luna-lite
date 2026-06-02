#include "builtin_assets.h"
#include "model_factory.h"

#include <utility>

namespace lunalite::asset {
namespace {
std::shared_ptr<renderer::interface::Model>
    createSingleMeshModel(AssetHandle modelHandle, AssetHandle meshHandle, AssetHandle materialHandle)
{
    renderer::interface::ModelMesh modelMesh;
    modelMesh.mesh = meshHandle;
    modelMesh.materials.push_back(materialHandle);

    auto model = std::make_shared<renderer::interface::Model>();
    model->handle = modelHandle;
    model->setMeshes({std::move(modelMesh)});
    return model;
}
} // namespace

std::shared_ptr<renderer::interface::Model> ModelFactory::createCube()
{
    return createSingleMeshModel(
        builtin::cubeModelHandle(), builtin::cubeMeshHandle(), builtin::defaultMaterialHandle());
}

std::shared_ptr<renderer::interface::Model> ModelFactory::createPlane()
{
    return createSingleMeshModel(
        builtin::planeModelHandle(), builtin::planeMeshHandle(), builtin::defaultMaterialHandle());
}

} // namespace lunalite::asset
