#include "../LunaLite/asset/asset_database.h"
#include "../LunaLite/renderer/interface/frame_render_data.h"
#include "../LunaLite/renderer/interface/mesh.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <vector>

namespace {
bool nearlyEqual(float lhs, float rhs)
{
    return std::abs(lhs - rhs) <= 0.001f;
}

bool nearlyEqual(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return nearlyEqual(lhs.x, rhs.x) && nearlyEqual(lhs.y, rhs.y) && nearlyEqual(lhs.z, rhs.z);
}

bool expectAABB(const lunalite::renderer::interface::AABB& aabb, const glm::vec3& min, const glm::vec3& max)
{
    return aabb.valid && nearlyEqual(aabb.min, min) && nearlyEqual(aabb.max, max);
}

lunalite::renderer::interface::SubMesh makeSubMesh(std::initializer_list<glm::vec3> positions)
{
    lunalite::renderer::interface::SubMesh submesh;
    std::vector<lunalite::renderer::interface::Vertex> vertices;
    vertices.reserve(positions.size());
    for (const auto& position : positions) {
        lunalite::renderer::interface::Vertex vertex;
        vertex.position = position;
        vertices.push_back(vertex);
    }
    submesh.setVertices(std::move(vertices));
    return submesh;
}
} // namespace

int main()
{
    using namespace lunalite;

    auto mesh = std::make_shared<renderer::interface::Mesh>();
    mesh->setSubMeshes({
        makeSubMesh({{-1.0f, -2.0f, -3.0f}, {2.0f, 1.0f, 4.0f}}),
        makeSubMesh({{4.0f, -1.0f, 0.5f}, {6.0f, 3.0f, 2.0f}}),
    });

    const auto& firstSubmesh = mesh->getSubMeshes().front();
    if (!expectAABB(firstSubmesh.getLocalAABB(), {-1.0f, -2.0f, -3.0f}, {2.0f, 1.0f, 4.0f})) {
        std::cerr << "Unexpected first submesh local AABB.\n";
        return 1;
    }

    if (!expectAABB(mesh->getLocalAABB(), {-1.0f, -2.0f, -3.0f}, {6.0f, 3.0f, 4.0f})) {
        std::cerr << "Unexpected mesh local AABB.\n";
        return 1;
    }

    if (!expectAABB(mesh->getLocalAABB(1, 1), {4.0f, -1.0f, 0.5f}, {6.0f, 3.0f, 2.0f})) {
        std::cerr << "Unexpected submesh range local AABB.\n";
        return 1;
    }

    auto& editedVertices = mesh->editSubMeshes()[0].editVertices();
    editedVertices.push_back(renderer::interface::Vertex{.position = {-5.0f, 8.0f, 1.0f}});
    if (!expectAABB(mesh->getSubMeshes()[0].getLocalAABB(), {-5.0f, -2.0f, -3.0f}, {2.0f, 8.0f, 4.0f})) {
        std::cerr << "Submesh local AABB was not refreshed after editing vertices.\n";
        return 1;
    }

    const auto meshHandle = asset::AssetDatabase::get().add(mesh);
    if (meshHandle != mesh->handle) {
        std::cerr << "Asset database returned an unexpected mesh handle.\n";
        return 1;
    }

    auto transform = glm::translate(glm::mat4{1.0f}, glm::vec3{10.0f, 20.0f, 30.0f});
    transform *= glm::mat4_cast(glm::angleAxis(glm::radians(90.0f), glm::vec3{0.0f, 0.0f, 1.0f}));
    transform = glm::scale(transform, glm::vec3{2.0f, 3.0f, 4.0f});

    renderer::interface::FrameRenderData frame;
    renderer::interface::MeshDrawCommand meshCommand;
    meshCommand.mesh = meshHandle;
    meshCommand.transform = transform;
    meshCommand.local_aabb = mesh->getLocalAABB(1, 1);
    meshCommand.world_aabb = meshCommand.local_aabb.transformed(meshCommand.transform);
    meshCommand.submesh_start = 1;
    meshCommand.submesh_count = 1;
    frame.meshes.push_back(meshCommand);

    if (frame.meshes.size() != 1) {
        std::cerr << "Unexpected mesh draw command count.\n";
        return 1;
    }

    const auto& command = frame.meshes.front();
    if (!expectAABB(command.local_aabb, {4.0f, -1.0f, 0.5f}, {6.0f, 3.0f, 2.0f})) {
        std::cerr << "Unexpected draw command local AABB.\n";
        return 1;
    }

    if (!expectAABB(command.world_aabb, {1.0f, 28.0f, 32.0f}, {13.0f, 32.0f, 38.0f})) {
        std::cerr << "Unexpected draw command world AABB.\n";
        return 1;
    }

    std::cout << "Mesh AABB data is computed and propagated to draw commands.\n";
    return 0;
}
