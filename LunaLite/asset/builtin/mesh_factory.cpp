#include "../../renderer/interface/tangent_space.h"
#include "builtin_assets.h"
#include "mesh_factory.h"

namespace lunalite::asset {
namespace {
renderer::interface::Vertex makeVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv)
{
    renderer::interface::Vertex vertex;
    vertex.position = position;
    vertex.normal = normal;
    vertex.uv = uv;
    return vertex;
}
} // namespace

std::shared_ptr<renderer::interface::Mesh> MeshFactory::createCube()
{
    constexpr float h = 0.5f;

    std::vector<renderer::interface::Vertex> vertices = {
        makeVertex({-h, -h, h}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}),
        makeVertex({h, -h, h}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}),
        makeVertex({h, h, h}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}),
        makeVertex({-h, h, h}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}),
        makeVertex({h, -h, -h}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}),
        makeVertex({-h, -h, -h}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}),
        makeVertex({-h, h, -h}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}),
        makeVertex({h, h, -h}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}),
        makeVertex({-h, -h, -h}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}),
        makeVertex({-h, -h, h}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}),
        makeVertex({-h, h, h}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}),
        makeVertex({-h, h, -h}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}),
        makeVertex({h, -h, h}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}),
        makeVertex({h, -h, -h}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}),
        makeVertex({h, h, -h}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}),
        makeVertex({h, h, h}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}),
        makeVertex({-h, h, h}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}),
        makeVertex({h, h, h}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}),
        makeVertex({h, h, -h}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}),
        makeVertex({-h, h, -h}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}),
        makeVertex({-h, -h, -h}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}),
        makeVertex({h, -h, -h}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}),
        makeVertex({h, -h, h}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}),
        makeVertex({-h, -h, h}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}),
    };

    std::vector<uint32_t> indices;
    indices.reserve(36);
    for (uint32_t face = 0; face < 6; ++face) {
        const auto base = face * 4;
        indices.insert(indices.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
    }

    renderer::interface::SubMesh submesh;
    submesh.name = "Cube";
    renderer::interface::generateTangentBasis(vertices, indices);
    submesh.setVertices(std::move(vertices));
    submesh.setIndices(std::move(indices));

    auto mesh = std::make_shared<renderer::interface::Mesh>();
    mesh->handle = builtin::cubeMeshHandle();
    mesh->setSubMeshes({std::move(submesh)});
    return mesh;
}

std::shared_ptr<renderer::interface::Mesh> MeshFactory::createPlane()
{
    constexpr float h = 0.5f;
    std::vector<renderer::interface::Vertex> vertices = {
        makeVertex({-h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}),
        makeVertex({h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}),
        makeVertex({h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}),
        makeVertex({-h, 0.0f, h}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}),
    };

    renderer::interface::SubMesh submesh;
    submesh.name = "Plane";
    const std::vector<uint32_t> indices{0, 1, 2, 2, 3, 0};
    renderer::interface::generateTangentBasis(vertices, indices);
    submesh.setVertices(std::move(vertices));
    submesh.setIndices(indices);

    auto mesh = std::make_shared<renderer::interface::Mesh>();
    mesh->handle = builtin::planeMeshHandle();
    mesh->setSubMeshes({std::move(submesh)});
    return mesh;
}

} // namespace lunalite::asset
