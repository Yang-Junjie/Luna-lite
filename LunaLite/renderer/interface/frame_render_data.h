#pragma once
#include "../../asset/asset.h"
#include "render_lighting.h"

#include <cstdint>

#include <glm/glm.hpp>
#include <limits>
#include <vector>

namespace lunalite::renderer::interface {

struct CameraData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 view_projection{1.0f};
    glm::mat4 inverse_view_projection{1.0f};
    glm::vec3 position{0.0f};
    float exposure{1.0f};
};

struct MeshDrawCommand {
    asset::AssetHandle mesh{0};
    std::vector<asset::AssetHandle> materials;
    glm::mat4 transform{1.0f};
    uint32_t submesh_start{0};
    uint32_t submesh_count{std::numeric_limits<uint32_t>::max()};
};

struct LineDrawCommand {
    glm::vec3 start{0.0f};
    glm::vec3 end{0.0f};
    glm::vec4 color{1.0f};
    bool depth_test{true};
};

struct FrameRenderData {
    CameraData camera;
    RenderLighting lighting;
    std::vector<MeshDrawCommand> meshes;
    std::vector<LineDrawCommand> debug_lines;

    void clear()
    {
        camera = CameraData{};
        lighting = RenderLighting{};
        meshes.clear();
        debug_lines.clear();
    }
};

} // namespace lunalite::renderer::interface
